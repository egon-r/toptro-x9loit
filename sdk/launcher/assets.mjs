import {readFile, realpath} from 'node:fs/promises';
import {resolve, relative, extname, sep} from 'node:path';
import {parse} from 'acorn';
import {root} from '../tools/build.mjs';

const sensitive = /secret|password|authorization|api.?key|app.?key|token|^pin$/i;
const types = new Map(Object.entries({
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8', '.png': 'image/png', '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg', '.svg': 'image/svg+xml', '.gif': 'image/gif',
  '.webp': 'image/webp', '.woff': 'font/woff', '.woff2': 'font/woff2',
  '.ttf': 'font/ttf', '.otf': 'font/otf'
}));

// Never log source excerpts or credential values. Private source remains read-only.
export function sanitizeScript(source) {
  const changes = [];
  function visit(node) {
    if (!node || typeof node !== 'object') { return; }
    let name;
    let value;
    if (node.type === 'Property') {
      name = node.key.name ?? node.key.value;
      value = node.value;
    } else if (node.type === 'AssignmentExpression' && node.left.type === 'MemberExpression') {
      name = node.left.property.name ?? node.left.property.value;
      value = node.right;
    }
    if (typeof name === 'string' && sensitive.test(name) && value?.type === 'Literal' &&
        typeof value.value === 'string' && value.value.length) {
      changes.push({start: value.start, end: value.end, replacement: '"X9_PREVIEW_REDACTED"', credential: true});
    }
    if (node.type === 'Literal' && node.value === 'https://cms.metaxplay.com/v1/launch/ctvos') {
      // The real Home requests this feed without handling network errors.
      // Localize that known service to an explicit empty-feed simulation.
      changes.push({start: node.start, end: node.end, replacement: '"/preview/recommendations"'});
    }
    for (const child of Object.values(node)) {
      if (Array.isArray(child)) { child.forEach(visit); }
      else if (child && typeof child === 'object') { visit(child); }
    }
  }
  visit(parse(source, {ecmaVersion: 'latest'}));
  for (const change of changes.sort((a, b) => b.start - a.start)) {
    source = source.slice(0, change.start) + change.replacement + source.slice(change.end);
  }
  if (/-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----/.test(source)) {
    throw new Error('Refusing private key material in launcher source');
  }
  return {source, redactions: changes.filter(change => change.credential).length};
}

export async function loadStockAssets() {
  const directory = await realpath(process.env.X9_WEBUI_DIR || resolve(root, 'stock-ui'));
  const cache = new Map();
  const audit = {files: 0, redactions: 0};
  for (const file of ['index.html', 'static/js/index.js', 'static/js/chunk-vendors.js']) {
    await readFile(resolve(directory, file));
  }
  return {
    audit,
    async get(name) {
      const allowed = name === 'index.html' || name === 'custom/config.js' ||
        /^static\/(?:js|css|img|media|fonts)\/[a-zA-Z0-9_./@-]+$/.test(name);
      const type = types.get(extname(name));
      if (!allowed || !type || name.split('/').includes('..')) { return null; }
      if (cache.has(name)) { return cache.get(name); }
      let file;
      try { file = await realpath(resolve(directory, name)); }
      catch { return null; }
      const rel = relative(directory, file);
      if (rel.startsWith('..' + sep) || rel === '..' || resolve(file) === directory) { return null; }
      let body = await readFile(file);
      if (name.endsWith('.js')) {
        const checked = sanitizeScript(body.toString('utf8'));
        body = Buffer.from(checked.source);
        audit.redactions += checked.redactions;
      }
      if (name === 'index.html') {
        let html = body.toString('utf8');
        // Prefetch is unnecessary for a focused preview and loads unrelated firmware pages.
        html = html.replace(/<link\b[^>]*rel=["']?(?:prefetch|preload)["']?[^>]*>/gi, '');
        html = html.replace(/<head>/i, '<head><script src="/preview/adapter.js"></script>');
        html = html.replace(/<\/body>/i, '<script src="/preview/mount.js"></script></body>');
        body = Buffer.from(html);
      }
      const asset = {body, type};
      audit.files += 1;
      cache.set(name, asset);
      return asset;
    }
  };
}
