# Research history — the roads not taken

Artifacts from the original research campaign that produced this repo's
working chain. They are preserved because their **negatives are load-bearing**:
each one closed a route measurably, which is what forced the search toward the
paths that actually work (the factory RPC, and the boot-console root route in
`uart-root.md`). If you re-research this platform, start here to avoid
re-walking the dead ends.

Two further files from the same era already live elsewhere in the repo:
`tools/factory-rpc.py` (the RPC client) and `esp32/x9-uart/src/main.cpp`
(the console firmware).

## The MI flash-read campaign (firmware extraction via the MI driver)

- `ioctl-callsites.md` — full static+runtime recovery of the MTK MI driver
  ioctl ABI (`/dev/mik/*`, FLASH module type 0x2D, the 16-byte
  `ioctl_wrapper` framing) from `libmi.so` and `libmidemo.so`.
- `mi_read.c`, `mi_read2.c` — the read attempts against `MI_FLASH_Read`:
  every argument shape returns `-EINVAL` (rc 3). **Negative: there is no
  flash-read primitive at uid 3032**; the ioctl ABI was right, the driver
  refuses on state/policy.
- `kallsyms-mik.txt` — symbol/address map pulled from the device during that
  campaign (research reference; addresses are unit/firmware-version
  specific).
- `MMAP_MI.h` — the vendor memory-map header found world-readable on the
  unit (`/application/MMAP_MI.h`); the MI register layout reference used for
  the ABI work.

## The af_unix GC race (CVE-2024-26923)

- `gc_race11.c` — the K-parallel-victim trigger for the garbage-collector
  race (in-flight listener vs `unix_gc`), the final iteration of the
  campaign: preconditions validated (in-flight listener stays connectable),
  race structurally corrected (V shipped from the victim's own socket,
  listener last in the candidate list), thousands of attempts on-device.
- `gc_timing.c` — the timing calibration harness (`close()`+`unix_gc()`
  vs listener queue depth).

**Negative (why this is history):** the interleaving was eventually produced
on demand in an instrumented kernel — and the result is a **hard local DoS,
not a primitive**, on this ARM kernel: the desync trips a `BUG_ON` inside
`unix_gc_lock`, wedging the whole AF_UNIX subsystem (every socket on the box
stops working; no software reboot). Root escalation was abandoned for this
route.

## The seccomp map

- `probe_sc.c` — fork-isolated syscall probe that produced the seccomp
  allow/deny map of the `factoryservice` sandbox (what is SIGSYS-killed,
  what is EPERM'd, what passes). Still the reference for "what can a
  stick-launched process do" — and the reason the working foothold avoids
  every filtered surface.
