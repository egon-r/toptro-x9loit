# MTK MI ioctl ABI — recovered from readable userspace (TOPTRO X9, MT5867, kernel 4.19.116)

Recovered by static analysis of world-readable libraries on the device plus runtime probing from
`factoryservice` (uid 3032, member of group `mi`/3010). Nothing on the device was modified.

## Where the ABI lives (and what is unreadable)

The MI user-space client is **not** in `factory_service`, `libmtktvfapi.so`, `libmtkapp.so`,
`libdtv_common.so` or `libcmpb.so` — those are RPC/IPC clients (`c_rpc_*`, `GAM_SendCmd`,
`a_mtvfapi_*`). The real client is:

```
/vendor/lib/utopia/libmi.so                      872764 bytes  world-readable  stripped
/vendor/tvservice/glibc/bin/libmi.so             identical copy
```

`libmi.so` contains every `/dev/mik/<node>` path and one exported function per MI op
(`MI_FLASH_Read`, `MI_DISP_GetAttr`, …). Disassemble with:

```
docker run --rm -u $(id -u):$(id -g) -v "$PWD/private/mi:/mi" mi-tools \
  sh -c 'arm-linux-gnueabihf-objdump -d /mi/libmi.so > /mi/libmi.dis'
```

`mi-tools` = `debian:bookworm-slim` + `binutils-arm-linux-gnueabihf` (a committed local image).
`llvm-objdump` mis-decodes this library as ARM; the code is **Thumb-2** — use GNU objdump.

Kernel modules are **not** readable, so offline module RE is unavailable:
`/basic/modules/mik.ko` (7147658 B), `misck.ko`, `mstar_fbdev_mi.ko`, `dtv_driver.ko` are all
mode `640 root:root`. `/proc/kallsyms` is world-readable but every address is `00000000` for a
non-root reader, so it yields **symbol names only** (`private/mi/kallsyms-mik.txt`, 4516 `[mik]`
symbols, incl. `_MI_DEV_FLASH_Ioctl`, `_MI_DEV_FLASH_DoIoctl`, `_MI_DEV_<NODE>_GetOp`).

## Command encoding — standard `_IOC`, type byte = MI module id

Every MI request is a Linux `_IOC` word:

```
cmd = (dir << 30) | (size << 16) | (type << 8) | nr
      dir 2 bits, size 14 bits (argument-struct bytes), type 8 bits (module), nr 8 bits (op)
```

The FLASH module is `type = 0x2D`; MTK's exported names are `MI_DEV_IOC_FLASH_*`
(visible as error strings in `libmi.so`).

## Two-layer argument ABI (critical, non-obvious)

`libmi.so` never calls `ioctl()` with the caller's struct directly. All MI ops go through
`ioctl_wrapper` at `libmi.so:0x67b8c`:

```c
long ioctl_wrapper(int fd, unsigned long cmd, void *arg)
{
    if (!arg)
        return ioctl(fd, cmd, 0);          /* syscall 54 */
    struct {            /* 16 B, built on the stack */
        unsigned short size;   /* +0  = (cmd >> 16) & 0x3FFF  */
        unsigned short pad;    /* +2                           */
        unsigned int   pad2;   /* +4                           */
        void          *buf;    /* +8  = arg                    */
        unsigned int   pad3;   /* +12 = 0                      */
    } w = { (cmd >> 16) & 0x3FFF, 0, 0, arg, 0 };
    return ioctl(fd, cmd, &w);
}
```

So the kernel's per-module `_MI_DEV_<NODE>_DoIoctl` receives a pointer to that **wrapper struct**
and takes the real argument pointer from `+8`. A raw `ioctl(fd, cmd, arg)` does not work.

`size` must equal `sizeof(argument struct)`; the kernel validates it.

## FLASH ops (`/dev/mik/flash`)

`MI_FLASH_Init` opens the node itself: `open("/dev/mik/flash", O_RDWR)` (`libmi.so:0x5e92e`).

| op | exported symbol | cmd | arg size | arg layout |
|---|---|---|---|---|
| Init | `MI_FLASH_Init` 0x5e8a4 | `0xC0102D00` | 16 | init params (zero-filled accepted) |
| Open | `MI_FLASH_Open` 0x5ead0 | `0xC08C2D02` | 140 | `+0` handle out (u32), `+4` device type |
| GetHandle | `MI_FLASH_GetHandle` 0x5ec20 | `0xC08C2D03` | 140 | same as Open |
| Close | `MI_FLASH_Close` 0x5ed78 | `0x40042D04` | 4 | `+0` handle |
| Read | `MI_FLASH_Read` 0x5f7d4 | `0xC0382D0B` | 56 | see below |
| GetAttr | `MI_FLASH_GetAttr` 0x5fbc4 | `0xC0182D0E` | 24 | handle + attr id + out |

