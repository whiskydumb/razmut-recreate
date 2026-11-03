# 02. shell and agent

## the tauri shell

identifiers and config bits that survived in the binary:

| key | value |
| --- | --- |
| version | 0.9.2 |
| identifier | me.razmut.desktop |
| dev url | http://127.0.0.1:5190 |
| dist dir | ../web-dist |
| updater endpoint | https://razmut.me/updates-tauri/latest.json |
| csp | default-src 'self'; script-src 'self'; img-src 'self' asset: http://asset.localhost data:; connect-src ipc: http://ipc.localhost; object-src 'none' |

webview2 arguments the app sets for itself:

```
--disable-features=msWebOOUI,msPdfOOUI,msSmartScreenProtection
--autoplay-policy=no-user-gesture-required
--proxy-server=http://  or  --proxy-server=socks5://
```

hidden command line modes found in strings: `--game-smoke-report`,
`--self-test-report`, `--private-lobby`, `--migration-session`.

### ipc commands

| command | what it does |
| --- | --- |
| activate | activate a subscription key |
| toggle_module | turn the module on or off |
| set_voice_icon | locked, regular or loud |
| set_ping_cooldown | client side ping cooldown |
| check_updates, apply_update | updater plugin |
| fit_window | resize the window to content |

there is also a `snapshot` event that pushes state to the ui: license, whether the
game is running, hotkey availability, cooldown message.

### ui strings

the ui is in russian. translated, it says:

- "activation by key", "activation key", "activate subscription"
- "activation required", "not activated", "the term starts on first activation"
- "dota 2 is not running"
- "voice icon": "how the icon looks in dota when the module is on. the choice is
  saved automatically"
- "ping cooldown", "error"

default global hotkey is `Ctrl+Alt+F8`.

## the agent dll

pe32+, x64, image base 0x180000000. there is a pdb reference inside
(`razmut_agent.pdb`, guid c7549539-e3fe-4ea6-4c4c-44205044422e) but the pdb is not
shipped.

exports:

| name | rva |
| --- | --- |
| razmut_build_profile | 0xfde6 |
| razmut_entry | 0xfdec |

imports, 163 functions from 8 libraries. the interesting ones:

- kernel32: CreateThread, VirtualProtect, ReadProcessMemory, OpenProcess,
  GetFinalPathNameByHandleW, QueryFullProcessImageNameW, DeviceIoControl
- ws2_32: WSASocketA, WSADuplicateSocketA, WSAGetLastError (its own socket to the
  host process, not the game network)
- crypt32: CryptUnprotectData for the device blob
- ntdll: NtReadFile, NtWriteFile

no steam, no audio, no network hooks on the game side.

### data shapes

serde names that survived because they are used in error messages:

| struct | fields |
| --- | --- |
| Launch | 2 |
| Control | 10 |
| Code | 2 |
| Factory | 3 |
| IconModule | 1 |
| Claims, PublicKey | 7, 3 |

control keys: nonce, utc, active, stopped, restored, pingStatus, iconStatus,
ownerPid, ownerCreated, token, enabled, icon, ping. the host and the agent
exchange them through `control.dat` and `status.json` in a private session
directory.

resource manifest, embedded in both binaries:

```json
{"schema":1,"version":"0.9.2","files":{"razmut_agent.dll":"2294b33e53f3d27c33848eecc6ebdb9ad12afa751bdf9fbbc672c5b3b15cd593"}}
```

that hash matches the shipped dll. mismatch paths are `Resource integrity check
failed`, `Wrong resource version`, `Resource escapes root`.

### how it finds the game

strings: `dota2.exe`, `dota/bin/win64/client.dll`, `Dota 2`, `Dota process
identity changed`, `Unexpected client.dll module`, `Panorama UI thread changed`,
`panorama.dll`.

the target is checked by path and by module content, not just pid. the binary
carries a reference `panorama.dll` digest:
82e6d303c02224c31e2cb5cbb4cd0d09588966d0d89d5dc101e9f503cc4c7e96, preferred base
0x180000000.

### panorama access

the agent does not walk the ui by hand, it asks the engine through
`CreateInterface`:

```
export          CreateInterface, rva 0x3db5b0
interface       PanoramaUIEngine001, registration_rva 0x48d70
factory         rva 0x6d410, object_rva 0x586f60
engine pointer  object+0x28, module_rva 0x586f88
create slot     11, get_engine slot 13, get_engine rva 0x6d5c0 (48 8b 41 28 c3)
vtable guards   CPanoramaUIEngine rva 0x4c04a8
                CUIEngineSource2 rva 0x4c1930
                CUIEngine rva 0x4c7230
                CUIPanel rva 0x4cb1a8
run_script      slot 77, rva 0xb42b0
run_frame       slot 5, rva 0xb6860
```

for `run_script` the binary even documents the abi: rcx engine, rdx panel, r8
source, r9 origin, packed source position on the stack, proven through
`GetScriptOrigin 0xb2fd0`.

### what it checks before patching

rtti names found in the dll: `CBasePlayerController`,
`m_bIsLocalPlayerController`, `C_DOTAPlayerController`, `m_nPlayerID`, and the
descriptor string `.?AVC_DOTAPlayerController@@` at 0x1800b76d0.

refusal messages, which double as a list of checks:

```
Controller RTTI differs
Ambiguous controller schema
Ping cooldown differs
Ping global section, Ping globals
Ping switch base, Ping call binding
Missing ping template, Missing ping pattern
Voice instruction template differs
Voice template extent, Voice RTTI binding
Voice getter binding, Voice branch binding
Unknown voice callee, Fallback binding
Voice resource binding, Voice function extent
```

so every edit is guarded by a full function template plus vtable and rtti
identity. if the build does not match, it stops instead of guessing.
