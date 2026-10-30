/* Measure how long a synchronous unix_gc() takes, as a function of the listener's queue
 * depth. close() of any socket runs unix_gc() on our thread, so clock_gettime around that
 * close() times the whole collection -- which is the window the race has to hit.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

static void out(const char *fmt, ...)
{
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n > 0) { ssize_t w = write(1, b, (size_t)n); (void)w; }
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
    c->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(c), &fd, sizeof fd);
    return sendmsg(sock, &msg, MSG_NOSIGNAL);
}

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

int main(void)
{
    static const int depths[] = { 0, 16, 64, 256, 1024 };
    out("== unix_gc() duration vs listener queue depth ==\n");

    for (unsigned t = 0; t < sizeof depths / sizeof depths[0]; t++) {
        int P = depths[t];
        char name[32];
        snprintf(name, sizeof name, "x9tm%u", t);
        struct sockaddr_un sa;
        socklen_t salen;
        memset(&sa, 0, sizeof sa);
        sa.sun_family = AF_UNIX;
        memcpy(sa.sun_path + 1, name, strlen(name));
        salen = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(name);

        int L = socket(AF_UNIX, SOCK_STREAM, 0);
        if (L < 0 || bind(L, (struct sockaddr *)&sa, salen) < 0 || listen(L, 8192) < 0) {
            out("setup failed errno=%d\n", errno);
            return 1;
        }
        int H[2];
        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, H) < 0) return 1;
        if (send_fd(H[0], L) < 0) return 1;
        close(H[0]);
        close(L);

        int *pre = calloc(P > 0 ? P : 1, sizeof(int));
        int npre = 0;
        for (int k = 0; k < P; k++) {
            int c = socket(AF_UNIX, SOCK_STREAM, 0);
            if (c < 0) break;
            if (connect(c, (struct sockaddr *)&sa, salen) != 0) { close(c); break; }
            pre[npre++] = c;
        }

        /* time a close() that must run a full GC (unix_tot_inflight > 0) */
        double best = 1e9, sum = 0;
        int reps = 30;
        for (int r = 0; r < reps; r++) {
            double a = now_us();
            int d = socket(AF_UNIX, SOCK_DGRAM, 0);
            if (d >= 0) close(d);
            double dur = now_us() - a;
            if (dur < best) best = dur;
            sum += dur;
            /* L is collected by the first GC; re-arm it for the next rep */
            if (r + 1 < reps) {
                int L2 = socket(AF_UNIX, SOCK_STREAM, 0);
                int H2[2];
                if (L2 < 0 || bind(L2, (struct sockaddr *)&sa, salen) < 0) break;
                listen(L2, 8192);
                if (socketpair(AF_UNIX, SOCK_DGRAM, 0, H2) < 0) break;
                if (send_fd(H2[0], L2) < 0) break;
                close(H2[0]);
                close(L2);
                close(H2[1]);
            }
        }
        out("queue=%-5d  close()+GC best=%.1fus mean=%.1fus\n", npre, best, sum / reps);
        for (int k = 0; k < npre; k++) close(pre[k]);
        close(H[1]);
        free(pre);
    }
    out("== done ==\n");
    return 0;
}
