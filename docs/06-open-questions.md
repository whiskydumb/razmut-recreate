# 06. what is proven, what is not, what it costs

## short version

razmut does not proxy or rewrite anything and it does not touch voice. it patches
client side checks in dota and repaints your own voice icon in the hud. part of it
is functional (client gates, ping cooldown), part is pure cosmetics (the icon).

## proven, from the binaries

| claim | evidence |
| --- | --- |
| tauri 2.12.5 with a vue bundle, assets embedded as brotli streams | version strings, css/js/html pulled out, csp and dev url |
| frida 17.9.11 built into the exe | gum interceptor strings, frida/runtime js, agent symbols |
| the dll is loaded into dota2.exe after checking client.dll | path strings, module identity check, rtti of the player controller |
| patches are full function templates with masked rel32 only | policy json, three templates, vtable guards |
| the hud overlay is cosmetic and self restoring | its own js, `visible_card:false`, the comment about a "cosmetic selfvoice override" |
| license is a device bound signed lease | claims, rs256, dotavoicelab-licensing, dpapi blob |
| updates are minisign signed | endpoint, embedded key id |
| nothing touches the voice stream | no audio, no steam voice, no network hooks on the game |
| the exe is unsigned | no security directory |

## not proven

| guess | why it is still a guess | how to settle it |
| --- | --- | --- |
| the inverted predicate at ClientModeDOTANormal slot 272 removes the communication restriction | slot name is lost, the meaning lives in client.dll | decompile the function and its callers |
| `restriction` reads an expiry timestamp | structure looks like float time compare against a global, but field names are gone | recover C_DOTAPlayerController in ghidra |
| messages and pings actually reach other players after the patch | server side validation was not observed | live test on two accounts, which risks a ban |
| `enqueue` is the chat and ping path | 652 byte function with a queue and a lock, exact role unnamed | decompile and trace callers |

one honest note: the overlay alone proves nothing about gameplay. the native
patches are the part that can matter, and they are client side by construction. a
client side check that is also enforced by the server changes nothing for other
players.

## cost of running the original

- account: injecting into dota2.exe and patching the client breaks the steam and
  dota terms. expect sanctions up to a lock.
- machine: unsigned binary, frida injection, VirtualProtect and dpapi reads are a
  textbook riskware profile. antivirus and edr will likely flag it.
- research: running a paid cheat on your main account and main box is a bad
  trade. static analysis gives more and costs nothing.

## what this repo deliberately leaves out

- no injection, no remote thread, no dll loading
- no writes into any binary
- no dota templates, no removal of client side limits
- no license network client, no signature verification, no updater

that is the line between "rebuilt the idea and documented it" and "shipped another
cheat". everything else is here: formats, protocols, file layout, the pattern
engine, the state machine.

## next steps if you want to keep going

1. run `scan` with your own patterns against a copy of client.dll and see which
   functions the original touches.
2. decompile `restriction` and `enqueue` in the game itself. the templates are
   described in `03-signatures.md`, ghidra already has the dll analysed.
3. if you need the license protocol, use a separate machine and your own account.
