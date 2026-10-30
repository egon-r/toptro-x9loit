/* MI_FLASH_Read retry with a correctly sized parameter struct.
 *
 * The SDK demo fills a 28-byte read param and the driver's ioctl is 56 bytes
 * (0xC0382D0B), so a 28-byte struct makes libmi copy 56 bytes from a 28-byte
 * stack buffer -> the driver sees garbage fields and answers "invalid param" (3).
 * This probe uses a full 56-byte param (and a 64-byte one) to test that theory.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

typedef int MI_S32;
extern MI_S32 MI_FLASH_Init(void *p);
extern MI_S32 MI_FLASH_Open(void *pOpenParam, void *pHandle);
extern MI_S32 MI_FLASH_Read(unsigned int h, void *pReadParam, void *pOut);
extern MI_S32 MI_FLASH_Close(unsigned int h);
extern MI_S32 MI_FLASH_GetAttr(unsigned int h, int attr, unsigned int reserved, void *pOut);

static void hexdump(const char *tag, const unsigned char *b, unsigned n)
{
    printf("  %s:", tag);
    for (unsigned i = 0; i < n && i < 16; i++) printf(" %02x", b[i]);
    printf("\n");
}

int main(void)
{
    unsigned char openp[136];
    unsigned int handle = 0, attr = 0;
    static unsigned char buf[4096];

    memset(openp, 0, sizeof openp);
    openp[0] = 1;
    printf("init=%d\n", MI_FLASH_Init(0));
    MI_S32 r = MI_FLASH_Open(openp, &handle);
    printf("open=%d handle=0x%x\n", r, handle);
    if (r) return 1;

    memset(&attr, 0, sizeof attr);
    printf("getattr(2)=%d val=0x%x\n", MI_FLASH_GetAttr(handle, 2, 0, &attr), attr);

    /* out struct: [0]=buffer pointer, [1]=0 (as the SDK demo builds it) */
    unsigned int out[4];

    for (int variant = 0; variant < 2; variant++) {
        size_t psz = variant ? 64 : 56;
        unsigned char rp[64];
        for (int type = 0; type <= 2; type++) {
            for (int ai = 0; ai < 3; ai++) {
                unsigned addrs[3] = { 0u, 0x100000u, 0x200000u };
                memset(rp, 0, sizeof rp);
                *(uint32_t *)(rp + 0) = (uint32_t)type;   /* selector */
                *(uint32_t *)(rp + 8) = addrs[ai];        /* address  */
                *(uint32_t *)(rp + 16) = 64;              /* length   */
                memset(buf, 0xcc, sizeof buf);
                memset(out, 0, sizeof out);
                out[0] = (unsigned int)(uintptr_t)buf;
                out[1] = 0;

                MI_S32 rr = MI_FLASH_Read(handle, rp, out);
                printf("psz=%zu type=%d addr=0x%x -> rc=%d outlen=%u\n",
                       psz, type, addrs[ai], rr, out[1]);
                if (rr == 0) {
                    hexdump("data", buf, 16);
                }
            }
        }
    }

    printf("close=%d\n", MI_FLASH_Close(handle));
    return 0;
}
