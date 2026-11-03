# razmut-recreate

> my own take on razmut 0.9.2, without tauri, webview and rust

razmut is a paid dota 2 tool: a tauri shell, a frida host and a small dll that
gets loaded into the game. i took it apart and rebuilt the parts i could rebuild
in a sane way: a cli/tui that walks a pe file, shows which signatures the
original relies on, keeps a local module state and writes nothing it is not asked
to write.

this is not a port. the original is rust plus vue plus a js overlay. this one is
plain c++20 with cmake and ninja, one binary, no dependencies.

## what it does

- parses pe32 and pe32+, prints sections, imports, exports, entry point
- has a small catalog of signatures (prologues, codegen idioms, data markers) and
  a matcher with `??` wildcards
- shows a dry run byte plan for a matched site, never writes it
- writes a markdown report about an artifact
- keeps module state (on/off, voice icon mode, ping cooldown) and an offline
  license lease in the same shape the original uses
- has a full screen terminal ui for the state bits

## build

needs cmake 3.25+, ninja and any c++20 compiler. i built it with mingw g++ 16.1
because there is no clang on this machine; there is a `clang` preset for when
there is one.

```powershell
cmake --preset release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

binary lands in `build/release/bin/razmut-recreate.exe`.

## usage

```powershell
$exe = "build\release\bin\razmut-recreate.exe"

& $exe pe path\to\razmut_agent.dll
& $exe scan path\to\razmut_agent.dll --sig rust_prologue
& $exe plan path\to\razmut_agent.dll --sig bool_invert
& $exe report path\to\razmut_agent.dll --out report.md

& $exe keygen --seed 7
& $exe activate --key RZM-.... --days 30
& $exe module on
& $exe icon loud
& $exe ping 4200
& $exe status

& $exe ui
```

`scan` exits 0 when something matched, 1 when nothing did, 2 on bad arguments, so
it works in scripts.

## layout

- `src/common.*`: files, hex, padding, logging, hashes
- `src/pe.*`: pe reader
- `src/scan.*`: signature catalog and matcher
- `src/patch.*`: byte plan and the two encodings it knows
- `src/state.*`: paths, state.ini, lease.json, device id, key format
- `src/cli.*`: commands
- `src/tui.*`: full screen ui
- `tests/`: 39 checks, no framework
- `docs/`: how the original works, what is proven and what is not
- `.gitattributes`: dumps and assets are marked `-text -diff -merge`, so git
  never rewrites a byte dump and never renders one as a giant diff
- `scripts/`: two node tools that pull strings and tauri assets out of the
  original binaries, plus `pack-dumps.ps1` for the gzip copies (see
  `docs/dumps/README.md`)

## what i did not build

no process injection, no remote thread, no byte writes into someone else's
binary, no dota specific templates. `plan` prints what would change and stops
there. that is on purpose: the rest of the thing is a reverse engineering
exercise, and shipping the last mile would just make another cheat. details in
`docs/06-open-questions.md`.

## docs

- [docs/01-what-it-is.md](docs/01-what-it-is.md): what razmut is, in plain words
- [docs/02-shell-and-agent.md](docs/02-shell-and-agent.md): tauri shell, ipc, the dll
- [docs/03-signatures.md](docs/03-signatures.md): how the pattern matching works
- [docs/04-license.md](docs/04-license.md): lease, device id, updates
- [docs/05-this-build.md](docs/05-this-build.md): what is in this rewrite
- [docs/06-open-questions.md](docs/06-open-questions.md): proven, unproven, risks
- [docs/dumps/](docs/dumps/): strings, extracted frontend, report

## notes

- state goes to `%LOCALAPPDATA%\razmut-recreate` by default, `RZM_HOME` overrides
  it, `NO_COLOR` kills colors
- the original stores the same files under `%LOCALAPPDATA%\Razmut`
- tested on windows 11 with g++ 16.1, cmake 4.4, ninja 1.13

## how this was written, and what is not here

the analysis, the code and the docs were done by an ai model while i was walking
through the binaries. i reviewed it, built
it, ran the tests and every command, but treat the wording and most of the code as
model output, not as a hand written library.

two things are missing on purpose: the injector and the dota half.

- injector: loading `razmut_agent.dll` into `dota2.exe`, remote threads, live
  memory writes, the frida plumbing around it.
- dota half: the two patches that lift the client side mute gate and the ping
  cooldown, plus the exact templates for one client build.

to be clear, this is not a model refusal. i could have pointed an unrestricted
model at it and gotten the same bytes out; that was never the blocker.

the real reasons, in order:

1. it rots in weeks. every dota update moves the functions and the templates die.
   the original carries a version pinned pattern set and still breaks on patch
   days. a rebuilt one would be a museum piece by next month.
2. it is a cheat. a client patch that removes a communication restriction and
   shortens the ping cooldown is the kind of thing that gets accounts locked, and
   shipping it to everyone who clones this repo is a bad trade for a demo.
3. honestly, i could not be bothered. the interesting part was the shell, the
   agent, panorama, the lease. re-implementing the last mile is mechanical work
   with a bad payoff.

so the engineering around the payload is here and the payload is not. the docs say
exactly where it would go, which is enough if you want to reason about it, and not
enough if you just want to press a button.
