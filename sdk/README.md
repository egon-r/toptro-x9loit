# X9 web-app SDK

A device-free proof of concept for **web apps targeting the stock Sraf browser**. Includes a small browser library, a Hello World + HTTP sample, a static build, and executable Chromium verification. **Not a native-app SDK, firmware emulator, app installer, or stock-device certification.**

All SDK source, dependencies, generated builds, and local browser downloads live under `sdk/`. No device-control tool is imported or executed, and no projector or vendor service is contacted. The stock launcher preview stages its UI assets from the local rootfs dump (`private/dumps/rootfs/basic/webui`, permission-restricted) — no device access happens at SDK runtime.

## Setup and verify

Prerequisites: Node.js 22+ and npm; a host supported by Playwright Chromium. Initial dependency/browser installation needs internet access. The verified development host is macOS arm64.

From the repository root, install once:

```sh
(cd sdk && npm ci && npm run browser:install)
```

Then the single build-and-verification command is:

```sh
npm --prefix sdk run verify
```

It exits nonzero on a build or assertion failure. It:

- Checks every shipped JavaScript file against ES5 syntax and builds `sdk/dist/hello/`.
- Starts a real HTTP server on an ephemeral **127.0.0.1** port and launches headless Chromium at 1280×720.
- Checks the visible `Hello World` heading and rendered greeting received from `/api/hello`, including evidence that the HTTP server handled the request.
- Exercises arrow focus, exactly one activation per Enter press, pointer activation, inert background clicks, and a second HTTP request from the refresh button.
- Checks that actual HTTP 503 and malformed-JSON responses surface as errors. No HTTP responses are mocked.
- Fails on uncaught page errors; blocks browser requests outside the local server origin.
- Writes a synthetic local screenshot to `sdk/screenshots/hello.png`, then closes Chromium and the server.

The verification run needs no internet after setup. `node_modules/`, `.cache/`, `dist/`, and generated `screenshots/` are ignored. npm's cache, the headless-only Chromium download, and verification browser temporary profiles are configured inside `sdk/.cache/`.

## Interactive preview

```sh
npm --prefix sdk start
```

Open **http://127.0.0.1:4173**. This rebuilds the sample and serves it until Ctrl-C. Use the pointer to click a button, or arrows to move focus and Enter to activate. The counter and HTTP greeting are independent. The sample automatically requests its greeting on load; **Fetch greeting** requests it again.

The preview intentionally binds loopback only. It is not a device deployment server. `/api/hello` is a development backend returning `{ "message": "Hello from the local HTTP server" }`; `/api/invalid-json` and other `/api/*` routes are verification fixtures, not production APIs.

## Stock launcher preview

```sh
npm --prefix sdk run launcher
```

Opens **http://127.0.0.1:4174** running the **original stock Navigator launcher** — the genuine Vue bundle with its own spatial navigation, not a recreation. The fixture defaults to this unit's skin: project ID 2 → theme2, derivative `sw_517` → the blue `517bg02.jpg` home (both selected through the stock theme/background logic, not hardcoded). Override via `localStorage` `x9-preview-pid` / `x9-preview-machine` before load. A development tile replaces the last app slot; activating it runs the stock launch handler into the SDK sample, and the harness button returns to the launcher with state intact.

What is simulated: the launcher's firmware WebSockets (`ws://127.0.0.1:7681`) are replaced by an in-page adapter with explicit fixtures (project ID, machine type, input source, config values, empty ad/recommend feeds). Unknown firmware methods return errors — never silent success. The launcher is pinned visible because there is no real TV source service. Everything outside this simulation (DirectFB window management, native app processes, IR timing, standby lifecycle) does not exist here.

Assets live in `sdk/stock-ui/` (36 MB, generated from the stock rootfs; untracked, rebuilt by `sdk/tools/stage-stock-ui.sh`). The server path-checks every request, refuses non-loopback clients and non-`127.0.0.1` Host headers, applies a strict page CSP, and script responses are checked for private-key material and have credential-valued literals redacted at serve time. The launcher's original console output (which can contain RPC parameters and identifiers) is suppressed; only error names are surfaced.

The device font `SourceHanSansCN` is declared `local()`-only by the stock CSS, so desktops substitute a fallback (clock text looks wrong). The staging script ships the projector's OTFs (`static/fonts/`) and the adapter injects `@font-face` overrides pointing at them, so text renders with the real device font.

To regenerate `sdk/stock-ui/` from the rootfs dump: `sh sdk/tools/stage-stock-ui.sh [webui-dir]` (default reads `private/dumps/rootfs/basic/webui`; keeps only the English language pack and redacts credential literals in the staged copy).

## Develop an app

- `src/x9.js`: reusable browser library, exporting `window.X9`.
- `examples/hello/index.html`, `style.css`, `app.js`: runnable reference app.
- `tools/build.mjs`: copies the library and sample into `dist/hello/`, rejecting non-ES5 script syntax.
- `tools/server.mjs`: local development/verification server.
- `tools/verify.mjs`: browser-driven build-and-verification entry point.
- `launcher/`: original-stock-launcher preview (`server.mjs`, `adapter.js`, `mount.js`, `serve.mjs`, `verify.mjs`).
- `tools/stage-stock-ui.sh`: regenerates `stock-ui/` from the rootfs dump (see below).

Edit the sample, then run the same verification command. Adapt its assertions when intentionally changing the sample's behavior. `npm --prefix sdk run build` creates the static bundle without launching a browser. To use the library in a different page, include `x9.js` before the app script. There is no runtime npm dependency, bundler, transpiler, module loader, or vendor RPC requirement.

