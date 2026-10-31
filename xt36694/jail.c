#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/seccomp.h>

static void die(const char *m)
{
	fprintf(stderr, "[jail] %s: %s\n", m, strerror(errno));
	exit(1);
}

/* ---- syscall name -> nr table (guards for names some armhf glibc omit) ---- */
struct sent { const char *n; long nr; };
static struct sent sc_tab[] = {
#define S(x) { #x, SYS_##x }
#define SOPT(x) S(x),
	S(read), S(write), S(open), S(openat), S(close), S(fstat), S(fstat64),
	S(stat), S(stat64), S(lstat), S(lstat64), S(lseek),
#ifdef SYS__llseek
	SOPT(_llseek)
#endif
	S(mmap2), S(munmap), S(mprotect), S(madvise), S(mremap), S(brk),
	S(vmsplice), S(pipe), S(pipe2), S(dup), S(dup2), S(dup3),
	S(io_setup), S(io_submit), S(io_getevents), S(io_destroy),
	S(pidfd_open), S(memfd_create), S(shmat), S(shmdt),
	S(poll), S(ppoll),
#ifdef SYS_select
	SOPT(select)
#endif
	S(pselect6),
	S(epoll_create1), S(epoll_ctl), S(epoll_wait),
	S(timerfd_create), S(timerfd_settime), S(timerfd_gettime),
	S(inotify_init1), S(inotify_add_watch), S(inotify_rm_watch),
	S(socket), S(bind), S(connect), S(listen), S(accept), S(accept4),
	S(sendto), S(recvfrom), S(sendmsg), S(recvmsg),
#ifdef SYS_recvmmsg
	SOPT(recvmmsg)
#endif
#ifdef SYS_sendmmsg
	SOPT(sendmmsg)
#endif
	S(shutdown), S(setsockopt), S(getsockopt), S(getsockname), S(getpeername),
	S(ioctl), S(sched_setaffinity), S(sched_getaffinity), S(sched_yield),
	S(nanosleep), S(clock_nanosleep), S(clock_gettime), S(gettimeofday),
	S(futex), S(clone),
#ifdef SYS_fork
	SOPT(fork)
#endif
#ifdef SYS_vfork
	SOPT(vfork)
#endif
	S(execve), S(exit), S(exit_group),
	S(wait4), S(waitid), S(kill), S(tgkill),
	S(rt_sigaction), S(rt_sigprocmask), S(rt_sigreturn),
#ifdef SYS_sigaltstack
	SOPT(sigaltstack)
#endif
	S(getuid), S(geteuid), S(getgid), S(getegid), S(getpid), S(gettid),
	S(getppid), S(set_tid_address), S(set_robust_list), S(get_robust_list),
	S(prctl), S(restart_syscall), S(readv), S(writev),
	S(pread64), S(pwrite64), S(ftruncate), S(fsync), S(fdatasync),
	S(getdents64), S(mkdir), S(unlink), S(rename), S(chmod), S(umask),
#undef SOPT
#undef S
};
static long sc_nr(const char *n)
{
	for (unsigned i = 0; i < sizeof(sc_tab) / sizeof(sc_tab[0]); i++)
		if (!strcmp(sc_tab[i].n, n))
			return sc_tab[i].nr;
	return -1;
}

