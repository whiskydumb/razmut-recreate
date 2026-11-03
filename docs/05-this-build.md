# 05. this build

## why it looks like this

the first version of this repo copied the original layout module for module: an
`easm` folder, an `obfuscator`, a `config_parser`, a `vendor/` with empty readme
files. that was silly. this is a small tool, so it is laid out like a small tool:
one flat source directory, one file per job, no placeholders.

## layout

| file | job |
| --- | --- |
| `src/common.*` | file io, hex, padding, logging, fnv1a and crc32 |
| `src/pe.*` | pe32 and pe32+ reader: sections, imports, exports, rva to offset |
| `src/scan.*` | signature catalog, pattern compiler, matcher, json output |
| `src/patch.*` | dry run plan and the two byte encodings it knows |
| `src/state.*` | paths, state.ini, lease.json, device id, key format |
| `src/cli.*` | argument parsing and commands |
| `src/tui.*` | full screen interface over the same state |
| `tests/main.cpp` | 39 checks, no test framework |

no third party code, no fetch at configure time, no vendored copies.

## decisions

- errors are exceptions (`rzm::Error`), the cli catches once and prints one line
- `scan` and `plan` use exit codes that are useful in scripts: 0 hit, 1 no hit,
  2 bad arguments
- the tui and the cli share one state module, so `icon loud` in the cli shows up
  in the ui immediately
- state defaults to `%LOCALAPPDATA%\razmut-recreate`, `RZM_HOME` overrides it, so
  tests and ci never touch a real profile
- padding counts code points, not bytes, otherwise the russian strings in the
  tables would break every column
- the plan never writes. there is no code path that opens a target for writing

## what is implemented

- pe inspection, signature scan with wildcards, dry run plan, markdown report
- offline key generation and activation, module state, state file layout close to
  the original
- full screen ui for the state bits
- tests for the matcher, the catalog, the encodings, the hashes, the key format
  and a state round trip

## what is not

- process injection, remote threads, dll loading, frida
- writing bytes into any binary
- dota specific templates and the two patches that remove client side limits
- license network calls, lease signature verification, updater

## extending it

- new signature: add an entry to `catalog()` in `src/scan.cpp`, nothing else
  changes
- new pe field: add a method to `pe::File` next to `imports()` / `exports()`
- new command: add `cmd_*` in `src/cli.cpp`, one line in `run()`, one row in
  `help()` and one item in the tui if it belongs there
- new report section: `cmd_report` builds markdown into a string, so it is just
  more lines in one place

## build details

- c++20, g++ 16.1 (mingw-w64 ucrt), cmake 4.4, ninja 1.13, windows 11
- clang was not installed here, `cmake --preset clang` is there for when it is
- static runtime on mingw, so the exe runs without dlls next to it
- build output: `build/release/bin/razmut-recreate.exe`, tests:
  `build/release/bin/rzm_tests.exe`
