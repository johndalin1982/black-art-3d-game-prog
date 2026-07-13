# 32-bit DOS/4GW project porting guide

## Ground truth

Treat this as the authoritative baseline for what exists in `32bit/` — if a
chapter isn't listed below, it has no 32-bit port. This is unrelated to VESA
work; see [vbe/PORTING.md](../vbe/PORTING.md) for that.

## What this is

`32bit/chapXX/` holds a **flat 32-bit DOS/4GW build** of an existing book
demo — same engine, same demo source, same 320×200 mode-13h graphics as the
16-bit original in `chapXX/`. The only difference is `-dDOS_32_BIT`
(`engine/black3.h`'s memory-model macros switch from `_fmalloc`/`_far`
segmented-heap calls to flat `malloc`/pointers) — there is no resolution
change, no camera pattern, and no rescaled assets to generate, unlike a VESA
port. Ported so far:

| Directory | Project | Source |
|---|---|---|
| `32bit/chap03/` | `light`, `mode13`, `modez` | `../../chap03/*.c` |
| `32bit/chap04/` | `alien`, `pcxdemo`, `speed`, `spheres`, `worms` | `../../chap04/*.c` |
| `32bit/chap05/` | `joytest`, `keytest`, `mousetst`, `ship` | `../../chap05/*.c` |
| `32bit/chap06/` | `digidemo`, `mididemo` | `../../chap06/*.c` |
| `32bit/chap07/` | `critters`, `floater`, `jumper`, `lockon`, `lostnspc` | `../../chap07/*.c` |
| `32bit/chap08/` | `jelly`, `timer`, `vblank`, `volcano` | `../../chap08/*.c` |
| `32bit/chap09/` | `blazer`, `term1`, `term2` | `../../chap09/*.c` |
| `32bit/chap11/` | `linedemo`, `wiredemo` | `../../chap11/*.c` |
| `32bit/chap12/` | `gourdemo`, `solidemo`, `textdemo`, `tridemo` | `../../chap12/*.c` |
| `32bit/chap13/` | `sol2demo` | `../../chap13/sol2demo.c` |
| `32bit/chap14/` | `objects` | `../../chap14/objects.c` |
| `32bit/chap15/` | `bspdemo`, `solzdemo`, `sortdemo`, `zdemo` | `../../chap15/*.c` |
| `32bit/chap16/` | `voxel`, `voxopt`, `voxtile` | `../../chap16/*.c` |
| `32bit/chap17/` | `blaze3d` | `../../chap17/blaze3d.c` |
| `32bit/chap18/` | `krk` | `../../chap18/krk.c` |

Engine module set scales with what each demo actually needs (same set its
16-bit `.tgt` already lists): `black3` alone for the simplest graphics-only
demos, up through `black3`/`4`/`5`/`6`/`8`/`9` for the input+sound+timer+modem
demos (chap09). Any demo pulling in `black6` (sound) also needs `dpmi.c` —
DIGPAK/MIDPAK are real-mode TSRs reached through the DPMI bridge in a 32-bit
build (see the repo README's [16-bit vs. 32-bit builds](../README.md#16-bit-vs-32-bit-builds)).

## Naming & layout

One folder per chapter: `32bit/chapXX/`, mirroring the book's own `chapXX/`
layout. Keep the book's own project name for the `.tgt`/`.wpj` (`blazer`,
`objects`, `bspdemo`, ...) — there's no separate 32-bit filename to invent,
the same way `vbe/chapXX/` keeps the book's names. `32bit/chapXX/` holds only
Watcom project files (`.tgt`/`.wpj`) and runtime assets (PCX/VOC/XMI/PLG/
`font.bin`) — no `.c` sources of its own. The demo source is
`chapXX/foo.c` (via `#ifdef DOS_32_BIT`, already there for every chapter
that's ever needed a 32-bit build — no separate porting step); the engine
source is `engine/black*.c`, same files the 16-bit build uses.

Runtime assets are **plain copies of the book's own**, not resolution-scaled
— a 32-bit build draws the identical 320×200 image as the 16-bit build, just
compiled with flat pointers. If a chapter's `chapXX/` assets change, copy the
update into `32bit/chapXX/` too (DOS resolves relative filenames against the
mounted `C:`, not the source tree, so the demo needs its own copy at
runtime).

## Adding a new 32-bit port

Copy an existing `32bit/chapYY/foo.tgt`/`.wpj` (e.g. `32bit/chap09/blazer.tgt`)
as a template and change:

| What | Rule |
|---|---|
| EXE filename | `foo.exe` — prefix = `len("foo.exe")` |
| Demo `.c` path | `..\..\chapXX\foo.c` — prefix = `len("..\..\chapXX\foo.c")` |
| Engine source paths | `..\..\engine\black3.c` etc. — already correct if copied from another `32bit/chapYY/*.tgt`, no change needed |
| Compiler defines (`?????WLANG_d`) | `DOS_32_BIT` only — no `VBE_SUPPORT` |
| `.wpj`'s two `WFileName` entries | must match the renamed `.tgt`'s filename |

The `.tgt`/`.wpj` format is the same **line-based, length-prefixed** Watcom
project format `vbe/PORTING.md` documents in full — see
[vbe/PORTING.md § Watcom IDE project files](../vbe/PORTING.md#watcom-ide-project-files-wpj--tgt)
for the length-prefix rule and a length-prefix validator script. Validate
every edit before opening it in the IDE: a wrong prefix silently corrupts or
drops the entry.
