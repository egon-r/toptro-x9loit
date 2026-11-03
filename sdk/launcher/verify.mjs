import {request as httpRequest} from 'node:http';
import assert from 'node:assert/strict';
import {mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {parse} from 'acorn';
import {build, root} from '../tools/build.mjs';
import {greeting} from '../tools/server.mjs';
import {startLauncher} from './server.mjs';

process.env.PLAYWRIGHT_BROWSERS_PATH = resolve(root, '.cache/browsers');
process.env.TMPDIR = resolve(root, '.cache/tmp');
await mkdir(process.env.TMPDIR, {recursive: true});
const {chromium} = await import('playwright');
await build();
const server = await startLauncher();
let browser;
let page;
let stage = 'startup';
try {
  browser = await chromium.launch({headless: true});
  const context = await browser.newContext({viewport: {width: 1440, height: 900}});
  let external = 0;
  let sockets = 0;
  const errors = [];
  await context.route('**/*', route => {
    if (new URL(route.request().url()).origin === server.url) { return route.continue(); }
    external += 1;
    return route.abort('blockedbyclient');
  });
  await context.routeWebSocket(/.*/, socket => { sockets += 1; socket.close(); });
  page = await context.newPage();
  page.on('pageerror', error => errors.push(error.name));
  page.setDefaultTimeout(20000);
  await page.goto(server.url);
  stage = 'original launcher rendering';
  const stock = page.frameLocator('#stock');
  const tile = stock.locator('img[alt="Hello World SDK"]');
  await tile.waitFor({state: 'visible'});
  assert.equal(await stock.locator('#app').evaluate(node => !!node.__vue__), true);
  // The unit's blue skin: sw_517 derivative must select 517bg02.jpg through
  // the stock setnewBg logic, and the device font must load from shipped OTFs.
  const frame = page.frames().find(frame => frame.url().includes('/stock/'));
  const appearance = await frame.evaluate(async () => {
    // The stock local()-only faces error on desktops; rendering falls through
    // to the shipped OTF face. Wait until that face reports loaded.
    const fontLoaded = await new Promise(resolve => {
      const deadline = Date.now() + 5000;
      (function poll() {
        const loaded = [...document.fonts].some(face =>
          face.family === 'SourceHanSansCN-Regular' && face.status === 'loaded');
        if (loaded || Date.now() > deadline) { resolve(loaded); }
        else { setTimeout(poll, 100); }
      })();
    });
    return {
      theme: document.getElementById('app').className,
      bg: (document.querySelector('.appBg') || {src: ''}).src.split('/').pop(),
      fontLoaded: fontLoaded
    };
  });
  assert.equal(appearance.theme.includes('theme2'), true, 'Theme2 (unit skin) active: ' + appearance.theme);
  assert.equal(appearance.bg, '517bg02.jpg', 'Blue 517 home background expected: ' + appearance.bg);
  assert.equal(appearance.fontLoaded, true, 'Device font SourceHanSansCN must load');
  await mkdir(resolve(root, 'screenshots'), {recursive: true});
  await page.screenshot({path: resolve(root, 'screenshots/stock-launcher.png')});
  console.log('PASS: original Vue launcher renders the development tile');

  stage = 'original tile dispatch';
  await tile.click();
  const sample = page.frameLocator('#sample');
  await sample.locator('#network').filter({hasText: greeting}).waitFor({state: 'visible'});
  assert.equal(await sample.locator('h1').innerText(), 'Hello World');
  assert.equal(server.audit.requests, 1);
  assert.deepEqual(await frame.evaluate(() => window.__x9Preview.launches), ['x9-sdk-hello']);
  await sample.locator('#counter').click();
  assert.equal(await sample.locator('#counter').innerText(), 'Count: 1');
  await page.screenshot({path: resolve(root, 'screenshots/stock-sample.png')});
  console.log('PASS: original stock handler dispatches the simulated launch; sample receives real HTTP');

  stage = 'return to launcher';
  await page.getByRole('button', {name: 'Back to launcher'}).click();
  await tile.waitFor({state: 'visible'});
  assert.deepEqual(await frame.evaluate(() => window.__x9Preview.launches), ['x9-sdk-hello']);
  assert.equal(await page.locator('#sample').isVisible(), false);
  console.log('PASS: return preserves original launcher state');

  stage = 'isolation';
  assert.equal(sockets, 0, 'No real WebSocket should be created: ' + sockets);
  assert.equal(external, 0, 'No external request should escape the page CSP: ' + external);
  assert.deepEqual(errors, [], 'Uncaught page errors: ' + errors.join(', '));
  const renderErrors = await frame.evaluate(() => window.__x9Preview.renderErrors);
  assert.deepEqual(renderErrors, [], 'Caught stock render errors: ' +
    renderErrors.map(error => error.name + '@' + (error.locations || []).join(',')).join('; '));
  const unsupported = await frame.evaluate(() => new Promise(accept => {
    window.wsocket.gSocket.sendData({method: 'x9.preview.unsupported'}, reply => accept(reply.error.code));
  }));
  assert.equal(unsupported, -32601, 'Unknown firmware calls must not report success');
  assert.equal(server.audit.failures, 0);
  for (const path of ['/private/', '/stock/config.ini', '/stock/../../private/', '/stock/page.json', '/stock/libs/base.js']) {
    const response = await context.request.get(server.url + path);
    assert.equal(response.status(), 404);
  }
  // Node fetch ignores a supplied Host header; use HTTP directly for rebinding.
  const reboundStatus = await new Promise((accept, reject) => {
    const request = httpRequest(server.url, {headers: {Host: 'untrusted.invalid'}}, response => {
      response.resume();
      response.once('end', () => accept(response.statusCode));
    });
    request.once('error', reject);
    request.end();
  });
  assert.equal(reboundStatus, 403);
  console.log('PASS: loopback/Host restrictions, private/config paths denied, external traffic blocked');

  stage = 'credential output inspection';
  const bundle = await (await context.request.get(server.url + '/stock/static/js/index.js')).text();
  function inspect(node) {
    if (!node || typeof node !== 'object') { return; }
    let key;
    let value;
    if (node.type === 'Property') { key = node.key.name ?? node.key.value; value = node.value; }
    if (node.type === 'AssignmentExpression' && node.left.type === 'MemberExpression') {
      key = node.left.property.name ?? node.left.property.value; value = node.right;
    }
    if (typeof key === 'string' && /secret|password|authorization|api.?key|app.?key|token|^pin$/i.test(key) &&
        value?.type === 'Literal' && typeof value.value === 'string' && value.value.length) {
      assert.equal(value.value === 'X9_PREVIEW_REDACTED', true, 'Credential literal escaped output sanitization');
    }
    for (const child of Object.values(node)) {
      if (Array.isArray(child)) { child.forEach(inspect); }
      else if (child && typeof child === 'object') { inspect(child); }
    }
  }
  inspect(parse(bundle, {ecmaVersion: 'latest'}));
  assert.equal(bundle.includes('PRIVATE KEY-----'), false, 'Private key material must not be served');
  console.log('PASS: served launcher credential fields contain only redacted values');
  console.log('Asset audit: ' + server.audit.stock.files + ' served; ' + server.audit.stock.redactions + ' credential literals redacted');
  console.log('LOCAL LAUNCHER VERIFICATION PASSED — firmware services and app launch are simulated');
} catch (error) {
  // Never print original console output, RPC parameters, URLs, or source excerpts.
  console.error('Launcher verification failed at ' + stage + ' (' + error.name + '): ' + error.message);
  if (page) {
    const frame = page.frames().find(frame => frame.url().includes('/stock/'));
    if (frame) {
      const diagnostics = await frame.evaluate(() => ({
        adapter: !!window.__x9Preview,
        unsupported: window.__x9Preview && [...new Set(window.__x9Preview.unsupported)]
      })).catch(() => ({available: false}));
      console.error(JSON.stringify(diagnostics));
    }
  }
  process.exitCode = 1;
} finally {
  if (browser) { await browser.close(); }
  await server.close();
}
