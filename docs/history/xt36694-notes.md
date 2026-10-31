# The x_tables seqcount campaign (CVE-2020-36694) — measured negative

Research record from the attempt to turn this kernel bug into a no-hardware
root chain for the X9 (kernel 4.19.116 armv7). Preserved for the same reason
as the rest of this folder: the negatives are load-bearing.

## What was established

- **The bug is present.** Fix lineage (none of it in 4.19.116):
  `784544739a25` (RCU attempt) → `cc00bcaa5899` ("Switch synchronization to
  RCU", reverted in 5.12) → Mark Tomlinson's "Use correct memory barriers"
  series (Mar 2021). The 4.19.116 drain in `xt_replace_table` samples each
  CPU's `xt_recseq` once and skips waiting when the sample reads even.
- **The surface is reachable** from the `factoryservice` position
  (`setsockopt(IPT_SO_SET_REPLACE)` passes its capability gate — measured on
  the device).
- **Symbol extraction from your own dump** (`tools/extract-kernel-symbols.sh`):
  KL partition is a u-boot uImage wrapping an uncompressed ARM zImage with an
  LZ4 piggy; decompress → `vmlinux-to-elf` reconstructs the full kallsyms
  (86,980 symbols, base 0xC0208000, no KASLR on ARM32).

## Why there is no weapon (three measured walls)

1. **The tempting trigger cannot race.** `do_add_counters` holds the xt mutex
   across its entire walk; `__do_replace` needs the same mutex for the swap.
   The two syscalls serialize — the free can never land inside an adder's
   window. (Verified in source and by 800 fully-instrumented race groups,
   including with a 300 µs lab-widened window: zero interleavings.)
2. **The real reader is the packet path** — `ipt_do_table` in softirq (no
   mutex, only the per-CPU seqcount). Its exploitable gap is the seqcount
   store-visibility window, which user-mode emulation (qemu TCG) does not
   emulate at all: no store buffers, so the race cannot be developed or even
   hosted in a sim. It would have to be raced on real hardware.
3. **A published "proof" from this campaign was retracted.** The first
   instrumented analysis reported ~11k "UAF writes"; the detector was
   actually matching SLUB address reuse (a fresh table reallocated at the
   same chunk), not use-after-free. The corrected detector (priv_read before
   free, counter writes after free, same group) shows zero. Lesson recorded:
   in SLUB analysis, same-address ≠ same-object.

## What is ready if anyone resumes this

- The spray vehicles are proven: a failing `IPT_SO_SET_ADD_COUNTERS` against
  a bogus table name does `kvmalloc` + full `copy_from_user` of attacker
  bytes and frees on the name lookup — a fast, fully-controlled same-cache
  spray; `sendmsg` control buffers work as slower holders.
- The escalation design: reclaim the freed `xt_table_info` with planted
  `counters.pcnt` values; the stale walk resolves `pcnt + percpu_unit_base`
  with attacker-chosen `u64 +=` deltas (pcnt and bcnt both), repeatable per
  entry — an arbitrary-address 64-bit add, given a walk.
- The missing piece is a real packet-path race on hardware (loopback UDP
  flood vs replace storm; classic store-buffer widening applies). Worst case
  on-device is a watchdog reboot.
