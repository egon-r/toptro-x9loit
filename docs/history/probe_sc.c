/* Map the seccomp sandbox: each syscall in its own fork; result written unbuffered. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>

static void emit(const char *tag, long r, int e)
{
    char buf[160];
    int n = snprintf(buf, sizeof buf, "%-22s ret=%ld errno=%d(%s)\n", tag, r, r < 0 ? e : 0,
                     r < 0 ? strerror(e) : "ok");
    write(1, buf, n);
}
static void test(const char *tag, long (*fn)(void))
{
    pid_t p = fork();
    if (p == 0) { long r = fn(); emit(tag, r, errno); _exit(0); }
    int st = 0; waitpid(p, &st, 0);
    if (WIFSIGNALED(st)) {
        char buf[96];
        int n = snprintf(buf, sizeof buf, "%-22s KILLED by signal %d\n", tag, WTERMSIG(st));
        write(1, buf, n);
    }
}
static long t_l2cap(void)   { return socket(31, 3 /*SEQPACKET*/, 0); }
static long t_packet(void)  { return socket(17 /*AF_PACKET*/, 3, 0); }
static long t_raw4(void)    { return socket(2, 3 /*SOCK_RAW*/, 1); }
static long t_raw6(void)    { return socket(10, 3, 58); }
static long t_netlink(void) { return socket(16 /*AF_NETLINK*/, 3, 0); }
static long t_unshare(void) { return unshare(0x40000000); }
static long t_tun(void)     { return open("/dev/net/tun", O_RDWR); }
static long t_perf(void)
{
    struct { unsigned type, size; unsigned long long cfg, s1, s2; unsigned f1, f2, f3; }
        __attribute__((packed)) attr;
    memset(&attr, 0, sizeof attr);
    attr.type = 1; attr.size = sizeof attr;
    return syscall(364, &attr, 0, -1, -1, 0);
}
static long t_mapcreate(void)
{
    union { char b[64]; unsigned u[8]; } attr;
    memset(&attr, 0, sizeof attr);
    attr.u[0] = 1;      /* BPF_MAP_TYPE_HASH */
    attr.u[1] = 4;      /* key size */
    attr.u[2] = 4;      /* value size */
    attr.u[3] = 16;     /* max entries */
    return syscall(280 /*__NR_bpf on ARM*/, 0 /*BPF_MAP_CREATE*/, &attr, sizeof attr);
}
int main(void)
{
    test("socket L2CAP", t_l2cap);
    test("socket PACKET", t_packet);
    test("socket RAW ipv4", t_raw4);
    test("socket RAW ipv6", t_raw6);
    test("socket NETLINK", t_netlink);
    test("unshare(NEWNET)", t_unshare);
    test("open /dev/net/tun", t_tun);
    test("perf_event_open", t_perf);
    test("bpf map_create", t_mapcreate);
    return 0;
}
