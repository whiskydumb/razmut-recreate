#!/usr/bin/env node
// Извлекает встроенные в Tauri ассеты. Ассеты хранятся как brotli-потоки, перед
// каждым лежит открытый ключ пути ("/index.html", "/assets/index-XXXX.js").
// Использование:
//   node scripts/extract-tauri-assets.mjs <app.exe> <outDir> [ключ ...]
// Без списка ключей сканируются все строки вида "/..." длиной до 120 символов.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

const [exe, outDir, ...extraKeys] = process.argv.slice(2);
if (!exe || !outDir) {
  console.error('usage: node scripts/extract-tauri-assets.mjs <app.exe> <outDir> [key ...]');
  process.exit(2);
}
fs.mkdirSync(outDir, { recursive: true });

const buf = fs.readFileSync(exe);
console.log(`loaded ${buf.length} bytes`);

function discoverKeys() {
  if (extraKeys.length > 0) return extraKeys;
  const keys = new Set();
  for (let i = 0; i < buf.length - 4; i++) {
    if (buf[i] !== 0x2f) continue; // '/'
    let end = i + 1;
    while (end < buf.length && end - i < 120) {
      const c = buf[end];
      if (c < 0x20 || c > 0x7e) break;
      end++;
    }
    if (end - i < 6) continue;
    const candidate = buf.toString('latin1', i, end);
    if (/^\/[A-Za-z0-9_@\-./]+\.(html|js|css|png|svg|ttf|woff2?|json|wasm)$/.test(candidate)) {
      keys.add(candidate);
    }
  }
  return [...keys];
}

const keys = discoverKeys();
if (keys.length === 0) {
  console.error('no asset keys found');
  process.exit(1);
}

let saved = 0;
for (const key of keys) {
  const needle = Buffer.from(key, 'latin1');
  let offset = -1;
  let done = false;
  while (!done && (offset = buf.indexOf(needle, offset + 1)) !== -1) {
    for (let skip = 0; skip <= 12 && !done; skip++) {
      const start = offset + needle.length + skip;
      if (start >= buf.length) continue;
      try {
        const data = zlib.brotliDecompressSync(buf.subarray(start, Math.min(buf.length, start + 64 * 1024 * 1024)));
        if (data.length < 16) continue;
        const name = key.replace(/^\//, '').replace(/\//g, '__');
        fs.writeFileSync(path.join(outDir, name), data);
        console.log(`${key} @0x${offset.toString(16)} skip=${skip}: ${data.length} bytes -> ${name}`);
        saved++;
        done = true;
      } catch {
        /* not a brotli stream here */
      }
    }
  }
  if (!done) console.log(`${key}: not found`);
}
console.log(`saved ${saved}/${keys.length}`);
