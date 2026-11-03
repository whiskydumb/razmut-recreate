# 03. signatures

## what this tool ships

a catalog of nine signatures that are useful when triaging an x64 image. they are
shapes, not patches:

| name | kind | pattern |
| --- | --- | --- |
| msvc_prologue | prologue | 48 89 5C 24 ?? 57 48 83 EC |
| rust_prologue | prologue | 48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 |
| stack_alloc_small | prologue | 48 83 EC 28 |
| bool_invert | codegen | 34 01 84 C0 0F 95 C0 |
| member_getter | codegen | 48 8B 41 28 C3 |
| virtual_call | codegen | FF 92 ?? ?? ?? ?? |
| rtti_class | data | 2E 3F 41 56 |
| rust_panic | data | 70 61 6E 69 63 6B 65 64 20 61 74 |
| import_thunk | data | FF 25 ?? ?? ?? ?? |

`list` them with `sigs`, or use them by name: `scan <file> --sig rust_prologue`.

## how matching works

- a pattern is whitespace separated hex bytes, `??` is one wildcard byte
- a pattern must have at least one fixed byte, otherwise it is rejected
- the matcher anchors on the first fixed byte and then checks the whole pattern
  with the mask
- hits are reported as file offset and length, `--json` gives machine output

`scan` returns 0 when there is at least one hit, 1 when there is none, 2 on bad
input. `plan` builds the same hits into a dry run byte plan.

## what the original does

the original does not use short signatures. it carries whole function templates,
keyed to one build of `client.dll` and `panorama.dll`, with this policy text
embedded:

```
Only instruction-identical relocation within these full templates is accepted
automatically. Relative CALL and RIP displacements are masked; branch layout,
constants and field offsets remain fixed.
```

so the wildcards only cover call and rip displacements, which change when the
linker moves code around. everything else must match byte for byte. on top of that
there are vtable guards (slot numbers and rvas) and rtti checks.

three templates were visible inside `razmut_agent.dll`:

| template | size | shape |
| --- | --- | --- |
| enqueue | 652 | large queueing function: saves rbx/rbp/rsi then sub rsp,0x50, marks a byte at [rcx+0x30], calls through [rdx+0x110], manages a counter with lock cmpxchg |
| restriction | 162 | small predicate: loads [this+0x678], multiplies the index by 0xF0, reads fields at +0xE0, +0xE4, +0xE8, compares a float with a global plus 0x30, returns bool |
| local_player_id | 60 | push rbx, sub rsp,0x20, one call, stores eax into [rbx]: returns the local player id |

the shell carries one more, for the game mode class:

```
class_name  .?AVClientModeDOTANormal@@
slot        272
predicate   ... 34 01  84 c0  0f 95 c0
                  xor al,1 / test al,al / setne al
```

that tail is the interesting bit: the predicate result gets negated. this is what
`bool_invert` in the catalog is modelled after, and what `plan` emits when you ask
for the `invert` strategy.

## how to read a hit

- `msvc_prologue` or `rust_prologue`: a function entry. stack slot bytes differ
  between builds, so the wildcard there is normal
- `bool_invert`: the end of a function that returns a negated boolean. on its own
  it says nothing about meaning, only that a gate is inverted there
- `member_getter`: one instruction getter at a fixed struct offset. useful to
  recover engine pointers
- `rtti_class` / `rust_panic`: data markers, cheap way to confirm MSVC RTTI or a
  rust build
- `import_thunk`: how many thunks you see is a lower bound on imported functions

## dry run

`plan` prints va, before bytes and after bytes per site and then exits. there is
no write path in this binary at all, by design. see
`06-open-questions.md` for why.