### HTTP JSON

```js
var request = X9.net.getJSON('/api/hello', function (error, value) {
  var output = document.getElementById('result');
  output.textContent = error ? error.message : value.message;
});
// request.abort(); // Cancels the request and reports an abort error.
```

`getJSON(url, callback)` issues an asynchronous GET using `XMLHttpRequest`, with a five-second timeout. On a 2xx response with valid JSON, the callback receives `(null, parsedValue)`; other HTTP statuses, malformed JSON, network errors, timeout, and abort receive an `Error`. Each request completes its callback at most once. It returns the underlying XHR for cancellation. Invalid arguments or a synchronous browser failure to open/send the request can throw. The application must validate its response schema; the Hello World sample requires a string `message`.

Standard browser origin, CORS, mixed-content, certificate, and cookie policies apply. No privilege bypass, proxy, retry, or `file://` reading is provided. Use a same-origin backend for the simplest deployment. An HTTPS-hosted app should use an HTTPS backend. HTTP status and JSON error paths are locally verified; timeout, cancellation, TLS, and cross-origin/device policy behavior are not certified by this harness.

### Input

```js
var detach = X9.input.attach(document.getElementById('app'));
// detach(); // Removes the SDK's keyboard listener when leaving the app.
```

Use one root containing ordinary HTML `<button>` controls, and attach once after rendering them. The SDK initially focuses the first visible, enabled button. Left/Up move to the previous button in DOM order; Right/Down move to the next, clamping at the ends. Enter activates the focused button once; held Enter repeats are ignored where the browser supplies `KeyboardEvent.repeat`. Pointer clicks use normal browser button behavior. Hidden and disabled buttons are excluded. Keyboard navigation is intentionally linear, not spatial, and is intended for button-based pages, not forms with editable fields or complex widgets.

If a background click leaves the document body focused, an arrow key restores focus to the first button. Keyboard events from focused controls outside the app root are left alone.

The sample includes visible focus styling and large targets. No Home/Back/Power behavior is intercepted or synthesized. The stock browser's pointer mode may consume IR arrows before the page sees keyboard events; pointer clicks are the evidenced control path. Local arrow-key verification proves the page's keyboard behavior, **not IR delivery**.

## Firmware evidence and confidence

The authoritative current architecture is documented in [`docs/x9-chain.md`, UI / launcher architecture](../docs/x9-chain.md#ui--launcher-architecture-2026-10-29-recon-root) and [`docs/x9-milestones.md`, M32](../docs/x9-milestones.md). Older README statements about absent SSH/root predate M31 and do not override those records.

| Contract or choice | Project evidence | What this SDK establishes |
| --- | --- | --- |
| Stock UI uses Sraf Chromium over DirectFB, not Android/X11 | `docs/x9-chain.md`, UI / launcher architecture; milestones M32 | Web app is the chosen PoC route; no Android APK or native ABI claim |
| HTML/JS Hello World receives pointer/OK activation | `hello.html`; root README, Hello World and native-bridge investigation | Reuses ordinary buttons, DOM text, and keyCode 13; local pointer/keyboard verification only |
| Browser pages can use ordinary XHR | `x9-cdp-shell.py`, page `L`, `post`, and `poll`; milestones M4 and chain bridge facts | Uses ordinary XHR against a real same-origin local endpoint; does not import or execute the bridge |
| New app code should avoid assuming a modern JS engine | Exact device Chromium version/support matrix is not established here | ES5 syntax is enforced as a conservative development choice, not a measured device feature profile |
| Home launch/app registration is a separate unresolved integration | M32 describes launch verbs and unresolved encrypted Sraf registry | The preview renders a development tile and dispatches the real `GAMSendCmd` verb into the sample — entirely inside the local simulation; no device installation, registration, or boot hook |

This environment **executes the real web bundle in desktop Chromium**. It does not emulate the firmware, ARM CPU, DirectFB compositor, vendor `sraf` object, vendor WebSocket APIs, video/DRM stack, GPU performance, memory limits, storage policy, IR timing, or standby lifecycle. It does not fake those APIs. Browser version, fonts, TLS trust, CSS support, device viewport, remote delivery, and lifecycle can differ on the projector. Do not infer real-device compatibility from a passing run.

A future device validation would need separately authorized access and observation of loading the exact bundle, rendering the heading, receiving an actual backend response, pointer/OK behavior, focus, and standby/reload behavior. This PoC neither performs nor authorizes that work.

## Changelog

### 0.1.0

- Added ES5 browser SDK for JSON GET and ordered button input.
- Added Hello World + same-origin HTTP sample and static build.
- Added loopback-only preview and Chromium verification with rendered-output, interaction, and HTTP error checks.
- Added original-stock-launcher preview (`launcher/`): genuine Navigator UI with an explicit simulated firmware adapter, development tile launch into the SDK sample, isolation checks, and a reproducible `stock-ui/` staging script.
- Recorded evidence boundaries; native integration and stock-device compatibility remain unverified.
- Verified the built sample in Chromium on macOS arm64: rendered greeting, real HTTP exchange, pointer/keyboard input, background-click focus recovery, and HTTP/JSON failures.

### 0.1.1

- Launcher preview now defaults to this unit's skin: project ID 2 (theme2) and derivative `sw_517`, selecting the blue `517bg02.jpg` home through the stock selection logic; overridable via `localStorage` `x9-preview-pid`/`x9-preview-machine`.
- Ships the projector's SourceHanSansCN OTFs with `@font-face` overrides so text renders with the real device font instead of a desktop fallback.
