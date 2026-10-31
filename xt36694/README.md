# xt36694 — the x_tables weapon (CVE-2020-36694, research state)

The main research output of this project: a parametrized weapon for the
4.19.116 x_tables per-CPU seqcount bug on the TOPTRO X9 — race trigger,
UAF-reclaim spray, and arbitrary-increment aiming, plus the device-faithful
jail and surface probes. Read `docs/history/xt36694-notes.md` first: the
current status is **reachable surface, no working trigger yet** — the
add-counters trigger was falsified (xt-mutex serialization) and the real
race needs packet-path timing on physical hardware. The weapon is published
exactly because it is the scaffolding for whoever races it for real.

## Components

| file | what it is |
|---|---|
| `xt36694.c` | the weapon: adder/replacer race threads, sendmsg + xt-path reclaim sprays, fake `xt_table_info` payload with planted `counters.pcnt` (calibrate/aim modes), counter-anomaly watcher |
| `ipt_blob.h` | generic iptables blob builder (SET_REPLACE/GET_ENTRIES/ADD_COUNTERS) + the UAF fake-payload builder; works from any AF_INET socket — no raw socket needed |
| `detect_xt_race.py` | console-log detector (v2): true-UAF interleaving = priv_read BEFORE free, counter writes AFTER free, same group; flags address-reuse false positives separately |
| `jail.c` | LD_PRELOAD-free launcher that recreates the exact device position in a lab: uid 3032 + CapEff 0x221008 (ambient caps survive exec) + optional seccomp allowlist |
| `xtinfo.c` | surface probe: table inventory, entry dumps, SET_REPLACE roundtrip, raw-percpu-pointer leak attempt |
| `seccomp-profile.device.txt` | measured device allowlist for jail (incomplete by construction — the device filter is default-deny) |
| `Makefile` | static armhf builds (native cross-gcc or docker fallback) |

## Build

```sh
make            # static armhf binaries in bin/ (docker cross toolchain)
```

## Run on your own unit (the real target)

From the `factoryservice` shell (see the repo README for the foothold):

```sh
./bin/xtinfo --info --replace          # verify the surface where you stand
./bin/xt36694 --table raw --rounds 5000 --sprayers 8
```

The honest expectation today: replaces and adds succeed (surface proven),
sprays allocate controlled chunks — but the race will not fire through this
trigger (mutex; see the history note). The next author adds a packet-path
reader: loopback UDP flood through a filter rule while the replacer runs,
on real hardware, where the seqcount store-visibility window exists.

## Escalation design (validated pieces, ready for a real walk)

Calibrate (planted pcnt=0 resolves to the percpu unit base), then aim:
planted `pcnt = target − unit_base` turns the stale walk into
`*(u64 *)target += chosen` per entry (bcnt delta 0 keeps the +8 slot clean).
With `modprobe_path` from your own dump's symbols (`tools/extract-kernel-symbols.sh`)
and a `TCP_CONGESTION`-setsockopt `request_module` trigger (reachable at
this privilege level), a single walk that lands is the whole chain.
