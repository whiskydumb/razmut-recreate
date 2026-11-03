# 01. what razmut is

razmut preview 0.9.2 is a windows desktop app that finds an installed dota 2,
loads a dll into the game process and changes two things inside the client: the
voice icon the hud draws for your own player, and a few client side checks around
voice and ping. it is sold as a subscription.

that is the whole product. everything else is plumbing: a tauri window, a license
check, an updater and a pattern engine that survives game patches.

## the shipped files

| file | size | sha256 (start) | what it is |
| --- | --- | --- | --- |
| razmut-tauri.exe | 73402880 | 1915f41bdb02 | the desktop app |
| razmut_agent.dll | 3196416 | 2294b33e53f3 | the dll loaded into the game |
| uninstall.exe | 147952 | - | uninstaller |
| NOTICE.txt, licenses/ | - | - | tauri mit, frida wxwindows, onest ofl |

the exe has no authenticode signature.

## the stack

| layer | what is there |
| --- | --- |
| shell | tauri 2.12.5, wry, webview2, vue 3 |
| ui bundle | index.html, assets/index-D-LfJbjA.js (82 kb), one css, one font |
| backend | rust, src-tauri/src/{main,backend,native,game_test,migration_local}.rs |
| core | crates/core/src/{pe,platform,profile}.rs |
| agent | rust dll, crates/agent/src/icon.rs |
| injection | frida core and gum 17.9.11 with the quickjs runtime, built into the exe |
| network | reqwest 0.12 and 0.13, rustls, http 1.5 |
| extras | tauri-plugin-updater 2.14.1, tauri-plugin-global-shortcut 2.4.1, muda |

it was built in a container or wsl: the binary still carries paths like
`/tmp/razmut-tauri-tools/cargo/registry/src/index.crates.io-1949cf8c6b5b557f/...`
and rustc commit `58cf8f9c35b355199727627d07ae34a4e2846d67`.

## how i looked at it

1. static pass: imports, exports, strings, version info, sections. no execution.
2. pulled the frontend out of the binary. tauri keeps assets as brotli streams
   with the path as a plain string in front of each one, so a small script gets
   html, js, css, font and the mascot png without launching anything.
3. read the serde schemas and the json manifests embedded in the dll.
4. decompiled the dll in ghidra 12.2 headless. the agent logic is mostly one 29
   kb function (`FUN_18000778e`, 5460 instructions) because of rust inlining.
5. looked at the runtime leftovers: `%LOCALAPPDATA%\Razmut\licensing`,
   `import-complete`, and the webview2 profile under `me.razmut.desktop`.

## things worth knowing up front

- the voice icon change is cosmetic. the js overlay only repaints your own
  `SelfVoice` panel. the original comment says so itself.
- the interesting changes are native patches in `client.dll` and they only touch
  client side checks. whether the server accepts the result is a different
  question, see `06-open-questions.md`.
- the license is an rs256 jwt bound to a device id, checked offline with an
  embedded public key, plus minisign for updates.
- nothing in the binary touches the voice stream, steam voice or the audio path.

## what is in the other docs

- `02-shell-and-agent.md`: ipc commands, the dll entry points, frida, panorama
- `03-signatures.md`: pattern policy, the catalog, what a hit means
- `04-license.md`: lease files, claims, endpoints, updater
- `05-this-build.md`: the c++ rewrite
- `06-open-questions.md`: proven, unproven, risks
