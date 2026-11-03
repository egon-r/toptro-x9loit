import {readFile, mkdir, copyFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {resolve} from 'node:path';
import {parse} from 'acorn';

export const root = fileURLToPath(new URL('../', import.meta.url));
export const dist = resolve(root, 'dist/hello');
const sources = [
  ['src/x9.js', 'x9.js'],
  ['examples/hello/app.js', 'app.js'],
  ['examples/hello/index.html', 'index.html'],
  ['examples/hello/style.css', 'style.css']
];

export async function build() {
  // Reject modern syntax in every script shipped to the older device browser.
  for (const [source] of sources) {
    if (source.endsWith('.js')) {
      parse(await readFile(resolve(root, source), 'utf8'), {ecmaVersion: 5});
    }
  }
  await mkdir(dist, {recursive: true});
  for (const [source, name] of sources) {
    await copyFile(resolve(root, source), resolve(dist, name));
  }
  console.log('Built dist/hello (ES5 script syntax checked)');
}
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  await build();
}
