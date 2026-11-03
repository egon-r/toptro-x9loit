import {build} from './build.mjs';
import {startServer} from './server.mjs';
await build();
const server = await startServer({port: 4173});
console.log('X9 web preview: ' + server.url + ' (loopback only; Ctrl-C to stop)');
for (const signal of ['SIGINT', 'SIGTERM']) {
  process.once(signal, async () => { await server.close(); });
}
