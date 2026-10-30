# X9 attack surface (as measured on stock firmware)

Firmware observed: `Aug 5 2025_16:31:51_6.1_X9_V1.0.1`, Linux 4.19.116 armv7l.

## 1. The browser is the way in

The projector ships an embedded Chromium fork ("Sraf") that can open any URL.
Any page it displays runs with the ability to reach the device's loopback-only
factory service. There is no clickjacking, no exploit — the stock firmware
exposes a privileged RPC to whatever page the owner's browser displays.

## 2. The factory websocket RPC

`ws://127.0.0.1:7681/mtk.webui.tvCmd?name=FactoryMode` — served by a
libwebsockets daemon, loopback only, no authentication. It dispatches 571
`mtk.webui.*` JSON-RPC methods. Highlights:

| method | effect |
|---|---|
| `mtk.webui.factory.sys.executeShell {scriptPath}` | copies a script from `/mnt/usb/*` or `/tmp/mnt/usb/*` to `/tmp/factory_service/MScript.sh` and runs it as `factoryservice` (uid 3032) |
| `mtk.webui.factory.sys.setEmmcEnvVar {key,val}` / `queryEmmcEnvVar` | read/write the u-boot environment (persistent) |
| `mtk.webui.factory.sys.checkSystemFile {filePath}` | `access(R_OK)` oracle at uid 3032 |
| `mtk.webui.updater.*` | signed-package upgrade machinery (not bypassed) |

The executed shell runs under a seccomp filter with capabilities
`CAP_FOWNER|CAP_NET_ADMIN|CAP_SYS_RAWIO|CAP_SYS_ADMIN` (0x221008):
`ns_capable`-gated paths like `setsockopt(IPT_SO_SET_REPLACE)` are reachable
from it. Notably, xtables rule installation passes the capability check on
stock 4.19.116 — an unweaponized-but-reachable kernel surface.

## 3. What the foothold is not

- `executeShell` is uid 3032, **not root**: no setuid binaries exist on the
  system, the rootfs is read-only, and `/data` is root-writable only.
- The write-class RPC methods are guarded by signature checks where they
  matter (upgrade machinery); no privilege boundary was found in userspace.
- Consequence: root requires either the bootloader route (see
  `firmware-root.md`) or a kernel exploit (research).

## 4. Operational notes

- Stick paths must be under the two accepted prefixes; the device name changes
  across inserts (`sda1`/`sdb1`) — glob on the stick side.
- The factory RPC lives in one process: malformed payloads to other
  `mtk.webui.*` methods can crash the listener (a plain DoS until reboot).
- Seccomp blocks `setresuid` (SIGSYS) — stock SSH builds die at pubkey auth;
  use the patched static build in `exploit/`.
