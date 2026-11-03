import {build} from '../tools/build.mjs';
import {startLauncher} from './server.mjs';
await build();
const server = await startLauncher({port: 4174});
console.log('Stock launcher preview: ' + server.url + ' (loopback only; simulated firmware services)');
for (const signal of ['SIGINT', 'SIGTERM']) {
  process.once(signal, async () => { await server.close(); });
}
