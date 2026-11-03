#!/bin/sh
# Regenerate sdk/stock-ui from the stock rootfs WebUI dump.
# Usage: tools/stage-stock-ui.sh [webui-dir]   (default: ../private/dumps/rootfs/basic/webui)
set -eu
src=${1:-"$(dirname "$0")/../../private/dumps/rootfs/basic/webui"}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
dest=$root/stock-ui
[ -d "$src/static/js" ] || { echo "missing WebUI source: $src" >&2; exit 1; }
rm -rf "$dest"
mkdir -p "$dest/custom" "$dest/static"
cp "$src/index.html" "$dest/"
cp "$src/custom/config.js" "$dest/custom/"
for part in js css img media; do
  [ -d "$src/static/$part" ] && cp -R "$src/static/$part" "$dest/static/"
done
# Launcher text is SourceHanSansCN via local(); desktop hosts lack it, so
# ship the device OTFs for the preview's @font-face override.
mkdir -p "$dest/static/fonts"
cp "$src"/wizard/static/fonts/SourceHanSansCN-*.otf "$dest/static/fonts/" 2>/dev/null || true
# English only; every other language pack is dead weight for local preview.
find "$dest/static/js" -name 'lang-*.js' ! -name 'lang-ENG-js.js' -delete
node --input-type=module - <<'EOF'
import {readFile, writeFile} from 'node:fs/promises';
import {parse} from 'acorn';
import {fileURLToPath} from 'node:url';
const root = fileURLToPath(new URL('.', import.meta.url));
const file = root + 'stock-ui/static/js/index.js';
const source = await readFile(file, 'utf8');
const sensitive = /secret|password|authorization|api.?key|app.?key|token|^pin$/i;
const changes = [];
function visit(node) {
  if (!node || typeof node !== 'object') { return; }
  let name;
  let value;
  if (node.type === 'Property') { name = node.key.name ?? node.key.value; value = node.value; }
  if (node.type === 'AssignmentExpression' && node.left.type === 'MemberExpression') {
    name = node.left.property.name ?? node.left.property.value; value = node.right;
  }
  if (typeof name === 'string' && sensitive.test(name) && value?.type === 'Literal' &&
      typeof value.value === 'string' && value.value.length) {
    changes.push({start: value.start, end: value.end});
  }
  for (const child of Object.values(node)) {
    if (Array.isArray(child)) { child.forEach(visit); }
    else if (child && typeof child === 'object') { visit(child); }
  }
}
visit(parse(source, {ecmaVersion: 'latest'}));
let out = source;
for (const change of changes.sort((a, b) => b.start - a.start)) {
  out = out.slice(0, change.start) + '"X9_PREVIEW_REDACTED"' + out.slice(change.end);
}
await writeFile(file, out);
console.log('stock-ui staged; credential literals redacted: ' + changes.length);
EOF
