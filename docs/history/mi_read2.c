/* mi_read2 - read flash through the MI driver from an unprivileged uid (3032).
 *
 * Calling convention recovered from libmi.so:MI_FLASH_Read (0x5f7d4) and the SDK
 * reference libmidemo.so:MI_DEMO_FLASH_Read (0x76678):
 *
 *   MI_FLASH_Read(u32 handle, void *param, void *bufdesc)
 *     param  (68 B): [0]=iface (1 = eMMC), [8]=offset, [16]=length
 *     bufdesc (8 B): { void *buf; u32 out_len; }
 *   ioctl cmd 0xC0382D0B on the handle's fd.
 *
 * usage:
 *   mi_read2 probe                      read 64 bytes at offset 0, hexdump
 *   mi_read2 <offset> <len> <outfile>   chunked read to a file
 *   mi_read2 <offset> <len> <outfile> <chunk>
 */
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef int (*fn_init)(void *);
typedef int (*fn_open)(void *, void *);
typedef int (*fn_read)(unsigned, void *, void *);
typedef int (*fn_close)(unsigned);

static fn_init  p_init;
static fn_open  p_open;
static fn_read  p_read;
static fn_close p_close;

static int mi_setup(unsigned *handle)
{
    const char *cands[] = {
        "/vendor/lib/utopia/libmi.so",
        "libmi.so",
        "/basic/lib/libmi.so",
        NULL
    };
    void *h = NULL;
    for (int i = 0; cands[i] && !h; i++)
        h = dlopen(cands[i], RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen failed: %s\n", dlerror()); return -1; }

    p_init  = (fn_init)dlsym(h, "MI_FLASH_Init");
    p_open  = (fn_open)dlsym(h, "MI_FLASH_Open");
    p_read  = (fn_read)dlsym(h, "MI_FLASH_Read");
    p_close = (fn_close)dlsym(h, "MI_FLASH_Close");
    if (!p_init || !p_open || !p_read) { fprintf(stderr, "dlsym failed\n"); return -1; }

    unsigned char openp[136];
    memset(openp, 0, sizeof openp);
    openp[0] = (unsigned char)(getenv("X9_OPEN0") ? atoi(getenv("X9_OPEN0")) : 1);
    p_init(0);
    if (p_open(openp, handle)) { fprintf(stderr, "MI_FLASH_Open failed\n"); return -1; }
    return 0;
}

static int g_iface = 1;

static int mi_read_chunk(unsigned handle, unsigned long off, void *buf, unsigned len,
                         unsigned *got)
{
    unsigned char param[68];
    struct { void *buf; unsigned out_len; } bd;
    memset(param, 0, sizeof param);
    param[0] = (unsigned char)g_iface;    /* iface: 0/1/2 per the helper's switch */
    *(unsigned *)(param + 8)  = (unsigned)off;
    *(unsigned *)(param + 16) = len;
    bd.buf = buf;
    bd.out_len = len;
    int r = p_read(handle, param, &bd);
    if (got) *got = bd.out_len;
    return r;
}

static void hexdump(const unsigned char *p, unsigned n)
{
    for (unsigned i = 0; i < n; i += 16) {
        printf("%08x  ", i);
        for (unsigned j = 0; j < 16; j++)
            printf(j < n - i ? "%02x " : "   ", p[i + j]);
        printf(" |");
        for (unsigned j = 0; j < 16 && i + j < n; j++) {
            unsigned char c = p[i + j];
            putchar(c >= 32 && c < 127 ? c : '.');
        }
        printf("|\n");
    }
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    unsigned handle = 0;
    if (mi_setup(&handle)) return 1;

    if (getenv("X9_IFACE")) g_iface = atoi(getenv("X9_IFACE"));

    if (argc < 2 || !strcmp(argv[1], "probe")) {
        unsigned char buf[64];
        unsigned got = 0;
        memset(buf, 0xaa, sizeof buf);
        int r = mi_read_chunk(handle, 0, buf, sizeof buf, &got);
        printf("MI_FLASH_Read(off=0,len=64) rc=%d got=%u\n", r, got);
        hexdump(buf, sizeof buf);
        if (p_close) p_close(handle);
        return r ? 2 : 0;
    }

    if (argc < 4) { fprintf(stderr, "usage: %s <offset> <len> <outfile> [chunk]\n", argv[0]); return 1; }
    unsigned long off = strtoul(argv[1], NULL, 0);
    unsigned long len = strtoul(argv[2], NULL, 0);
    unsigned chunk = argc > 4 ? (unsigned)strtoul(argv[4], NULL, 0) : 65536;
    FILE *out = fopen(argv[3], "wb");
    if (!out) { perror("fopen"); return 1; }

    unsigned char *buf = malloc(chunk);
    if (!buf) { perror("malloc"); return 1; }
    unsigned long done = 0, errors = 0;
    while (done < len) {
        unsigned want = (unsigned)((len - done) < chunk ? (len - done) : chunk);
        unsigned got = 0;
        int r = mi_read_chunk(handle, off + done, buf, want, &got);
        if (r) { fprintf(stderr, "read off=0x%lx rc=%d\n", off + done, r); errors++; if (errors > 3) break; }
        size_t n = got ? got : want;          /* driver may not report length */
        fwrite(buf, 1, n, out);
        done += n;
        if ((done % (4u << 20)) == 0)
            fprintf(stderr, "  %lu / %lu\n", done, len);
    }
    fclose(out);
    free(buf);
    if (p_close) p_close(handle);
    printf("done: %lu bytes, %lu read errors\n", done, errors);
    return errors ? 3 : 0;
}
