import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {dist} from './build.mjs';

export const greeting = 'Hello from the local HTTP server';
const assets = new Map([
  ['/', ['index.html', 'text/html; charset=utf-8']],
  ['/index.html', ['index.html', 'text/html; charset=utf-8']],
  ['/x9.js', ['x9.js', 'text/javascript; charset=utf-8']],
  ['/app.js', ['app.js', 'text/javascript; charset=utf-8']],
  ['/style.css', ['style.css', 'text/css; charset=utf-8']]
]);

export async function startServer({port = 0} = {}) {
  const requests = [];
  const server = createServer(async (request, response) => {
    const path = new URL(request.url, 'http://localhost').pathname;
    response.setHeader('Cache-Control', 'no-store');
    if (request.method !== 'GET') {
      response.writeHead(405, {'Allow': 'GET'}).end();
      return;
    }
    if (path.startsWith('/api/')) {
      requests.push(path);
      response.setHeader('Content-Type', 'application/json');
      if (path === '/api/hello') {
        response.end(JSON.stringify({message: greeting}));
      } else if (path === '/api/invalid-json') {
        response.end('{broken');
      } else {
        response.writeHead(503).end(JSON.stringify({error: 'Service unavailable'}));
      }
      return;
    }
    const asset = assets.get(path);
    if (!asset) { response.writeHead(404).end('Not found'); return; }
    try {
      const content = await readFile(resolve(dist, asset[0]));
      response.writeHead(200, {'Content-Type': asset[1]}).end(content);
    } catch {
      response.writeHead(500).end('Build the sample first: npm run build');
    }
  });
  await new Promise((accept, reject) => {
    server.once('error', reject);
    server.listen(port, '127.0.0.1', accept);
  });
  return {
    url: 'http://127.0.0.1:' + server.address().port,
    requests,
    close: () => new Promise((accept, reject) => {
      server.close(error => error ? reject(error) : accept());
      server.closeAllConnections();
    })
  };
}
