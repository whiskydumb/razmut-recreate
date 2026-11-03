#!/usr/bin/env node
// Дамп печатных строк из бинаря — то, с чего начинался разбор Razmut.
// Использование:
//   node scripts/dump-strings.mjs <file> <out.txt> [minLen] [--utf16]
import fs from 'node:fs';

const [file, out, minLenArg] = process.argv.slice(2);
if (!file || !out) {
  console.error('usage: node scripts/dump-strings.mjs <file> <out.txt> [minLen] [--utf16]');
  process.exit(2);
}
const minLen = Number(minLenArg || 6);
const wide = process.argv.includes('--utf16');
const buf = fs.readFileSync(file);

const lines = [];
const step = wide ? 2 : 1;
let start = -1;
for (let i = 0; i <= buf.length; i += step) {
  const c = i < buf.length ? buf[i] : 0;
  const ok = i < buf.length && c >= 0x20 && c < 0x7f && (!wide || buf[i + 1] === 0);
  if (ok) {
    if (start < 0) start = i;
  } else {
    if (start >= 0 && i - start >= minLen) {
      lines.push(`0x${start.toString(16)} ${buf.toString(wide ? 'utf16le' : 'latin1', start, i)}`);
    }
    start = -1;
  }
}
fs.writeFileSync(out, lines.join('\n'));
console.log(`${lines.length} runs -> ${out}`);