static void install_seccomp(const char *path)
{
	long nrs[255];
	unsigned n = 0;
	FILE *f = fopen(path, "r");
	char line[128];
	if (!f)
		die("open seccomp profile");
	while (fgets(line, sizeof(line), f)) {
		char *p = line, *e;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '#' || *p == '\n' || !*p)
			continue;
		for (e = p; *e && *e != '\n' && *e != ' ' && *e != '\t' && *e != '#'; e++);
		*e = 0;
		long nr = sc_nr(p);
		if (nr < 0) {
			fprintf(stderr, "[jail] seccomp: unknown syscall '%s' — refusing\n", p);
			exit(1);
		}
		if (n >= 255) {
			fprintf(stderr, "[jail] seccomp: >255 entries unsupported\n");
			exit(1);
		}
		nrs[n++] = nr;
	}
	fclose(f);

	struct sock_filter ins[4 + 255 + 2];
	unsigned i = 0;
	ins[i++] = (struct sock_filter)BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
			offsetof(struct seccomp_data, arch));
	ins[i++] = (struct sock_filter)BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
			AUDIT_ARCH_ARM, 1, 0);
	ins[i++] = (struct sock_filter)BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL);
	ins[i++] = (struct sock_filter)BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
			offsetof(struct seccomp_data, nr));
	for (unsigned k = 0; k < n; k++) {
		/* hit -> jump forward past remaining rules + the RET_KILL */
		ins[i++] = (struct sock_filter)BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,
				nrs[k], n - k, 0);
	}
	ins[i++] = (struct sock_filter)BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL);
	ins[i++] = (struct sock_filter)BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
	struct sock_fprog prog = { .len = i, .filter = ins };

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
		die("PR_SET_NO_NEW_PRIVS");
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog))
		die("PR_SET_SECCOMP");
	fprintf(stderr, "[jail] seccomp: %u syscalls allowed, default KILL\n", n);
}

static void print_status(void)
{
	char buf[4096];
	int fd = open("/proc/self/status", O_RDONLY);
	if (fd < 0)
		return;
	ssize_t r = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (r <= 0)
		return;
	buf[r] = 0;
	for (char *p = strtok(buf, "\n"); p; p = strtok(NULL, "\n"))
		if (!strncmp(p, "Uid:", 4) || !strncmp(p, "Gid:", 4) ||
		    !strncmp(p, "Cap", 3) || !strncmp(p, "NoNewPrivs", 10) ||
		    !strncmp(p, "Seccomp", 7))
			fprintf(stderr, "[jail] %s\n", p);
}

int main(int argc, char **argv)
{
	long uid = 1000;
	uint64_t caps = 0;
	const char *seccomp = NULL;
	int i = 1;

	for (; i < argc; i++) {
		if (!strcmp(argv[i], "--uid") && i + 1 < argc) {
			uid = strtol(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "--caps") && i + 1 < argc) {
			caps = strtoull(argv[++i], NULL, 0);
		} else if (!strcmp(argv[i], "--seccomp") && i + 1 < argc) {
			seccomp = argv[++i];
		} else if (!strcmp(argv[i], "--") && i + 1 < argc) {
			i++;
			break;
		} else {
			fprintf(stderr, "usage: jail --uid N --caps 0xHEX [--seccomp FILE] -- cmd [args]\n");
			return 2;
		}
	}
	if (i >= argc) {
		fprintf(stderr, "usage: jail --uid N --caps 0xHEX [--seccomp FILE] -- cmd [args]\n");
		return 2;
	}

	if (getuid() == 0) {
		if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0))
			die("PR_SET_KEEPCAPS");
		for (int c = 0; c <= CAP_LAST_CAP; c++)
			if (!(caps >> c & 1))
				if (prctl(PR_CAPBSET_DROP, c, 0, 0, 0) && errno != EINVAL)
					die("PR_CAPBSET_DROP");
		if (syscall(SYS_setgroups, 0, NULL) && errno != EPERM)
			die("setgroups");
		if (setgid(uid) || setuid(uid))
			die("setuid/setgid");
	}
	{
		struct __user_cap_header_struct hdr = {
			.version = _LINUX_CAPABILITY_VERSION_3, .pid = 0 };
		struct __user_cap_data_struct data[2] = {{0}};
		data[0].permitted = data[0].effective = data[0].inheritable = caps & 0xFFFFFFFF;
		data[1].permitted = data[1].effective = data[1].inheritable = caps >> 32;
		if (syscall(SYS_capset, &hdr, data))
			die("capset");
		/* keep the mask EFFECTIVE across execve: ambient caps re-enter
		 * pE after exec for non-root binaries without file caps */
		for (int c = 0; c <= CAP_LAST_CAP; c++)
			if (caps >> c & 1)
				if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, c, 0, 0))
					fprintf(stderr, "[jail] ambient raise cap %d: %s\n",
					        c, strerror(errno));
	}
	if (seccomp)
		install_seccomp(seccomp);
	print_status();
	execvp(argv[i], &argv[i]);
	perror("[jail] execvp");
	return 127;
}
