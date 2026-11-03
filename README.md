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
  can talk to that loopback RPC (see `docs/`).
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
| `exploit/` | USB stick layout + `MScript.sh` launcher template, static dropbear build recipe, and `root-payload/` (installer + boot hook) |
| `tools/` | host helpers: page bridge, RPC client, mboot orchestrator/command/dump tools, serial logger |
| `esp32/` | ESP32-S3 UART bridge firmware for the projector console |
| `xt36694/` | **the weapon** — CVE-2020-36694 race trigger, UAF reclaim spray, arbitrary-increment aiming, device-faithful jail, surface probes, detector (research state; see its README) |
| `docs/history/` | preserved research artifacts: the measured dead ends (MI flash-read, af_unix GC race, seccomp map) that shaped the working chain |
| `docs/*.md` | the attack-surface writeup and the firmware/root routes |
| `sdk/` | **web-app SDK** — develop for the projector's stock Sraf browser entirely off-device: ES5 browser library, a Hello World sample, and a local preview running the *original* stock launcher (simulated firmware services) with a dev tile that launches your app |


## Developing apps for the projector (`sdk/`)

The stock launcher and browser UI are web-based (Sraf Chromium), so apps can be
built as ordinary web pages. `sdk/` gives you the pieces to do that without a
projector attached:

```sh
npm --prefix sdk run verify    # build + verify the Hello World sample headlessly
npm --prefix sdk run launcher  # original stock launcher at http://127.0.0.1:4174
```

The launcher preview runs the genuine Navigator Vue bundle with firmware
services (the loopback websocket RPC) replaced by an explicit local simulator;
unsupported firmware calls fail loudly rather than pretending. Launcher assets
are staged from a rootfs dump you hold locally (not shipped) and are scrubbed at
serve time. This is a development harness, not a firmware emulator — see
`sdk/README.md` for the exact evidence boundaries.

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
- Root: **complete guide at `docs/uart-root.md`** — page-enabled boot console,
  UART wiring (photo inside), mboot tap, self-backup, one-time root shell,
  persistent root payload, recovery. Firmware stays stock and verified.
- A no-hardware root path is research-in-progress (kernel 4.19.116 has
  reachable-but-unweaponized x_tables surface); not published yet because it
  does not exist yet.