Also present as strings (write-class, **never issued**): `MI_DEV_IOC_FLASH_ERASE`,
`_WRITE`, `_WRITE_WITHOUT_ERASE`, `_OTP_READ`, `_OTP_WRITE`, `_SET_ATTR`, `_SET_ENV_VAR`,
`_GET_ENV_VAR`, `_STORE_ENV`, `_LOAD_ENV`, `_ENTER_STR_MODE`, `_DE_INIT`. Their cmd words were
not needed and were not derived; do not guess them.

### Read argument struct (56 B), from `MI_FLASH_Read` + `libmidemo.so`

`MI_FLASH_Read(handle, pRead, pOut)` maps caller fields into the kernel struct via a helper at
`libmi.so:0x5e840`. Directly observed at kernel-visible offsets:

```
+0   u32 handle          (from MI_FLASH_Open;  fd-scoped)
+4   u32 read type       0 = flat address, 1 = sub-mode (see below), 2 = third form
+8   u32 address         (type 0: pRead[4];  type 1: pRead[8])
+12  u32 length          (type 0: pRead[12]; type 1: pRead[16])
+16..+36  zero
+40  void *buffer        (from pOut[0])
+44  u32 0
+48  u32 bytes-read out  (pOut[1])
```

For `type == 1` a sub-mode selector sits at `+24` (`pRead[4]`); sub-mode 0 is the case the demo
uses. `libmidemo.so` `MI_DEMO_FLASH_Read` (0x76678) is the reference call sequence:
`Open(param[0] = 0 or 1) -> Read -> Close`, with `ReadParam[0]` matching the Open selector, and
reading at `flash_size - 0x100000`.

## Runtime results (evidence: `private/mi/probe-log.txt`)

`/dev/mik/*` and `/dev/fusion0..7` are `660 root:mi` and **open successfully** as uid 3032
(fd = 3), so group `mi` membership is sufficient for the driver entry points.

| call | result |
|---|---|
| `open("/dev/mik/flash", O_RDONLY\|O_RDWR)` | 3 — allowed |
| `read()` / `pread64()` on `/dev/mik/flash` | `EINVAL` (22) — no buffered-read interface |
| `read()` on `/dev/fusion0` | **blocks forever** even with `O_NONBLOCK` (killed after 180 s) |
| `MI_FLASH_Init` | `2` (kernel auto-init at insmod, `mik_init.ini: MI_FLASH_INIT=1`) |
| `MI_FLASH_Open`, `param[0]=1` | **`0`, handle `0x46000001`** |
| `MI_FLASH_Open`, `param[0]=0` | `7` |
| `MI_FLASH_Open`, `param[0]=2` | `7` |
| `MI_FLASH_Open`, `param[0]=3,4,5` | `3` |
| `MI_FLASH_Close(0x46000001)` | `0`; repeat → `7` (handle gone) |
| `MI_FLASH_Read` with handle from Open | `3` (param rejected) for type 0/1/2, addrs 0 … 0x8000000 |
| `MI_FLASH_Read` **without** Open on the same fd | `7` — the MI handle is **fd-scoped** |
| `MI_FLASH_GetAttr` attrs 0…7 | `8` |

Observed error codes: `0` success, `2` already-done (Init), `7` not-open / no valid handle on this
fd, `3` invalid parameter (read/getattr), `8` invalid argument (getattr).

## Status: no dump primitive yet

Open/Close work and the handle lifecycle is understood, but **`MI_FLASH_Read` rejects every
argument shape tried**, including an exact replication of the SDK's own reference implementation
(`libmidemo.so: MI_DEMO_FLASH_Read`), for type 0 and type 1, across the address range. `GetAttr`
also fails, so the flash geometry is unknown and the read param cannot be validated against it.
The kernel module (which holds `_MI_DEV_FLASH_Ioctl` and the real param definition) and the
consumers `picus` / `dtv_svc` / `mi_daemon_sys.bin` are all root-only, and `/dev/kmsg`/`dmesg`
are denied, so the rejection reason is not observable.

Consequence for the project gate: because no read primitive exists, **nothing was dumped and no
escalation work was started** — consistent with the "dump firmware first" directive.

Next steps that would unblock it, in order of expected value:
1. Recover `MI_FLASH_ReadParam`/`OpenParam` from the MI SDK headers (they are on the vendor build
   host: path prefix `/home/BK_customer/RG_code_2/mlr61_projector/apollo/linux_core/misdk/mi/`).
2. Read `mik.ko` — it is `640 root:root` on the squashfs; the only routes are root or a raw
   read of `mmcblk0p16`, which is also `600 root:root`.
3. Drive MI from an already-logged-in consumer (`picus`) via its control interface instead of
   reimplementing the client — `picus` holds an Open handle already.
