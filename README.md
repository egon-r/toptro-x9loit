# toptro-x9loit

Tools for owners of **TOPTRO X9 projectors** who want to understand and take
control of their own hardware: a documented attack surface, a USB-stick shell
launcher, and a browser-based page that drives the device's factory RPC from
its own web browser.

Everything here is for **devices you own**. No vendor firmware images, no
leaked credentials, no factory-pirated binaries — only original tooling and
documentation of what the stock firmware exposes.

## What the firmware exposes (short version)

The X9 runs a Linux 4.19 armv7 system with an embedded Chromium-based browser
("Sraf"). Out of the box:

- A **factory websocket RPC** on `ws://127.0.0.1:7681` (loopback only) with
  571 methods, including `mtk.webui.factory.sys.executeShell`, which copies a
  script from a USB stick and executes it as the `factoryservice` user.
- The browser can open arbitrary pages, so any page loaded on the projector
  can talk to that loopback RPC (see `page/`).
- The u-boot environment is writable through the same RPC (`setEmmcEnvVar`).

Result: **unprivileged code execution from a USB stick + one browser page
load**, no disassembly. That is enough to explore the system, run a debug
shell, and — with the bootloader route documented in `docs/` — full root
(with the caveat that the reliable root path currently needs a UART connection
to the bootloader; see `docs/firmware-root.md`).

## Repository layout

| path | what it is |
|---|---|
| `docs/`  | GitHub Pages site (exploit page): open it in the projector's browser, it drives the factory RPC (stick `executeShell`, USB-path probing, status display) |
| `exploit/` | USB stick layout + `MScript.sh` launcher template and the static dropbear build recipe |
| `tools/` | host-side helpers: bridge/shell page host, reverse-shell listener, RPC client, page client |
| `docs/` | the attack-surface writeup and the firmware/root routes |

## Quickstart (factory device, no disassembly)

1. Build the static dropbear (once, in docker):
   `cd exploit && ./build-dropbear.sh`
2. Generate your own SSH keypair (`ssh-keygen`) — never use shared keys.
3. Lay out a FAT32 USB stick per `exploit/README.md` with your public key.
4. Start the bridge on a machine on the same LAN:
   `python3 tools/x9-cdp-shell.py`
5. On the projector, open the browser and visit the GitHub Pages URL (or your
   bridge directly). The page triggers `executeShell` for your stick's
   `MScript.sh`.
6. Connect: `ssh -p 2222 factoryservice@<projector-ip>` with your key.

## Legal & scope

These tools touch only your own device's documented interfaces. They do not
bypass DRM, do not extract vendor keys, and do not enable anything on units
you do not own. Vendor-copyrighted binaries are intentionally absent.

## Status

- Foothold (factoryservice shell from stick + page): working, documented here.
- Root: possible via the mboot/UART route — see `docs/firmware-root.md` for
  the state of the art and hardware needed.
- A no-hardware root path is research-in-progress (kernel 4.19.116 has
  reachable-but-unweaponized x_tables surface); not published yet because it
  does not exist yet.
