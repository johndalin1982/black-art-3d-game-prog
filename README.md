# Black Art of 3D Game Programming — modernized port

A hand-converted port of every demo and engine module from André LaMothe's 1995 book *Black Art of 3D Game Programming*, built with **Open Watcom 2.0 beta** and targeting both **16-bit real-mode DOS** and **32-bit DOS/4GW protected mode**. Every chapter demo from the original CD has been reformatted into a consistent modern coding style, and the engine has been audited and patched for latent bugs that the original code carried. It also goes beyond a straight port in places — most substantially an original **LAN-multiplayer layer over IPX** ([engine/ipx.c](engine/ipx.c)) that gives Starblazer head-to-head play the 1995 code never shipped (see [Networking](#networking-lan-play)).

## Table of contents

- [What this is](#what-this-is)
- [Build requirements](#build-requirements)
- [Repository layout](#repository-layout)
- [Engine modules](#engine-modules)
- [Chapter demos](#chapter-demos)
- [Coding style](#coding-style)
- [Notable fixes vs. the book code](#notable-fixes-vs-the-book-code)
- [16-bit vs. 32-bit builds](#16-bit-vs-32-bit-builds)
- [Building](#building)
- [Running under DOSBox-X](#running-under-dosbox-x)
- [Audio](#audio)
- [Single-player AI](#single-player-ai)
- [Networking (LAN play)](#networking-lan-play)
- [SVGA (VESA)](#svga-vesa)
  - [Starblazer and the camera pattern](#starblazer-and-the-camera-pattern)
- [Credits and license](#credits-and-license)

## What this is

LaMothe's *Black Art of 3D Game Programming* (1995, Waite Group Press) was the canonical "how to write a 3D engine in DOS" tome of the era. The book's CD shipped with full source for a software rasterizer engine, a series of chapter-by-chapter demos building up to that engine, and three complete game demos:

- **Starblazer** (chapter 9) — two-player top-down space-combat game, originally played head-to-head over a modem/serial link; this port extends it with **IPX LAN play** (see [Networking](#networking-lan-play)) and a **single-player mode with an AI-controlled enemy ship** (see [Single-player AI](#single-player-ai))
- **Starblazer 3-D** (chapter 17) — asteroids-style first-person 3D space shooter
- **Kill or Be Killed** (chapter 18) — single-player mech-vs-aliens 3D action game with a HUD, radar scanner, and selectable battle mechs

This repository is a faithful port of all of that material to **Open Watcom 2.0 beta** (community fork) with a consistent modern C99 coding style, building as Watcom IDE projects (`.wpj` / `.tgt`). Both real-mode 16-bit (Watcom IDE system identifier `de6en`) and DOS/4GW 32-bit (`dr2en`) variants exist for the larger demos.

On top of that port, it adds original work the book didn't have — chiefly an **IPX LAN-multiplayer layer for Starblazer** (32-bit, combined with the VESA port and the AI into one definitive build, `blazerx/`; see [Networking](#networking-lan-play)) and **runtime SVGA (VESA) support merged directly into the mode-13h engine** (`black3`/`black4`, 32-bit only), with higher-resolution ports of the book's graphics demos (see [SVGA (VESA)](#svga-vesa)) — and patches a number of latent bugs in the original source, including some that only surface in two-player (see [Notable fixes](#notable-fixes-vs-the-book-code)).

## Build requirements

- **Open Watcom 2.0 beta** — the community fork from [github.com/open-watcom/open-watcom-v2](https://github.com/open-watcom/open-watcom-v2). The official 1.9 release does not have sufficient C99 support.
- **DOSBox-X** to run the binaries (see [Running under DOSBox-X](#running-under-dosbox-x) for why). 32-bit builds run under DOS/4GW (bundled with Watcom).
- Watcom IDE for opening `.wpj` project files.

The toolchain assumes 16-bit medium model for the original chapter demos and 32-bit flat model for the `ch*_32/` variants.

## Repository layout

```
blackart3d/
├── README.md                            # this file
├── .gitignore                           # excludes build artifacts (.obj, .exe, .lst, .map, etc.)
├── .gitattributes
├── engine/                              # shared engine library
│   ├── black3.{c,h}                     # video / palette / BIOS / mode 13h + SVGA (VESA) + Mode Z
│   ├── black4.{c,h}                     # double buffer, bitmaps, sprites (incl. tinted), PCX — mode 13h + SVGA
│   ├── black5.{c,h}                     # keyboard ISR, mouse, joystick
│   ├── black6.{c,h}                     # DIGPAK / MIDPAK sound and music
│   ├── black8.{c,h}                     # timer
│   ├── black9.{c,h}                     # serial / modem
│   ├── black11.{c,h}                    # 3D engine — chap11 snapshot
│   ├── black15.{c,h}                    # 3D engine — chap15 snapshot (BSP + Mode-Z)
│   ├── black17.{c,h}                    # 3D engine — chap17 snapshot (mode 13h pipeline)
│   ├── black18.{c,h}                    # 3D engine — chap18 snapshot (krk additions)
│   ├── dpmi.{c,h}                       # 32-bit only: DPMI bridge to real-mode TSRs + DOS memory
│   ├── ipx.{c,h}                        # LAN multiplayer over IPX (16- and 32-bit; DOSBox-X ipxnet / real IPX LAN)
│   ├── *.asm                            # 16-bit assembly inner loops
│   └── *_32.asm                         # 32-bit flat-mode assembly inner loops
├── chap02/ – chap18/                    # 16-bit demos for each chapter
├── ch09_32, ch14_32, ch15_32,           # 32-bit DOS/4GW project variants
│   ch16_32, ch17_32, ch18_32/
├── blazerx/                             # 32-bit VESA Starblazer + IPX LAN play + AI (the definitive build)
├── vbe/                               # 32-bit SVGA (VESA) ports of the book's graphics demos
│   └── chap03/, …                      #   one subfolder per chapter, mirroring chap*/
├── audio/                               # DIGPAK / MIDPAK driver TSRs and patch files
└── exp_font/                            # utility — dumps the BIOS 8x8 ROM font to font.bin
                                         # (needed by 32-bit builds since flat mode can't reach 0xF000:FA6E)
```

## Engine modules

The engine is split across multiple `black*` modules following the book's chapter progression. Each chapter's snapshot of the engine is preserved (chap11, chap15, chap17, chap18) because the book's APIs and structs evolve as it goes — a later chapter's engine isn't always a strict superset of an earlier one.

| Module | Purpose | Key APIs |
|---|---|---|
| `black3` | VGA mode 13h always; SVGA (VESA VBE, 32-bit only, at any resolution/bpp) when built with `-dVBE_SUPPORT` — that build's mode-13h/VESA state is runtime (`DisplayWidth`/`DisplayHeight`/`DisplayPitch`/`DisplayBpp`), not fixed per-resolution, so one `VBE_SUPPORT` binary can switch resolution/bpp while running; a build without `VBE_SUPPORT` gets the fixed-320×200×8 engine instead, with none of that runtime state or VESA capability compiled in at all (see [16-bit vs. 32-bit builds](#16-bit-vs-32-bit-builds) and [SVGA (VESA)](#svga-vesa)) | `setGraphicsMode`, `setGraphicsModeVesa`, `setModeZ`, `writePalette`, `lineH/V`, `printString` |
| `black4` | Double buffering, bitmaps, PCX loading (256-color indexed or 24-bit true color), sprites (incl. tinted, for palette-free shield/glow effects), parallax layers, screen transitions — all bpp/resolution-generic via `black3`'s runtime state | `createDoubleBuffer`, `pcxLoad`, `spriteInit`, `spriteDraw`, `spriteDrawTinted` |
| `black5` | Keyboard ISR (custom INT 9 handler), mouse via INT 33h, joystick port | `keyboardInstallDriver`, `KeyboardState[]`, `mouseControl`, `getScanCode` |
| `black6` | DIGPAK (.VOC) sound and MIDPAK (.XMI) music via INT 66h TSRs | `soundLoad`, `soundPlay`, `musicLoad`, `musicPlay` — works in 16-bit and 32-bit (32-bit bridges through `engine/dpmi.c`) |
| `dpmi`   | 32-bit only: DPMI INT 31h bridge to real-mode TSRs and DOS conventional memory | `dpmiRealModeInt`, `dpmiAllocDos`, `dpmiFreeDos`, `dpmiGetVector`, `dpmiAllocRealCallback`, `dpmiFreeRealCallback` |
| `ipx`    | LAN multiplayer over IPX (DOOM-style) — broadcast peer discovery, datagram send/recv via polled ECBs. 16-bit real mode and 32-bit DOS/4GW (via the DPMI bridge); runs under DOSBox-X `ipxnet` or a real IPX LAN | `netInit`, `netPoll`, `netHost`, `netJoin`, `netRole`, `netSendToPeer`, `netRecv` |
| `black8` | BIOS timer queries and PIT reprogramming | `timerQuery`, `timerProgram` |
| `black9` | Serial port ISR and modem AT command driver | `serialOpen`, `modemControl`, `makeConnection`, `waitForConnection` |
| `black11` | First full 3D engine: object loader (.PLG), matrix math, shading | `plgLoadObject`, `rotateObject`, `removeBackfacesAndShade`, `drawPolyList` |
| `black15` | Adds BSP trees, Mode-Z renderer, Z-sorted painter's algorithm | `buildBspTree`, `bspTraverse`, `drawPolyListZ` |
| `black17` | Mode 13h pipeline with assembly inner loops, DOS/4GW additions | `fillDoubleBuffer32`, `displayDoubleBuffer32`, `triangleAsm` |
| `black18` | Chap17 engine + line clipper, wireframe, force-color shading | `clipLine`, `drawLine`, `drawObjectWire`, `removeBackfacesAndShade(obj, forceColor)` |

## Chapter demos

### 16-bit demos (every chapter)

| Chapter | Demo | Description |
|---|---|---|
| 2 | `guess.c`, `light.c` | First C programs — number guessing, color cycling |
| 3 | `light.c`, `mode13.c`, `modez.c` | Video mode setup, mode 13h, Mode Z (320×400) |
| 4 | `pcxdemo.c`, `alien.c`, `worms.c`, `speed.c`, `spheres.c` | Bitmaps, PCX images, double buffering benchmarks |
| 5 | `keytest.c`, `joytest.c`, `mousetst.c`, `ship.c` | Input device tests |
| 6 | `digidemo.c`, `mididemo.c` | Sound (.VOC) and music (.XMI) playback |
| 7 | `critters.c`, `floater.c`, `jumper.c`, `lockon.c`, `lostnspc.c` | Sprite animation demos |
| 8 | `timer.c`, `vblank.c`, `jelly.c`, `volcano.c` | Timer ISR, vertical blank, palette animation |
| 9 | `term1.c`, `term2.c`, **`blazer.c`** | Modem terminal builds up to **Starblazer** — modem multiplayer game |
| 11 | `linedemo.c`, `wiredemo.c` | First 3D wireframe demos |
| 12 | `tridemo.c`, `solidemo.c`, `gourdemo.c`, `textdemo.c` | Flat-shaded, Gouraud, textured triangles |
| 13 | `sol2demo.c` | Solid-shaded multiple objects |
| 14 | `objects.c` | Multiple PLG object loading and rendering |
| 15 | `bspdemo.c`, `sortdemo.c`, `solzdemo.c`, `zdemo.c` | BSP tree, Z sorting, Mode Z renderer |
| 16 | **`voxel.c`**, **`voxtile.c`**, **`voxopt.c`** | Comanche-style heightmap terrain (3 variants) |
| 17 | **`blaze3d.c`** | **Starblazer 3-D** — first-person 3D space shooter |
| 18 | **`krk.c`** | **Kill or Be Killed** — full 3D action game with HUD |

### 32-bit DOS/4GW demos

The larger demos (and chapters that benefit from flat-mode memory) have parallel 32-bit Watcom projects in `ch*_32/`:

| Directory | Project | Source | Notes |
|---|---|---|---|
| `ch09_32/` | `blazer32` | `../chap09/blazer.c` | Starblazer (32-bit DOS/4GW build) |
| `ch14_32/` | `obj_32` | `../chap14/objects.c` | |
| `ch15_32/` | `bsp_32`, `sort_32`, `solz_32`, `zdemo_32` | `../chap15/*.c` | |
| `ch16_32/` | `vox_32`, `voxt_32`, `voxo_32` | `../chap16/{voxel,voxtile,voxopt}.c` | |
| `ch17_32/` | `blz3d_32` | `../chap17/blaze3d.c` | Includes `_32.asm` rasterizer files |
| `ch18_32/` | `krk_32` | `../chap18/krk.c` | Kill or Be Killed — same `_32.asm` rasterizer set as `ch17_32` |
| `blazerx/` | `blazerx` | `blazerx/blazerx.c` (own copy) | **The definitive Starblazer**: 32-bit VESA (640×480) + **IPX LAN play** (via the DPMI bridge, `engine/ipx.c`) + single-player AI. See [Networking](#networking-lan-play). |
| `vbe/chap03/` | `svga`, `svga_m16`, `svga_l32`, `light` | `chap03/{mode13,light}.c` (`-dVBE_SUPPORT` [+`_M16`/`_L32`]) | **SVGA (VESA)** ports of the chap03 video-mode demos at 640×480×256, 800×600×16bpp, and 1024×768×32bpp — all three compiled from `chap03/mode13.c`, compiled from the same source as the book original. See [SVGA (VESA)](#svga-vesa). |
| `vbe/chap04/` | `pcxdemo`, `worms`, `alien`, `speed`, `spheres` | `chap04/*.c` (`-dVBE_SUPPORT`) | SVGA ports of all five chap04 demos: PCX loading, sprite animation, parallax scrolling, palette cycling. `spheres` swaps the book's Mode Z page-flip for software double buffering (VESA has no Mode Z equivalent). |
| `vbe/chap05/` | `keytest`, `ship`, `mousetst`, `joytest` | `chap05/*.c` (`-dVBE_SUPPORT`) | SVGA ports of all four chap05 input demos: keyboard ISR (palette cycling), keyboard ship (A/D rotate, W/S thrust), mouse hammer+ant, joystick ship. |
| `vbe/chap07/` | `critters`, `floater`, `jumper`, `lockon`, `lostnspc` | `chap07/*.c` (`-dVBE_SUPPORT`) | SVGA ports of all five chap07 sprite-animation demos: pixel-critter convergence, terrain-following speeder, pattern-motion spider, tracking/evasion alien+creature (keyboard, black5.h), random-wander ship with vapor trail. |
| `vbe/chap08/` | `jelly`, `volcano` | `chap08/*.c` (`-dVBE_SUPPORT`) | SVGA ports of the two graphical chap08 demos: jellyfish sprite animation (real timer ISR in the mode-13h build; position updated in the main loop under `VBE_SUPPORT`, since `_asm sti` GPFs under DOS/4GW), volcanic cinder particle simulation. |
| `vbe/chap09/` | `blazer` | `chap09/blazer.c` (`-dVBE_SUPPORT`) | SVGA port of **Starblazer** at 640×480, book-faithful (real modem/serial link, no IPX) — the first full-game (not static demo) port; see [SVGA (VESA) § Starblazer and the camera pattern](#starblazer-and-the-camera-pattern). `blazerx/` (above) forks from this file and adds IPX + is the recommended way to actually play. |

## Coding style

The original book code uses snake_case names, K&R braces with separate-line opens, and `// end Foo` trailing comments. This port consistently uses:

- **Types**: `PascalCase` with a `*Ptr` companion typedef. `Sprite`, `SpritePtr`. `Point3D`, `Point3DPtr`.
- **Functions**: `lowerCamelCase`. `spriteInit`, `rotateObject`, `pcxGetSprite`.
- **Globals**: `PascalCase`. `ViewPoint`, `ColorPalette3D`, `KeyboardState`.
- **Local variables / parameters**: `lowerCamelCase`. `currPoly`, `tempX`.
- **Struct fields**: `lowerCamelCase`. `.worldPos`, `.currFrame`, `.numPolys`.
- **Constants / `#define`s**: `UPPER_SNAKE_CASE`. `MAX_OBJECTS`, `SHADE_BLUE`.
- **Indent**: 4 spaces. K&R braces. `*` attached to the type, not the variable. Uppercase `FAR` macro.
- **No trailing `// end Foo` comments.** No `///////` separator lines between functions.

## Notable fixes vs. the book code

The port surfaced a number of latent bugs in the original 1995 source. Most were dormant in 16-bit mode but easy to trigger. Each was fixed in this port; the C-level logic of the affected functions is otherwise faithful to the book.

### Critical (caused crashes or memory corruption)

- **`loadPaletteDisk` buffer overflow** ([engine/black11.c](engine/black11.c), [engine/black17.c](engine/black17.c)) — `fscanf("%d %d %d", &color.red, &color.green, &color.blue)` where the targets are `unsigned char`. `%d` writes `sizeof(int)` bytes per argument — 2 bytes in 16-bit (already corrupting adjacent fields), 4 in 32-bit (9-byte stack overwrite per call × 256 iterations). The fix uses the C99 `%hhu` length modifier so each scan writes exactly one byte.

### Bounds checks (caused crashes on edge inputs)

- **`plgLoadObject` PLG loader** ([engine/black11.c](engine/black11.c), [engine/black17.c](engine/black17.c)) — added explicit bounds checks on `totalVertices`, `totalPolys`, and per-polygon `numVertices` against the engine's `MAX_*` limits. Original would happily write past fixed-size arrays if a hand-edited PLG declared more vertices than allowed.
- **`fscanf("%s", ...)` width limits** — added `%31s` / `%63s` modifiers where the destination is a fixed-size stack buffer (PLG object names, modem init strings). Prevents stack buffer overflow from a long token in a hand-edited file.
- **`lineVDb` in voxel demos** ([chap16/voxel.c](chap16/voxel.c), [chap16/voxtile.c](chap16/voxtile.c)) — clipped `y1`/`y2` to `[0, 199]` before computing the write offset into `DoubleBuffer`. At low altitudes with high `MountainScale`, the computed `top` could go significantly negative, producing a write address well before the buffer.

### Algorithmic / state

- **`FP_SCALE` typo** ([engine/black17.h](engine/black17.h)) — defined as `65526L` instead of `65536L`. Comment even said `2^16 = 65536`. No active C call sites used the macro (the asm uses a literal `65536.0`), but the value was off by 10. Fixed.
- **`timeDelay` missing `volatile`** ([engine/black3.c](engine/black3.c)) — BIOS tick counter at `0x46C` was read in a busy-wait loop without a `volatile` qualifier. Under release compiler flags the read could be hoisted out of the loop, hanging forever. Added `volatile`, switched `abs` to `labs` since the counter is `long`.
- **Voxel demos U/D/F/C key clamps** ([chap16/voxel.c](chap16/voxel.c), [chap16/voxtile.c](chap16/voxtile.c), [chap16/voxopt.c](chap16/voxopt.c)) — pressing the height (`U`/`D`) or focal-length (`F`/`C`) keys far enough would drive `PlayZ` into the `[51, 100]` range where the perspective formula `playDist * playZ / (playZ - rowInv)` divides by zero, or grow the product past 16-bit `int` range. Added clamps `PlayZ ∈ [110, 1000]` and `PlayDist ∈ [10, 1000]`, and promoted the multiplication to float to prevent integer overflow.
- **`voxopt.c` stale ray-length table** ([chap16/voxopt.c](chap16/voxopt.c)) — the book's optimization precomputed a per-row ray-length table once at startup, then the U/D/F/C keys updated `PlayZ`/`PlayDist` but the table was never refreshed. Pressing the keys had no visual effect. Added a per-frame `rebuildRayLengths()` call so the keys actually do something.
- **`light.c` WEST wrap typo** ([chap02/light.c](chap02/light.c), [chap03/light.c](chap03/light.c)) — book bug inherited from `MSC/CHAP_2/LIGHT.C`: in the WEST case of the direction switch, `if (--playerX < 0) playerY = 319;` set the *Y* coordinate to 319 (off-screen) instead of wrapping *X* to 319. Turning left from the start sent the light cycle off the visible area and made the demo look frozen. Fixed to `playerX = 319;`.

### Starblazer two-player (remote ship)

Three related bugs in the remote-ship code path ([chap09/blazer.c](chap09/blazer.c)). All were dormant in the original: a fourth bug (the energy one below) kept the remote ship from ever thrusting, which masked the other two. They only surface once two machines are linked and the remote ship can actually move — so they affect both the modem and the IPX builds. The remote-ship logic is meant to mirror the player-ship logic exactly (input-lockstep re-simulates both ships on both machines), so each fix restores that symmetry.

- **Remote energy zeroed every frame** — the remote's per-frame energy upkeep read `if (RemotesCloak == 1) RemotesEnergy--; else RemotesEnergy = 0;`, so any frame the remote wasn't cloaked its energy was forced to 0. Thrust, fire, and cloak are all gated on `RemotesEnergy > 0`, so the remote ship could never accelerate — it just coasted and slid out of sync with the peer actually flying it. Fixed to mirror the player's upkeep (decrement, floor at 0).
- **Thrust-frame draw corrupted the heading** — drawing the remote ship with engines on did `currFrame += 16` (to pick the thrust sprite) but then drew the sprite twice and subtracted 16 *twice* — an extra `-= 16` with no matching `+= 16` — leaving `RemotesShip.currFrame` at F−16. That negative index into `MotionDx[]`/`MotionDy[]` sent the remote's thrust off in a garbage direction. Fixed to the player's balanced `+= 16` / draw / `-= 16`.
- **Engine register collided with the shield register** — `REMOTES_ENGINE_REG` was defined as `241`, the same palette register as `REMOTES_SHIELD_REG`. Once the remote could thrust, its engine-flicker wrote the (white) engine color into the shield palette entry and lit up a phantom white shield outline. Fixed to `243`, the distinct register the remote ship's art uses (240/241/242/243 = player-shield / remote-shield / player-engine / remote-engine).

### Modem connect (Starblazer / term demos)

Two robustness fixes in the modem driver ([engine/black9.c](engine/black9.c)), which the stock modem multiplayer (and the `term1`/`term2` demos) use:

- **`CONNECT`-speed matching too narrow** — `modemResult` exact-matched the modem's response against a fixed list of result strings (`CONNECT`, `CONNECT 1200`, `CONNECT 2400`, `CONNECT 9600`, ...), so a modem that reported any *other* connect speed read as `MODEM_ERROR` and the game treated a successful call as a failure ("COMM PROBLEM"). This bit DOSBox-X's emulated modem, which always reports a hardcoded `CONNECT 57600`, and would equally bite a fast real modem. The line speed is informational only (the program drives the UART at its own DTE rate), so the fix accepts **any** `CONNECT…` line as `MODEM_CONNECT` via a prefix check — exactly how robust comm software handled it. On real hardware the book relied on the modem being configured (e.g. `ATX0` for a bare `CONNECT`) via its init string; DOSBox-X's softmodem doesn't implement those result-code commands, so the prefix check is the portable fix.
- **`waitForConnection` fell off the end** — if the first response wasn't `RING`, the function reached its closing brace with no `return`, yielding an undefined result the caller would then act on. Added an explicit `return result;` so a timeout / stray response / user-abort is reported cleanly.

## 16-bit vs. 32-bit builds

### What changes between builds

- **Memory model**: 16-bit medium model (separate code/data segments, FAR pointers explicit) vs. 32-bit flat (one 4 GB linear address space).
- **`int` size**: 16-bit (2 bytes) vs. 32-bit (4 bytes). Several book bugs were latent in 16-bit because `int` arithmetic stayed in 16-bit range; they manifested when `int` grew to 32 bits and the same overflow patterns produced larger out-of-range values.
- **Far pointers**: `FAR` is a real keyword in 16-bit, a no-op macro in 32-bit. Pointer construction via `_FP_SEG`/`_FP_OFF` doesn't work in 32-bit; the code uses `MK_FP` from `<dos.h>` instead.
- **VGA buffer**: `0xA0000000` (real-mode segment:offset) in 16-bit vs. `0xA0000` (flat linear address) in 32-bit.
- **ROM character set**: at `0xF000:FA6E` in real mode. In 32-bit flat mode that address is unreachable — the `exp_font/` utility extracts the font to `font.bin` (plus scaled `font16.bin`/`font24.bin`/`font32.bin` variants for `VBE_SUPPORT` builds). A plain 32-bit build loads the fixed 8×8 `font.bin` via `initRomCharSet()`; a `VBE_SUPPORT` build loads any of the sizes via `loadFontSet(filename, size)` instead (see [vbe/PORTING.md](vbe/PORTING.md)).
- **Inner-loop rasterizers**: 16-bit uses `.asm` files (`fpdiv`, `qcpy`, `tri_fp`, etc.); 32-bit uses parallel `_32.asm` files (`fpdiv_32`, `qcpy_32`, `tri_fp_32`). The 32-bit asm explicitly preserves callee-saved registers via Watcom's `USES` clause.
- **Sound / music**: DIGPAK and MIDPAK are real-mode TSRs hooked via INT 66h. Calling them from 32-bit DOS/4GW requires a DPMI bridge — `engine/dpmi.c` provides the wrappers (`dpmiRealModeInt` for INT 31h func 0300h to simulate the real-mode interrupt, `dpmiAllocDos` for INT 31h func 0100h to allocate DOS conventional memory shared between real and protected mode, `dpmiGetVector` for func 0200h to read the real-mode IVT). The 32-bit branch of `engine/black6.c` allocates VOC / XMIDI / SndStruc storage in DOS memory, builds real-mode `seg:off` FAR pointers inside the SndStruc by hand, and dispatches each INT 66h call through DPMI. Two 32-bit-specific subtleties:
  - **`SndStruc` is `#pragma pack(1)`'d** in `engine/black6.h` — DIGPAK is real-mode code expecting the 12-byte unpacked layout. Watcom 32-bit's default `-zp8` would otherwise insert 2 bytes of padding before `isPlaying` and shift `frequency` to offset 12, causing playback at the wrong sample rate. A `SndStrucPackedCheck` typedef makes the build fail if anyone removes the pragma.
  - **TSR detection probes the signature**, not just the vector. `audioPresent()` reads the INT 66h vector via DPMI 0200h, walks **6 bytes back** from the handler entry, and verifies the ASCII `"KERN"` (DIGPAK) or `"MIDI"` (MIDPAK) signature — matching what DIGPAK's own `CheckIn` does in [Ratcliff's DIGPLAY.ASM](https://github.com/jratcliff63367/digpak/blob/master/DIGPLAY.ASM). A non-DIGPAK TSR hooking INT 66h won't fool it.
- **The `DOS_32_BIT` macro** is defined by the 32-bit Watcom project files and gates the per-platform variant code.

### What stays the same

- All chapter `.c` files are *shared* between their 16-bit and 32-bit projects. The `.tgt` for each project just points to the same `chap*/foo.c`.
- The engine `.c` files are also shared.
- Compilation produces different `.exe` formats: MZ (real-mode 16-bit) vs. LE (Linear Executable for DOS/4GW). The LE binary has a bound MZ stub that locates and invokes `dos4gw.exe`.

## Building

Open the `.wpj` for the demo you want in Open Watcom IDE and choose Build → Make Target.

```
chap16/voxel.wpj            # 16-bit voxel terrain (Comanche-style)
ch16_32/voxo_32.wpj         # 32-bit optimized voxel
chap17/blaze3d.wpj          # 16-bit Starblazer 3-D
ch17_32/blz3d_32.wpj        # 32-bit Starblazer 3-D
chap18/krk.wpj              # 16-bit Kill or Be Killed
ch18_32/krk_32.wpj          # 32-bit Kill or Be Killed
blazerx/blazerx.wpj       # 32-bit VESA Starblazer with IPX LAN play + AI (the definitive build)
vbe/chap03/svga.wpj      # 32-bit SVGA (VESA) port of the chap03 mode-13h demo
...
```

Each chapter's project includes the necessary engine modules (`black3.c`, `black4.c`, ...). 32-bit projects also include the `_32.asm` files when needed.

The 16-bit projects produce ~50–100 KB MZ `.exe`s. The 32-bit projects produce a small MZ stub + the LE body, which needs the DOS/4GW extender at runtime — **DOSBox-X provides DOS/4GW built-in**, so no `dos4gw.exe` file is bundled (on real DOS you'd supply `dos4gw.exe` on the path).

## Running under DOSBox-X

**Use DOSBox-X, not vanilla DOSBox.** Vanilla DOSBox has incomplete emulation of the DOS System File Table (SFT) and a low default limit on simultaneously open file handles. Games like Starblazer and Kill or Be Killed open dozens of asset files (PCX backgrounds, VOC sound effects, XMI music, PLG models) at startup, and several of them keep file handles open across scenes. Under vanilla DOSBox this can cause apparent crashes or silent failures partway through a game's load sequence.

DOSBox-X reworked SFT emulation (dynamic allocation, per the DOSBox-X release notes) and is the more compatible host for any of the larger book demos. Get it from [dosbox-x.com](https://dosbox-x.com).

If for some reason you must use vanilla DOSBox, raise `files=` in your `dosbox.conf` to its maximum value (255) — but DOSBox-X is the recommended path.

## Audio

The book uses two commercial 1990s sound libraries:

- **DIGPAK** (John Ratcliff, The Audio Solution) — digital sound effects playback from `.VOC` files
- **MIDPAK** — XMIDI music playback to AdLib, Sound Blaster, Roland MT-32, General MIDI, and other supported synths

Both are real-mode DOS TSRs that must be loaded *before* the game starts. The drivers ship in [audio/DRIVERS/](audio/DRIVERS/) (sound-card-specific drivers, MIDPAK variants for different synths) and patch sets in [audio/PATCHES/](audio/PATCHES/).

To enable audio for a 16-bit demo that uses sound (chap06 demos, chap09's Starblazer):

1. **Generate the runtime drivers (one-time, in [audio/DRIVERS/](audio/DRIVERS/)).** The repo ships the canonical *source* drivers — the card-specific DIGPAK drivers (`SBPRO.COM`, `SB16.COM`, `ADLIB.COM`, …), the music TSR `MIDPAK.COM`, the synth drivers (`SBFM.ADV`, …) and patch sets (`FAT.OPL`, …) — plus the DIGPAK/MIDPAK setup tools. It does **not** ship the per-machine *configured* drivers those tools produce. Run:
   ```
   SETUP.BAT
   ```
   It runs `SETD`, which detects your sound card and writes **`SOUNDRV.COM`** (a copy of your card's digital driver with the right port/IRQ/DMA baked in; see [the DIGPAK/MIDPAK context below](#why-a-game-shipped-its-own-sound-drivers-digpakmidpak-in-context) for why that matters), then `SETM`, which writes the music driver's synth/patch files (e.g. `MIDPAK.ADV` ← `SBFM.ADV`, `MIDPAK.AD` ← `FAT.OPL`) for `MIDPAK.COM` to load. These generated files (`SOUNDRV.COM`, `MIDPAK.ADV`, `MIDPAK.AD`) are `.gitignore`d, like the other build artifacts.
2. Load the drivers before starting the game:
   ```
   SOUNDRV.COM
   MIDPAK.COM
   ```
3. Run the game. Pass any sound/music command-line switches the game expects (Starblazer takes `S` and `M` — e.g. `blazer s m` for both digital effects and music).

The TSRs hook INT 66h; the engine calls them via inline assembly stubs in `engine/black6.c`.

### Music file format

MIDPAK plays **XMIDI (`.XMI`)** files only — it does not play standard `.MID` files. The only `.XMI` files kept in this repo are the book's **own** game soundtracks: `BLAZEMUS.XMI` (Starblazer, in `chap06/`, `chap09/`, `vbe/chap09/`, and `blazerx/`) and `KRKMUS.XMI` (Kill or Be Killed, in `chap18/`). To exercise `chap06/mididemo.c`, enter `BLAZEMUS.XMI` at the prompt, or drop in any `.XMI` file you have the rights to and enter its name.

32-bit DOS/4GW builds use the DPMI bridge in `engine/dpmi.c` to reach the real-mode TSRs. The same `SOUNDRV.COM` / `MIDPAK.COM` setup applies — load them before launching the 32-bit executable, identical to the 16-bit flow. The bridge transparently allocates DOS conventional memory for the VOC and XMIDI buffers via INT 31h func 0100h and dispatches each INT 66h call through INT 31h func 0300h.

### Why a game shipped its own sound drivers (DIGPAK/MIDPAK in context)

DOS had **no OS-level sound API**. A game couldn't ask the operating system to "play this sound" — it had to talk to the sound hardware itself, and the hardware was wildly fragmented (Sound Blaster, AdLib, Gravis UltraSound, Pro Audio Spectrum, Roland MT-32, …), each with different ports, IRQ/DMA, and register layouts. So games bundled a **sound engine**, and a market grew for licensable middleware that abstracted the cards behind a single interface — exactly as VESA's VBE did for SVGA. DIGPAK/MIDPAK's abstraction is the **INT 66h** API: a card-specific `.COM` driver (e.g. `SOUNDRV.COM`) implements it, and the game makes the same calls regardless of card. The driver has the port/IRQ/DMA baked in, set at install time by DIGPAK's `SETD`/`SETM` detection tools — which is why an emulator whose sound card sits on a different IRQ than the driver was configured for stays silent (DOSBox-X's SB16 is IRQ 7).

DIGPAK and MIDPAK were written by **John Ratcliff** (The Audio Solution). The kit's own licensee list — shipped in this repo at [audio/DRIVERS/README.PRN](audio/DRIVERS/README.PRN) — shows the customer base was mostly RPGs, wargames, and edutainment: SSI's *Gold Box* AD&D titles, Activision's *Return to Zork* and *MechWarrior 2*, Trilobyte's *The 7th Guest*, Interplay's *Battle Chess 4000*, the Humongous/edutainment catalogue (*Putt-Putt*, *Oregon Trail Deluxe*), and others. Notably, **MIDPAK's music engine is built on John Miles' AIL** (Audio Interface Library) — which is why the `.ADV` synth drivers in `audio/DRIVERS` carry a "Copyright Miles Design" header and why MIDPAK plays the AIL-native `.XMI` format.

It was one of several such systems. For the wider picture: **Miles AIL / Miles Sound System** (John Miles) was the dominant commercial choice (Westwood, MicroProse, Blizzard, later Origin); **iMUSE** was LucasArts' in-house engine; **DMX** (Paul Radek) powered id's DOOM/Heretic/Hexen; and **HMI Sound Operating System** drove Descent, Daggerfall, and Fallout. The book's engine (`engine/black6.c`) targets DIGPAK/MIDPAK specifically.

## Single-player AI

**AI is a `blazerx`-only feature.** The book's **Play Solo** menu item originally dropped you into the arena alone, and the plain `blazer`/`blazer32`/`ch09_32`/`vbe/chap09` builds all keep that book-faithful behavior — the enemy ship just sits inert at its spawn as a stationary target for practice, no link setup needed. Only **`blazerx/blazerx.c`** (32-bit VESA + IPX; see [Networking](#networking-lan-play)) adds a small **AI opponent** that hunts you down, lines up shots, fires, and raises its shields against incoming fire, so Solo is an actual single-player mode there.

The implementation reuses the game's existing two-player machinery rather than bolting on a parallel code path. In Starblazer the "remote" (enemy) ship is already simulated *locally* on each machine from a one-byte input of `REMOTE_*` action flags (turn left/right, thrust, fire, shields, cloak); in head-to-head play that byte arrives over the link each frame (`serialReadWait()`), and the same code re-simulates the remote ship, its collisions, death, and rendering. The AI simply **synthesizes that input byte** instead of reading it from the network — every frame `computeRemoteAi()` (in [blazerx/blazerx.c](blazerx/blazerx.c)) decides which "keys" the enemy presses, and the unchanged remote simulation does the rest. A single `AiEnabled` flag swaps the input source and opens the collision/death paths that were previously gated on a live link.

The enemy runs a small **behavior state machine** that cycles between three modes. It mostly **circles you (Strafe)**, mixing in shorter aggressive pushes and the occasional break:

- **Strafe** *(the most common)* — circle the player at a held radius. When it has matched your speed it's free to face you, so it still fires while circling, but it's less relentless than a head-on push.
- **Hunt** — close to a short distance, then pace the player and fire. The aggressive mode; this is where the heaviest shooting happens.
- **Flee** — back off to reposition (holding fire), and where it retreats to conserve power when its *own* energy runs low.

**Smooth movement was the hard part, and the fix is the important bit:** the enemy steers its **velocity**, not its position. Each frame it computes a *target velocity* — the player's own velocity, plus a small per-behavior maneuver (toward you for Hunt, sideways for Strafe, away for Flee) — and only thrusts to correct the difference. Chasing a *position* with momentum is what made earlier versions spin, weave, and jerk: the bearing to a fast-moving player swings wildly, the ship overshoots and loops, and because the camera is locked to the player anything not matching the player's velocity lurches across the screen. Pacing the player's *velocity* fixes all three at once — the enemy glides, never builds runaway momentum, and (since it's then free to point at you) faces you and shoots. When its velocity already matches the target it simply faces the player, which is the common, calm-looking case. A hard backstop (`AI_RETURN_DIST` / `AI_LEASH_X` / `AI_LEASH_Y`) still drags it back if it's ever knocked out of view. Tuning lives in the `AI_*` knobs near the top of [blazerx/blazerx.c](blazerx/blazerx.c) — `AI_MANEUVER` (maneuver speed), `AI_VEL_TOL` (how loosely it matches before it stops correcting and shoots), the per-state distances, and the fire-rate set.

**Cloak blinds it.** The enemy can only track the player while the player is visible — the instant you engage your cloaking device it loses the lock. It remembers the last spot it saw you, steers toward *that* point, and holds fire until you decloak, so the cloak genuinely shakes its aim and lets you slip away and reposition (the view follows you, so it falls behind off-screen until you reappear). And on defense, it raises its own shields only against a missile that is both close *and* actually closing in on it — not any shot that happens to drift past — so its shielding reads as a deliberate reaction rather than random flicker.

**Shields are a committed cooldown ability** for both ships. Raising shields commits them for a fixed ~5.5-second window that can't be dropped early, after which they must recharge for another ~5.5 seconds before they can be raised again (`SHIELD_ON_TIME` / `SHIELD_COOLDOWN_TIME`). This replaces the old behavior where shields could be re-triggered the instant they expired — which, combined with the AI reacting to every passing shot, made the enemy's shields flicker on and off. Now both you and the enemy have to time a single shield window and then ride out the recharge.

The underlying mechanics are shared with the player and deliberately classic-arcade: it aims by picking, of the 16 ship headings, the one whose motion vector best matches the target direction (integer dot-product, no trig) and rotates one step per frame toward it — the same turn rate you have; and it thrusts and fires under the same velocity cap, energy costs, and space-friction the player ship obeys. All the tuning lives in named `AI_*` `#define`s at the top of [blazerx/blazerx.c](blazerx/blazerx.c) (fire range, cooldown, max shots in flight, thrust stand-off, shield reaction), so the difficulty is a one-line change.

Because the AI ship is a normal remote ship, it is fully part of the world: it dies to your missiles and to asteroids, its kills end the match, it shows up on the scanner, and it takes the opposite ship type (Gryfon vs. Raptor) from the one you picked. To play, just choose **Play Solo** from the setup menu — no link setup, no second instance. `chap09/blazer.c`, `vbe/chap09/blazer.c`, and every other Starblazer variant deliberately do **not** carry the AI — it lives only in `blazerx/blazerx.c`, alongside the IPX code it shares its input-synthesis trick with.

## Networking (LAN play)

**`blazerx/`** is the definitive Starblazer build: 32-bit DOS/4GW, the VESA 640×480 port, **IPX LAN play**, and the single-player AI, all in one `.exe`. It gets **head-to-head play over a LAN** using **IPX** — the protocol real DOS multiplayer games (DOOM, Duke3D, Warcraft, Descent) actually used — in place of the book's null-modem / dial-up serial link.

`blazerx/blazerx.c` is a copy of the VESA port (`vbe/chap09/blazer.c`) with the IPX serial-link shim merged in: the game's `serialWrite`/`serialReadWait`/`makeConnection`/etc. call sites redirect to the IPX transport. It reaches the real-mode IPX entry through the DPMI bridge (INT 31h `0300h` to fetch it, `0301h` to far-call it), with ECBs/buffers in DOS conventional memory — the same mechanism `black3.c`'s `setGraphicsModeVesa` already uses to talk to the VBE BIOS. It's a 32-bit DOS/4GW build, and two `blazerx` instances pair with each other over IPX.

The plain `blazer`/`blazer32` builds (`chap09/`, `ch09_32/`) are unaffected — they stay the book-faithful mode-13h demo with real modem/serial networking, no IPX, no VESA.

### Requirements

**No packet driver, no TCP/IP stack, no DHCP** — IPX is self-contained. Under DOSBox-X just enable it:

```ini
[ipx]
ipx=true
```

Then bring up the IPX-over-UDP tunnel from the DOS prompt: `IPXNET STARTSERVER` on one instance and `IPXNET CONNECT <ip>` on the other — `127.0.0.1` works for **two instances on the same machine** (which the old NE2000/UDP path could never do). On real hardware, any working IPX setup (e.g. the AMD PCnet ODI stack, or `PCNTPK.COM` + `PDIPX.COM`) provides the same API. For internet play, relay with [ipxbox](https://github.com/fragglet/ipxbox).

### How it works

`engine/ipx.c` is a small IPX layer (16-bit real mode and 32-bit DOS/4GW), translated from id's DOOM `ipx/IPXNET.C` into Open Watcom:

- **Driver access** — the IPX entry point is obtained via `INT 2Fh`/`AX=7A00h` and invoked through `#pragma aux` wrappers that issue `call dword ptr [IPXEntry]` (a real-mode far-indirect call), the Watcom equivalent of DOOM's Borland thunk. Byte-exact 42-byte ECB and 30-byte IPX-header structs (verified against the DOOM/C&C definitions). In 32-bit builds the same real-mode entry is reached through the DPMI bridge instead (`0300h` to fetch it, `0301h` to far-call it), with ECBs/buffers in DOS conventional memory polled via the flat selector — so no DPMI `0303h` callback is ever needed.
- **Polled receive** — listen ECBs are posted with `ESRAddress = 0` (no async upcall); `netPoll()` scans each ECB's `InUseFlag`, copies completed packets into a queue, and re-arms. No real-mode callback, so none of the interrupt-time/stack fragility of a DPMI receive callback.
- **App protocol** — each IPX payload is tagged `[B1]` (discovery beacon) or `[B2][len][bytes]` (game datagram); the explicit length keeps the byte stream exact regardless of frame padding. Game data is only accepted from the paired peer's node; the driver loops our own broadcasts back, so we ignore packets from our own node.
- **Discovery** — host (`netHost`) and joiner (`netJoin`) each broadcast a role beacon a couple of times a second until they hear the complementary role, then pair: **host = Master, joiner = Slave**. `GAME_LINKING` applies the result via `netRole()`.

### Transport shim

The game's serial call sites are left verbatim and permanently redirected onto the IPX transport in `blazerx/blazerx.c`: `serialWrite` / `serialReadWait` / `makeConnection` / `serialFlush` / etc. become an IPX byte-FIFO (see the "Serial-link compatibility shim" block right before `main()`). Starblazer's lockstep protocol (write a byte, then a blocking read, every frame) maps to **flush-on-read** — each read ships pending writes as one datagram, then pumps `netPoll()` until the peer's datagram arrives. A tiny stop-and-wait sequence/ack layer rides on top so a lost datagram self-heals instead of desyncing the lockstep, with a timeout so a vanished peer degrades instead of hanging.

### Playing

Bring up IPX (above) and run `blazerx.exe` on both instances — or just use the ready-made configs below, which do it for you. From the setup menu one player picks **Wait for Connection** (host → Master) and the other picks **Make Connection** (joiner → Slave); the "phone number" prompt is ignored under IPX, so just press Enter. They auto-discover, exchange RNG seed + ship type, and drop into the game.

### Ready-to-run DOSBox-X configs (`dosbox/`)

The [dosbox/](dosbox/) folder has paired config files for every two-player demo. Each one launches a single instance: it sets up the link (IPX or modem), mounts the right build directory as `C:`, and **auto-runs the program** — so a two-player test on one PC is just two launches. The configs use **relative paths, so run them from the repository root** (DOSBox-X resolves `mount` against the shell's working directory — there's nothing machine-specific to edit). Start the **host / server / answer** side first:

```
dosbox-x -conf dosbox/blazerx-host.conf
dosbox-x -conf dosbox/blazerx-join.conf
```

(Run with the repo root as the working directory so the relative `mount` resolves. If `dosbox-x` isn't on your PATH, use its full path, e.g. `"C:\DOSBox-X\dosbox-x.exe" -conf dosbox\blazerx-host.conf`. On PowerShell, from the repo root: `Start-Process "C:\DOSBox-X\dosbox-x.exe" -ArgumentList '-conf','dosbox\blazerx-host.conf' -WorkingDirectory .` opens a window without blocking the shell.)

| Demo | Link | Configs — launch ① then ② | In-game |
|---|---|---|---|
| **SVGA demos** | — | `svga-demos.conf` (single instance) | Mounts `vbe\chap03` as `C:`; type `svga`, `svga_m16`, or `svga_l32` (all three compile from `chap03/mode13.c` — see [vbe/PORTING.md § VBE_DEMO_L32 / VBE_DEMO_M16](vbe/PORTING.md)) |
| **joytest** (chap05) | — | `joytest.conf` (single instance) | Configures FCS joystick (`joysticktype=fcs`); mounts `vbe\` as `C:`; `cd chap05`, then `joytest` |
| **blazerx** (32-bit, VESA) | IPX | `blazerx-host.conf` · `blazerx-join.conf` | ① Wait for Connection  ② Make Connection (Enter at the number prompt) |
| **blazer** (16-bit) | modem | `blazer-answer.conf` · `blazer-dial.conf` | ① Wait for Connection  ② Make Connection → dial `5551234` |
| **blazer32** (32-bit) | modem † | `blazer32-answer.conf` · `blazer32-dial.conf` | same as blazer |
| **term1** | null-modem | `term1-server.conf` · `term1-client.conf` | each window: COM `1`, then type to chat |
| **term2** | modem | `term2-answer.conf` · `term2-dial.conf` | ① COM `1` → menu `2`  ② COM `1` → menu `1` → dial `5551234` |

† 32-bit modem (`blazer32`) is **untested** — the verified 32-bit multiplayer path is `blazerx` (IPX). Every other row is confirmed working.

Notes:
- The configs use **paths relative to the repository root** (`mount c chap09`, etc.), so there's nothing machine-specific to edit — just launch them with the repo root as your working directory (see above). Works regardless of where the repo is checked out.
- **Audio is on the first instance only.** The host/answer config mounts [audio/DRIVERS/](audio/DRIVERS/) as `D:`, loads `SOUNDRV.COM` + `MIDPAK.COM`, and runs the game with `s m` (sound + music); the join/dial config runs **silent** so you don't get two overlapping soundtracks on one machine. (The `term1`/`term2` configs have no audio.) 32-bit audio goes through the DPMI bridge and is less exercised than 16-bit. **Note:** `SOUNDRV.COM` is not shipped — run `SETUP.BAT` (or `SETD`) once in `audio/DRIVERS` to generate it (see [Audio](#audio)); until you do, the host instance just starts without digital sound.
- Modem configs (and `term2`) share `dosbox/phonebook.txt`, which maps the dialed number `5551234` → `127.0.0.1:5000`. This mapping is **required** — the in-game number prompt accepts digits only (and caps at 12 characters), so you can't type an IP/port directly; `5551234` works only because the phonebook translates it. For two real machines, change the mapping to the answerer's LAN IP. **Path gotcha:** DOSBox-X resolves `phonebookfile` relative to the **config file's own folder** (`dosbox/`), *not* the working directory, so the configs must set `phonebookfile=phonebook.txt` (a bare filename) — a `dosbox/phonebook.txt` value silently fails to load and every dial then ends in `COMM PROBLEM`. (`mount`, by contrast, *is* resolved against the working directory, which is why you still launch from the repo root.)
- **Launch order matters** — the host/server/answer instance must be up first, or the other side gets `NO CARRIER` / can't pair.

### How the links work

The configs above wrap three different DOSBox-X transports; the conceptual picture, if you want to build your own or run across real machines:

- **IPX** (`blazerx`) — DOSBox-X's built-in IPX-over-UDP. `ipx=true`, then `IPXNET STARTSERVER` (host) and `IPXNET CONNECT <ip>` (join) bring up a virtual IPX network the game discovers peers on — no packet driver, DHCP, or TCP/IP stack. `127.0.0.1` works for two instances on one machine. The configs run those `IPXNET` commands for you.

- **Modem** (`blazer`, `blazer32`, `term2`) — DOSBox-X emulates a Hayes "softmodem": `serial1=modem` gives the guest a COM port that accepts AT commands, and a "phone call" (`ATDT…`) becomes a **TCP connection**. `listenport:` is the side that listens; `phonebookfile` maps the dialed digit-string to the listener's `host:port` (the game's number prompt only takes digits, so the phonebook is the only way to "dial" an address). Relies on the `black9.c` `CONNECT`-speed fix (see [Notable fixes](#notable-fixes-vs-the-book-code)) — DOSBox-X always answers `CONNECT 57600`.

- **Null-modem** (`term1`) — `serial1=nullmodem` is a raw serial cable over TCP, no modem/AT layer: one side `port:`-listens, the other `server:<ip> port:`-connects, and the link is live as soon as both are up (no dialing).

For two real machines instead of one PC, point the joining/dialing side at the host's LAN IP (in `phonebook.txt` for modem, or the `server:` field for null-modem) and make sure the listener's TCP port is reachable.

## SVGA (VESA)

Beyond the book's 320×200 mode 13h, the engine gains SVGA support through the **VESA BIOS Extensions (VBE)**, merged directly into `black3.c`/`black4.c` — but only in builds compiled with `-dVBE_SUPPORT`. In that build, the same `writePixel`/`spriteDraw`/`pcxLoad`/etc. that draw mode 13h also draw any VESA resolution/bpp the card supports, dispatching at runtime on a handful of globals (`DisplayWidth`/`DisplayHeight`/`DisplayPitch`/`DisplayBpp`) that every mode-set call updates. Without `-dVBE_SUPPORT`, none of that exists — the engine compiles to the book's original fixed-320×200×8 form, with no VESA capability, no runtime dispatch, and none of those globals present at all (see [vbe/PORTING.md](vbe/PORTING.md) for the full three-way `DOS_32_BIT`×`VBE_SUPPORT` build matrix). VESA support (`setGraphicsModeVesa`) additionally requires `DOS_32_BIT` — VBE has no 16-bit real-mode equivalent — but a `DOS_32_BIT` build without `VBE_SUPPORT` still gets the same fixed mode-13h-only engine as a 16-bit build, just with flat pointers.

**Why 32-bit only for VESA?** A 640×480×8 frame is ~300 KB. In 16-bit real mode a far pointer wraps at the 64 KB segment boundary, so a flat off-screen surface can't be addressed without huge pointers — which would force rewriting every primitive. 32-bit flat mode makes surfaces trivial, so VESA is built there.

**The performance trade-off is opt-in, not universal.** mode-13h's own pixel/line functions stay fast in a non-`VBE_SUPPORT` build because 320 is a compile-time constant that decomposes into two bit-shifts instead of a multiply — exactly the book's own code, zero dispatch overhead. Only a `VBE_SUPPORT` build pays for the flexibility: pitch becomes a runtime value there (since it might be a VESA mode's negotiated pitch), so every drawing call pays one runtime multiply and one runtime bpp `switch`. In practice this remains imperceptible even where it's paid: DOSBox-X's real bottlenecks are fixed BIOS-tick delays and blit bandwidth, not per-pixel branch/multiply cost — but the majority of demos, which never need VESA, no longer carry any of that cost or code at all.

A program can call `setGraphicsMode(GRAPHICS_MODE13)`, then `setGraphicsModeVesa(width, height, bpp)`, then back to mode 13h, all in one run — the mode-set functions are the only thing that changed; every drawing function already reads whichever mode is currently active. See [vbe/PORTING.md](vbe/PORTING.md) for the full API reference and the demo-porting recipe.

### Demos

The `vbe/` demos are **higher-resolution ports of the book's graphics demos** — each replicates a chapter demo at a VESA resolution instead of mode 13h, using the same engine and function names as the book original. They live in per-chapter subfolders mirroring the book's layout, each a Watcom IDE project (32-bit, `dr2en`). Ported so far: chap03 (mode setup, palette, primitives, light-cycle; also 800×600×16bpp and 1024×768×32bpp variants), chap04 (PCX loading, double buffering, parallax scrolling, palette cycling), chap05 (keyboard, mouse, and joystick input demos), chap07 (sprite animation), chap08 (jellyfish sprite animation, volcanic cinder particle simulation), and **chap09 — the full Starblazer game** (see below). Demos that draw larger text load a bigger `fontNN.bin` via `loadFontSet()` (`exp_font` generates `font16.bin`/`font24.bin`/`font32.bin` alongside the base 8×8 `font.bin`) — chap09's `blazefnt.pcx` is a rarer case, its own bitmap font sheet drawn via `Bitmap`/`bitmapGet`/`bitmapPut` instead.

### Starblazer and the camera pattern

chap09's `blazer.c` is the first VESA port of a full *game* rather than a static or looping demo — it has a scrolling world bigger than one screen (a player-locked window into a 2500×2500-unit universe), so the earlier chapters' "just double every screen-relative coordinate" recipe doesn't hold up: nothing distinguishes a screen-relative constant from a world-relative one just by reading it, and the port shipped for a while with ship speed, AI engagement distances, and effect positions all silently wrong by a factor of 2 because the doubling had been applied inconsistently.

The fix is a small **camera**: the entire simulation — every speed, AI distance, collision size, and world-object dimension — stays in the book's original units, unchanged from 1995. Exactly one transform, `screen = (world - windowOrigin) * CAM_ZOOM`, is applied at the existing world-to-screen mapping sites, with `CAM_ZOOM = 2` giving a 320×240-unit visible window (matching the book's own view size) on the 640×480 screen. Sprite bitmaps are pre-scaled ×2 at asset-creation time as before, so the camera only ever multiplies *positions*; the one place scaling doesn't fall out for free is single-world-pixel effects (missile bolts, nova spark cinders), which need small helper functions to draw/erase/save a `CAM_ZOOM`×`CAM_ZOOM` block instead of one raw pixel.

This is also a specific case of a general truth: rendering more screen pixels always costs more per-pixel work, camera or not — a 640×480 buffer is ~4.8× the pixels of the book's 320×200, and this engine's erase-under-yourself draw model (no full-frame redraw) means every moving object pays that multiplier every frame. The camera doesn't add that cost; it just applies it in one place instead of scattered by hand across ~30 call sites, which is what made the earlier scale bugs possible in the first place. See [vbe/PORTING.md § Coordinate constants & the camera pattern](vbe/PORTING.md) for the implementation details, and chap17/chap18 (both full 3D games with their own scrolling/first-person world) are natural future candidates for the same treatment if they get a VESA port.

**VRAM requirement for high-colour demos.** The default DOSBox-X S3 Trio64 has 4 MB VRAM. `svga_m16` (800×600×16bpp) uses ≈ 1.9 MB for two pages — fine. `svga_l32` (1024×768×32bpp) needs ≈ 6 MB for two pages, which exceeds the default. Use the ready-made config:

```
dosbox-x -conf dosbox\svga-demos.conf
```

This sets `machine=svga_s3`, `vmemsize=8`, mounts `vbe\chap03` as `C:`, and prints the available demo names. For everything else — DOSBox-X's default `machine=svga_s3` already provides VBE 2.0 with a linear framebuffer; mount the repo, `cd vbe/chap0N`, build the project, and run the `.exe`.

## Credits and license

- **Original code**: André LaMothe / Waite Group Press, 1995 (Black Art of 3D Game Programming book/CD)
- **Conversion to modern coding style + bug fixes**: this repository's contributor
- **AI assistance (Claude Opus)**: the later/larger chapters and the original extensions were done with help from Anthropic's Claude Opus — Starblazer 3-D (chapter 17), the voxel demos (chapter 16), Kill or Be Killed (chapter 18), the 32-bit DOS/4GW conversions, and the IPX LAN-multiplayer layer.

The original 1995 source was bundled with the book and CD. This port restructures it for modern toolchains; the underlying algorithms and architecture remain LaMothe's. If you want to learn how 3D rendering worked before GPUs, read the book; it's still findable used and the techniques are foundational.

### Licensing

This repository mixes work under **different terms**, so the licensing is layered:

- The **contributors' own work** — the coding-style conversion, the bug fixes, the 32-bit ports, the IPX/DPMI networking layer, the build files, the DOSBox-X configs, and this documentation — is released under the **MIT License** ([LICENSE](LICENSE)).
- The **original book code** remains © André LaMothe / Waite Group Press (rights now likely held by Pearson and/or the author). It is **not** relicensed here; this is an **unofficial, non-commercial, educational port**, and the maintainers will honor any takedown request from the rights holder. Please own a copy of the book.
- The bundled **sound drivers** are redistributable under their authors' own later grants — DIGPAK is MIT (John Ratcliff, 2021) and the AIL synth drivers are open-source freeware (John Miles, 2000). The Fat Man's General MIDI patches are retained with their required credit.

Full per-component details are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The book and its CD are digitally available on archive.org:
- https://archive.org/details/BlackArt3DEBook
- https://archive.org/details/BlackArtOf3DGameProgramming
