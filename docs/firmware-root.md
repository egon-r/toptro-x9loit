# Routes to root on the X9 (state of the art)

The foothold (see `attack-surface.md`) is uid 3032. Everything beyond that
measured so far:

## Route A — bootloader (works, needs UART)

The device's u-boot ("mboot") console is on the SoC UART at 115200 8N1. The
console is only live when the env var `UARTOnOff=on` is set — writable
through the factory RPC (`setUartEnvironment isOn:1` or `setEmmcEnvVar`), so
no hardware is needed to *enable* it; hardware is needed to *use* it (TX/RX
pads inside the case, 3.3 V TTL; a ~$5 USB-UART adapter or an ESP32).

With console access, `init=/bin/sh` in bootargs gives a root shell on ttyS0
(one-time, no `saveenv`). The verified-persistent variant keeps the rootfs
byte-exact stock and plants a launcher in the writable `/data` partition that
the stock `autorun.sh` executes at boot (the vendor's own smcast hook) —
normal boots, verified dm-verity, root SSH after the remote's POWER press.

Recovery is always available: the mboot prompt can reflash any partition from
a FAT stick (`bin2emmc`), and a full eMMC dump fits on one stick.

## Route B — kernel exploit (no hardware, research)

Kernel 4.19.116 with: PREEMPT, SMP×4, SLUB (no freelist hardening), no KASLR
(ARM32), CONFIG_CPU_SW_DOMAIN_PAN (ret2usr dead), HARDENED_USERCOPY,
REFCOUNT_FULL. From the uid-3032 position: xtables is reachable
(`IPT_SO_SET_REPLACE` passes its capability gate); nearly everything else
(netfilter families, BPF, keyctl, userfaultfd) is compiled out or seccomp-
killed. CVE-2020-36694 (x_tables per-CPU seqcount UAF on concurrent table
replace) is present and reachable with no public weapon. If that lands, the
entire root chain becomes stick + one page load, no disassembly.

## Route C — upgrade machinery (not bypassed)

`mtk.webui.updater.*` fetches/installs vendor-signed packages. Signature
verification has not been audited end-to-end; no bypass is claimed.

## What does NOT work

- Modified rootfs images: mboot's AVB2.0 dm-verity reset-loops them on cold
  autoboot (and `avbab disable-verity` refuses while the device state is
  locked).
- `bootcmd`/`preboot` env hooks: the platform pins `bootcmd` and never
  executes the env copy — measured, not assumed.
- `init=/bin/sh` *without* UART: the shell's stdio is ttyS0 with nothing
  attached; from the RPC alone it just watchdog-loops.
