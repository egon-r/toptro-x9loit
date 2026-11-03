import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {resolve} from 'node:path';
import {dist} from '../tools/build.mjs';
import {greeting} from '../tools/server.mjs';
import {loadStockAssets} from './assets.mjs';

const directory = fileURLToPath(new URL('.', import.meta.url));
const previewFiles = new Map([
  ['/', ['index.html', 'text/html; charset=utf-8']],
  ['/preview/style.css', ['style.css', 'text/css; charset=utf-8']],
  ['/preview/shell.js', ['shell.js', 'text/javascript; charset=utf-8']],
  ['/preview/adapter.js', ['adapter.js', 'text/javascript; charset=utf-8']],
  ['/preview/mount.js', ['mount.js', 'text/javascript; charset=utf-8']]
]);
const sampleFiles = new Map([
  ['/sample/', ['index.html', 'text/html; charset=utf-8']],
  ['/sample/x9.js', ['x9.js', 'text/javascript; charset=utf-8']],
  ['/sample/app.js', ['app.js', 'text/javascript; charset=utf-8']],
  ['/sample/style.css', ['style.css', 'text/css; charset=utf-8']]
]);
const csp = "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; font-src 'self' data:; connect-src 'self'; frame-src 'self'; object-src 'none'; media-src 'none'; worker-src 'none'; base-uri 'self'; form-action 'none'; frame-ancestors 'self'";

export async function startLauncher({port = 0} = {}) {
  const stock = await loadStockAssets();
  const audit = {requests: 0, failures: 0, stock: stock.audit};
  const server = createServer(async (request, response) => {
    response.setHeader('Content-Security-Policy', csp);
    response.setHeader('Cache-Control', 'no-store');
    response.setHeader('X-Content-Type-Options', 'nosniff');
    response.setHeader('Referrer-Policy', 'no-referrer');
    // Reject DNS rebinding as well as non-loopback clients.
    if (!/^127\.0\.0\.1:\d+$/.test(request.headers.host || '') ||
        !['127.0.0.1', '::ffff:127.0.0.1'].includes(request.socket.remoteAddress)) {
      response.writeHead(403).end('Loopback access only'); return;
    }
    if (request.method !== 'GET') { response.writeHead(405, {'Allow': 'GET'}).end(); return; }
    try {
      const path = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
      if (path === '/api/hello') {
        audit.requests += 1;
        response.writeHead(200, {'Content-Type': 'application/json'}).end(JSON.stringify({message: greeting}));
        return;
      }
      if (path === '/preview/recommendations') {
        response.writeHead(200, {'Content-Type': 'application/json'}).end(JSON.stringify({
          code: 0, data: [], update_cycle: 3600
        }));
        return;
      }
      const entry = previewFiles.get(path) || sampleFiles.get(path);
      if (entry) {
        const body = await readFile(resolve(sampleFiles.has(path) ? dist : directory, entry[0]));
        response.writeHead(200, {'Content-Type': entry[1]}).end(body);
        return;
      }
      const prefix = ['/stock/', '/basic/webui/'].find(prefix => path.startsWith(prefix));
      if (prefix) {
        const asset = await stock.get(path.slice(prefix.length));
        if (asset) { response.writeHead(200, {'Content-Type': asset.type}).end(asset.body); return; }
      }
      response.writeHead(404).end('Not available in the preview');
    } catch {
      audit.failures += 1;
      response.writeHead(500).end('Local asset could not be served safely');
    }
  });
  server.on('upgrade', (request, socket) => socket.destroy());
  await new Promise((accept, reject) => { server.once('error', reject); server.listen(port, '127.0.0.1', accept); });
  return {
    url: 'http://127.0.0.1:' + server.address().port,
    audit,
    close: () => new Promise((accept, reject) => {
      server.close(error => error ? reject(error) : accept());
      server.closeAllConnections();
    })
  };
}
