# docs/dumps

raw material from one razmut 0.9.2 install. this is not public data and it is not
redistributed with the repo by default: `strings-*.txt`, `report-*.md` and the
`frontend/` folder are listed in `.gitignore`.

| file | source | how to get it again |
| --- | --- | --- |
| strings-agent-dll.txt | razmut_agent.dll, ascii runs of 6 or more | `node scripts/dump-strings.mjs razmut_agent.dll strings-agent-dll.txt 6` |
| strings-tauri-exe.txt | razmut-tauri.exe, ascii runs of 8 or more | `node scripts/dump-strings.mjs razmut-tauri.exe strings-tauri-exe.txt 8` |
| frontend/ | tauri assets, brotli streams | `node scripts/extract-tauri-assets.mjs razmut-tauri.exe frontend` |
| report-agent-dll.md | this tool | `razmut-recreate report razmut_agent.dll --out report-agent-dll.md` |

## sizes

| artifact | bytes | sha256 |
| --- | --- | --- |
| razmut-tauri.exe | 73402880 | 1915f41bdb0271839f97d929dc933fe799e5f1eabd0225d1db3d50930335cb9d |
| razmut_agent.dll | 3196416 | 2294b33e53f3d27c33848eecc6ebdb9ad12afa751bdf9fbbc672c5b3b15cd593 |
| frontend/index.html | 460 | - |
| frontend/assets__index-D-LfJbjA.js | 82062 | - |
| frontend/assets__index-D1po1LR7.css | 9336 | - |
| frontend/assets__onest-DZuv1oNk.ttf | 193056 | - |
| frontend/assets__mascot-D8ftI7gc.png | 13953 | - |

## where to look

- `strings-agent-dll.txt`: dota/bin/win64/client.dll, PanoramaUIEngine001,
  run_script, vtable_guards, "Ping cooldown differs", RMVoiceIcon,
  "Cosmetic SelfVoice override", crates/agent/src/icon.rs
- `strings-tauri-exe.txt`: me.razmut.desktop, https://razmut.me/api/v1/,
  dotavoicelab-licensing, licensing/lease.json, ClientModeDOTANormal,
  frida/runtime, crates/core/src/platform.rs

## limits

- strings are not a disassembler. some data is compressed (assets) or utf-16,
  the dump script has a `--utf16` flag for that.
- everything here is one build. after an update the addresses and templates move,
  the strings might not.

## git, size and reading them

a 6.4 mb strings dump and a handful of extracted assets do not belong in a normal
diff. three things are set up for that:

1. `.gitattributes` marks the dumps `-text -diff -merge` and
   `linguist-generated` (assets are also `linguist-vendored`). so git never
   rewrites line endings inside a dump, never renders it as one huge diff, never
   tries to merge it, and github does not count it in the language bar.
2. `.gitignore` keeps the raw `.txt`, `report-*.md` and `frontend/` out of
   history. compressed copies (`*.txt.gz`) are not ignored.
3. `scripts/pack-dumps.ps1` writes a gzip next to each raw file with the smallest
   compression level that is still cheap, and optionally deletes the raw ones:

```powershell
./scripts/pack-dumps.ps1
./scripts/pack-dumps.ps1 -RemoveRaw
```

if windows blocks the script ("not digitally signed"), the execution policy is the
reason, not the script:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\pack-dumps.ps1
```

on this install it takes 6779232 bytes of raw dumps down to 1995083, so 71% saved.
the biggest one is the 6.4 mb exe strings dump, which goes to 1.9 mb.

git cannot compress on commit by itself. that is what git-lfs is for, and lfs
needs an lfs client plus a remote that supports it, which is a lot of machinery
for a folder like this. gzip plus an attribute is the boring option that works
everywhere.

if you do commit the `.gz` files, make diffs readable again with a textconv
filter (this is local config, not something `.gitattributes` can set up alone):

```bash
git config diff.gunzip.textconv "gzip -dc"
git config diff.gunzip.cachetextconv true
```

after that `git diff docs/dumps/strings-agent-dll.txt.gz` prints text instead of
"binary files differ". to read one by hand: `gzip -dc file.txt.gz | less`, or on
windows `tar -xOzf file.txt.gz`.
