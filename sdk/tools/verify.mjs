import assert from 'node:assert/strict';
import {mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {build, root} from './build.mjs';
import {startServer, greeting} from './server.mjs';

process.env.PLAYWRIGHT_BROWSERS_PATH = resolve(root, '.cache/browsers');
process.env.TMPDIR = resolve(root, '.cache/tmp');
await mkdir(process.env.TMPDIR, {recursive: true});
const {chromium} = await import('playwright');
await build();
const server = await startServer();
let browser;
try {
  browser = await chromium.launch({headless: true});
  const page = await browser.newPage({viewport: {width: 1280, height: 720}});
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  // No internet, firmware service, or projector can be contacted by this run.
  await page.route('**/*', route => {
    if (new URL(route.request().url()).origin === server.url) { return route.continue(); }
    return route.abort('blockedbyclient');
  });
  await page.goto(server.url);
  await page.waitForFunction(expected => document.getElementById('network').textContent === expected, greeting);
  assert.equal(await page.locator('h1').innerText(), 'Hello World');
  assert.equal(await page.locator('h1').isVisible(), true);
  assert.equal(await page.locator('#network').isVisible(), true);
  assert.equal(server.requests.filter(path => path === '/api/hello').length, 1);
  console.log('PASS: rendered Hello World and real HTTP greeting');

  await page.keyboard.press('ArrowRight');
  assert.equal(await page.locator('#counter').evaluate(element => element === document.activeElement), true);
  await page.keyboard.press('Enter');
  assert.equal(await page.locator('#counter').innerText(), 'Count: 1');
  await page.locator('#counter').click();
  assert.equal(await page.locator('#counter').innerText(), 'Count: 2');
  await page.locator('h1').click();
  assert.equal(await page.locator('#counter').innerText(), 'Count: 2');
  await page.keyboard.press('ArrowLeft');
  await page.keyboard.press('Enter');
  await page.waitForFunction(expected => document.getElementById('network').textContent === expected, greeting);
  assert.equal(server.requests.filter(path => path === '/api/hello').length, 2);
  console.log('PASS: arrow focus, single Enter activation, pointer input, HTTP refresh');

  // Exercise errors over actual HTTP, not fulfilled/intercepted mock responses.
  for (const [path, expected] of [['/api/unavailable', 'HTTP 503'], ['/api/invalid-json', 'Invalid JSON response']]) {
    const result = await page.evaluate(url => new Promise(accept => {
      window.X9.net.getJSON(url, (error, value) => accept({error: error && error.message, value}));
    }), path);
    assert.equal(result.error, expected);
    assert.equal(result.value, undefined);
  }
  assert.deepEqual(errors, []);
  console.log('PASS: HTTP failure and invalid JSON surface as errors');
  await mkdir(resolve(root, 'screenshots'), {recursive: true});
  await page.screenshot({path: resolve(root, 'screenshots/hello.png')});
  console.log('Screenshot: screenshots/hello.png');
  console.log('LOCAL VERIFICATION PASSED — stock-device compatibility is unverified');
} finally {
  if (browser) { await browser.close(); }
  await server.close();
}
