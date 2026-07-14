# VESA engine & demo porting guide

How the book's mode-13h graphics demos run at SVGA resolutions via the
merged `black3`/`black4` engine.

---

## Goal

Run the book's graphics demos at any VESA resolution/bpp the card supports
(640×480×256, etc.), or still at 320×200 mode 13h, from **one engine**
(`engine/black3.c`/`black4.c`) that supports both, compiled from **one
source file per demo** — the book's own `chapXX/foo.c` — that switches to
VESA behavior when `VBE_SUPPORT` is defined at compile time. There is no
separate VESA-only engine layer and no separate VESA-only demo source file.

**`VBE_SUPPORT` is a compile-time choice, not a runtime one.** A build with
`VBE_SUPPORT` defined gets the full runtime-generic engine — any
resolution/bpp the card supports, switchable while the program runs, via
`setGraphicsModeVesa`. A build *without* it — whether or not `DOS_32_BIT` is
also defined — gets the book's original, unmodified mode-13h-only engine:
fixed 320×200×8, the book's own fast fixed-320 address arithmetic
(`(y<<8)+(y<<6)+x`), no VESA capability compiled in at all, not even the
option. See **Architecture** below for exactly what that means and why.

The *demo* source files aren't separate either — `vbe/chapXX/foo.c` had
become the book's `chapXX/foo.c` plus a mode-set call and some scaled
coordinate constants, nothing more, and duplicating a whole file for that
small a diff was pure maintenance liability (a fix in one copy silently not
applying to the other). So `vbe/chapXX/foo.c` was retired: its VESA-specific
behavior was folded into `chapXX/foo.c` itself, behind `#ifdef VBE_SUPPORT` —
see **The demo-port recipe** below. `VBE_SUPPORT` implies 32-bit build
(`DOS_32_BIT` is already required for `setGraphicsModeVesa` to exist), the
same way `32bit/chap09/blazer.tgt` has always built `chap09/blazer.c` directly
with just `-dDOS_32_BIT` and no separate 32-bit copy — `VBE_SUPPORT` follows
that exact precedent one step further.

`vbe/chapXX/` folders now hold only Watcom project files (`.tgt`/`.wpj`)
and runtime assets (fonts, PCX, PLG) — no `.c` sources of their own. The
project's source reference points up to `..\..\chapXX\foo.c`, compiled with
`-dVBE_SUPPORT -dDOS_32_BIT`.

---

## Architecture

The stack is `black3`/`black4` → `dpmi` for VESA mode-set/mapping (32-bit
DOS/4GW only); `dpmi` is otherwise invisible to demo code. Mode-13h and
Mode-Z paths in `black3.c` never touch `dpmi` at all.

| Module | Role |
|---|---|
| `engine/dpmi.{c,h}` | DPMI bridge — real-mode INT 10h calls and physical memory mapping for the VBE linear framebuffer. Used internally by `setGraphicsModeVesa`; nothing else calls it. |
| `engine/black3.{c,h}` | Mode set (`setGraphicsMode`, `setGraphicsModeVesa`), the runtime mode-geometry globals, and the address-computing primitives: pixels, lines, rectangles, palette, text, Mode Z. |
| `engine/black4.{c,h}` | Everything built on top: double buffer, PCX, bitmaps, sprites (incl. tinted sprites), parallax layers, screen transitions. |

**Three build configurations, one source tree.** Every dual-purpose function
in `black3.c`/`black4.c` (pixel/line/rect/fill, PCX/bitmap/sprite/layer,
double-buffer, screen transitions, text) has two complete bodies, selected
entirely by preprocessor flags — there is no partial/runtime middle ground:

| `DOS_32_BIT` | `VBE_SUPPORT` | What gets compiled |
|---|---|---|
| ✗ | ✗ | The book's original 16-bit real-mode engine, untouched — fixed 320×200×8, `(y<<8)+(y<<6)+x` addressing, the book's own `_asm` blocks. Byte-for-byte what shipped before any of this work. |
| ✓ | ✗ | The same fixed-320×200×8 logic, just with flat 32-bit pointers instead of segment:offset (`FAR` expands to nothing) — a pre-VESA 32-bit port, not book code (see [16-bit vs. 32-bit builds](../README.md#16-bit-vs-32-bit-builds)). No VESA capability compiled in at all: `setGraphicsModeVesa`, the `DisplayWidth`/`Height`/`Pitch`/`Bpp` globals, `RGB32`, tinted sprites, and the true-colour PCX path don't exist in this build. |
| ✓ | ✓ | The full runtime-generic engine described below — any VESA resolution/bpp, switchable while the program runs. This is what every `vbe/chapXX/*.tgt`, `blazer.tgt`, and `blazerx.tgt` build. |

**`VBE_SUPPORT` implies `DOS_32_BIT`, enforced in code, not just by
convention.** `black3.h` (the one header every engine/demo file includes,
directly or transitively) opens with:

```c
#if defined(VBE_SUPPORT) && !defined(DOS_32_BIT)
#define DOS_32_BIT
#endif
```

A `.tgt` that sets `-dVBE_SUPPORT` but forgets `-dDOS_32_BIT` still builds
correctly — and since the guard makes `VBE_SUPPORT` a strict subset of
`DOS_32_BIT`, every place that used to test
`#if defined(DOS_32_BIT) && defined(VBE_SUPPORT)` (the handful of things
that are genuinely 32-bit-only mechanisms — `setGraphicsModeVesa`, the
`#pragma aux` assembly helpers, `loadFontSet`/`GlyphWidth`/`GlyphHeight`) has
been simplified to a bare `#ifdef VBE_SUPPORT`, logically equivalent now
but with one less condition to read. This doesn't relax the row 2 vs. row 3
distinction in the table above — `setGraphicsModeVesa` etc. must still be
excluded from a plain `DOS_32_BIT`-only build, and `#ifdef VBE_SUPPORT`
alone still does that correctly, since row 2 by definition has
`VBE_SUPPORT` undefined. It just
means the combination can no longer be *accidentally impossible* to satisfy.

**Runtime mode state** (`black3.h`, only declared under `VBE_SUPPORT` — a
non-`VBE_SUPPORT` build has no such globals to read, since its address math
is all compile-time constants instead):

```c
extern unsigned char FAR* VideoBuffer;  // current mode's framebuffer
extern int DisplayPitch;                 // bytes per row of the CURRENT mode
extern int DisplayWidth;                 // pixel width of the CURRENT mode
extern int DisplayHeight;                // pixel height of the CURRENT mode
extern int DisplayBpp;                   // 8, 16, or 32
extern int DisplayBppShift;              // log2(bytes/pixel): 0/1/2 for 8/16/32bpp
extern int DisplayPitchShift1;           // see PITCH_OFFSET below
extern int DisplayPitchShift2;           // -1 if unused
#define PITCH_OFFSET(y) /* y * DisplayPitch, via shifts - see below */
```

`DisplayBppShift` exists so an `x`-offset byte calculation (`x * (DisplayBpp / 8)`, everywhere a drawing function turns a pixel column into a byte offset) can be `x << DisplayBppShift` instead — a runtime multiply the compiler can't strength-reduce on its own, since it doesn't know `DisplayBpp / 8` is restricted to {1, 2, 4}. Kept in sync with `DisplayBpp` everywhere it's set.

`DisplayPitch`'s own multiply (`y * DisplayPitch`, the row-offset half of the same address computation) is handled differently, because `DisplayPitch` isn't always a single power of two the way `DisplayBpp / 8` always is:

- `setGraphicsModeVesa` renegotiates the *hardware's own* logical scan line length via `INT 10h 4F06h`/`BL=02h` (VBE 2.0's "Set Logical Scan Line Length in Bytes") right after setting the mode, requesting the smallest power of two that still fits the visible width. The BIOS reconfigures the card's actual per-scanline VRAM stride to that value and reports it back in `BX`; the mode fails outright (same as any other unsupported width/height/bpp) if the card can't honor it. This is a real hardware change, not just bookkeeping, so addressing stays correct — it costs extra VRAM for widths that weren't already a power of two (e.g. 800×600×32: 3200 bytes/row padded to 4096, +28%). Once negotiated, `DisplayPitchShift1 = log2(DisplayPitch)` and `DisplayPitchShift2 = -1` (unused).
- `GRAPHICS_MODE13`'s pitch is a fixed physical VGA constant (320) with no VBE equivalent to renegotiate it through — but 320 = 256 + 64, the same two-term decomposition the book's own non-`VBE_SUPPORT` code already uses directly (`(y<<8)+(y<<6)`), so `DisplayPitchShift1 = 8`, `DisplayPitchShift2 = 6`.

A `VBE_SUPPORT` build can switch into true mode-13h at runtime (`setGraphicsMode(GRAPHICS_MODE13)` is always available, not just under non-`VBE_SUPPORT` builds — see just below, where a program can call it, then `setGraphicsModeVesa`, then back, all in one run), so every `VBE_SUPPORT` drawing function needs to handle both pitch shapes — hence `PITCH_OFFSET(y)`, a macro expanding to `(y << DisplayPitchShift1) + (y << DisplayPitchShift2)` (the second term only when `DisplayPitchShift2 >= 0`), used everywhere a function would otherwise write `(unsigned long)y * DisplayPitch`. Per-row loop advances (`p += DisplayPitch`) are already plain addition, not multiplication, and don't need it.

(`VideoBuffer` itself is the one exception — declared unconditionally, since
every build configuration needs it, just pointing at a fixed segment in the
non-`VBE_SUPPORT` case instead of wherever `setGraphicsModeVesa` mapped it.)

Named `Display*`, not `Screen*` — `black11.h` (the 3D system, out of scope for
this work) already declares `extern float ScreenWidth, ScreenHeight;` for an
unrelated concept (the 3D projection plane's virtual size in perspective
math). Any demo including both headers would get a Watcom "type does not
agree with previous definition" error if this header reused that name.

In a `VBE_SUPPORT` build, every drawing function reads these instead of
assuming mode-13h's fixed 320×200×8 — that's what makes the same
`writePixel`/`spriteDraw`/etc. work at any resolution/bpp. They're updated
by whichever mode-set function ran most recently:

```c
void setGraphicsMode(int mode);                          // TEXT_MODE / GRAPHICS_MODE13 - unchanged from the book
#ifdef VBE_SUPPORT
int  setGraphicsModeVesa(int width, int height, int bpp); // any VBE mode; 1=ok, 0=unavailable
#endif
```

`setGraphicsModeVesa` needs 32-bit flat mode (VESA has no 16-bit real-mode
equivalent) — a plain `#ifdef VBE_SUPPORT` is enough to guard it, not
`#if defined(DOS_32_BIT) && defined(VBE_SUPPORT)`, since `VBE_SUPPORT`
now always implies `DOS_32_BIT` (see **`VBE_SUPPORT` implies `DOS_32_BIT`**
above). Within a `VBE_SUPPORT` build,
the bpp dispatch inside every drawing function is a runtime
`switch (DisplayBpp)`, not a further compile-time branch, so the same
compiled binary can run mode-13h, switch to VESA at any resolution/bpp, and
switch back, all while running. Call `setGraphicsModeVesa` again with
different arguments any time to change resolution/bpp mid-program.

**The performance trade-off is now opt-in, not universal.** mode-13h's own
`writePixel` is fast in a non-`VBE_SUPPORT` build because 320 is a
compile-time constant that decomposes into two shifts (`(y<<8) + (y<<6)`)
instead of a multiply — exactly as the book wrote it, with zero runtime
dispatch overhead. Only a `VBE_SUPPORT` build pays for the flexibility:
pitch becomes a runtime value (since it might be VESA's negotiated pitch),
so every drawing call there pays one runtime multiply and one runtime bpp
`switch`. In practice this remains imperceptible even where it's paid:
DOSBox-X's real bottlenecks are fixed BIOS-tick delays and blit bandwidth,
not per-pixel branch/multiply cost — but the point of this three-way split
is that the vast majority of demos, which never need VESA at all, no longer
pay any part of that cost, or carry any of that code.

---

## Header API reference

There is one drawing API (`black3.h` + `black4.h`), same function names and
signatures in every build configuration — VESA support never added new
function names, it made the existing ones' *implementations*
runtime-generic, gated by `VBE_SUPPORT`. Anything marked `VBE_SUPPORT`-only
below simply doesn't exist as a symbol in a non-`VBE_SUPPORT` build; calling
code that needs it won't compile, which is the intended guardrail (see
**Three build configurations** above).

### `engine/black3.h`

```c
#define GRAPHICS_MODE13 0x13
#define TEXT_MODE       0x03

void setGraphicsMode(int mode);                          // TEXT_MODE / GRAPHICS_MODE13
#ifdef VBE_SUPPORT
int  setGraphicsModeVesa(int width, int height, int bpp); // bpp: 8, 16, or 32
#endif

void timeDelay(int clicks);            // BIOS-tick delay (~18.2 Hz per click)
void fillScreen(int color);
void writePixel(int x, int y, int color);
int  readPixel(int x, int y);
void lineH(int x1, int x2, int y, int color);
void lineV(int y1, int y2, int x, int color);
void drawRectangle(int x1, int y1, int x2, int y2, int color);

typedef struct { unsigned char red, green, blue; } RgbColor;   // 0..63 (6-bit VGA DAC range)
typedef struct { int startReg, endReg; RgbColor colors[256]; } RgbPalette;
void writeColorReg(int index, RgbColorPtr color);
RgbColorPtr readColorReg(int index, RgbColorPtr color);
void readPalette(int startReg, int endReg, RgbPalettePtr palette);
void writePalette(RgbPalettePtr palette);

#ifdef VBE_SUPPORT
// RGB32(r,g,b): pack 0..255 components into a 24/32bpp color value (R bits
// 16-23, G bits 8-15, B bits 0-7). Only meaningful when DisplayBpp is 32,
// which is only possible under VBE_SUPPORT (mode-13h is always 8bpp).
#define RGB32(r,g,b) (((unsigned long)(r)<<16)|((unsigned long)(g)<<8)|(unsigned long)(b))
#endif

void printChar(int xc, int yc, char c, int color, int transparent);
void printString(int x, int y, int color, char* string, int transparent);
#ifdef VBE_SUPPORT
// draws at GlyphWidth x GlyphHeight (see Assets) - variable-size fonts are
// VESA-only; a DOS_32_BIT build without VBE_SUPPORT always draws the fixed
// 8x8 ROM-equivalent font instead (see initRomCharSet below)
extern int GlyphWidth, GlyphHeight;   // size of the CURRENTLY LOADED font's glyph cell
void freeFontSet(void);
int  loadFontSet(const char* filename, int size);   // any square bit-packed font, e.g. font16.bin - call once before any text
#elif defined(DOS_32_BIT)
int  initRomCharSet(void);
void freeRomCharSet(void);
#endif

// Mode Z (320x400, 4-plane unchained VGA) - untouched, mode-13h-only, no VESA equivalent
void setModeZ(void);
void fillScreenZ(int color);
void writePixelZ(int x, int y, int color);
```

### `engine/black4.h`

```c
extern unsigned char FAR* DoubleBuffer;   // the one global off-screen buffer
extern unsigned int DoubleBufferSize;     // its size in WORDs (legacy unit - see black4.c)

int  createDoubleBuffer(int numLines);   // VBE_SUPPORT: width is implicitly DisplayWidth, pitch is DisplayPitch. Without it: fixed 320.
void fillDoubleBuffer(int color);
void displayDoubleBuffer(unsigned char FAR* buffer, int y);   // blit to VideoBuffer at row y
void deleteDoubleBuffer(void);
void writePixelDb(int x, int y, int color);
int  readPixelDb(int x, int y);
void printCharDb(int xc, int yc, char c, int color, int transparent);
void printStringDb(int x, int y, int color, char* string, int transparent);

// Bitmaps - a single static image at a position
typedef struct { int x, y, width, height; unsigned char FAR* buffer; } Bitmap;
int  bitmapAllocate(BitmapPtr image, int width, int height);
void bitmapGet(BitmapPtr image, PcxPicturePtr source);                // VBE_SUPPORT: reads at source's OWN width (source->header), e.g. a font sheet PCX. Without it: fixed 320-wide source, the book's original form.
void bitmapPut(BitmapPtr image, unsigned char FAR* destination, int transparent);  // dest: VideoBuffer or DoubleBuffer
void bitmapDelete(BitmapPtr image);

// PCX - 256-color indexed (bpp 8) always; 24-bit true color widened to
// 32bpp on load (bpp 32) is VBE_SUPPORT-only (mode-13h is always 8bpp, so
// there's nothing for a true-colour PCX to widen into without it). File's
// own bpp must match DisplayBpp; pcxLoad fails (returns 0) otherwise,
// before decoding - VBE_SUPPORT only, the non-VBE_SUPPORT decoder is the
// book's original simple 256-colour-only form with no such check.
typedef struct { PcxHeader header; RgbColor palette[256]; unsigned char FAR* buffer;
#ifdef VBE_SUPPORT
                 int bpp;
#endif
                } PcxPicture;
int  pcxInit(PcxPicturePtr image);       // VBE_SUPPORT: allocates DisplayWidth*DisplayHeight*(DisplayBpp/8) bytes. Without it: fixed 320x200.
void pcxDelete(PcxPicturePtr image);
int  pcxLoad(char* filename, PcxPicturePtr image, int loadPalette);
void pcxShowBuffer(PcxPicturePtr image);              // 1:1 to VideoBuffer, top-left
void pcxCopyToBuffer(PcxPicturePtr image, unsigned char FAR* buffer);  // 1:1 at (0,0)

// Sprites - transparentColor/tintMask (VBE_SUPPORT-only fields; a
// non-VBE_SUPPORT Sprite doesn't carry them at all) support
// live-recolored regions (see Tinted sprites below); transparentColor
// defaults to 0 in spriteInit ("index 0 is transparent", the book's own
// hardcoded convention, which is also what a non-VBE_SUPPORT build's
// transparency check is hardcoded to, with no field to override it).
#define MAX_SPRITE_FRAMES 32
typedef struct { int x, y, width, height;
                 unsigned char FAR* frames[MAX_SPRITE_FRAMES];
#ifdef VBE_SUPPORT
                 unsigned char FAR* tintMask[MAX_SPRITE_FRAMES];
                 unsigned long transparentColor;
#endif
                 int counter1, counter2, counter3, threshold1, threshold2, threshold3;
                 int currFrame, numFrames, state;
                 unsigned char FAR* background;
                 int xClip, yClip, widthClip, heightClip, visible; } Sprite;
void spriteInit(SpritePtr sprite, int x, int y, int width, int height,
                int c1, int c2, int c3, int t1, int t2, int t3);
void spriteDelete(SpritePtr sprite);
void pcxGetSprite(PcxPicturePtr image, SpritePtr sprite, int spriteFrame, int cellX, int cellY);
void spriteDraw (SpritePtr sprite, unsigned char FAR* buffer, int transparent);
void spriteUnder(SpritePtr sprite, unsigned char FAR* buffer);
void spriteErase(SpritePtr sprite, unsigned char FAR* buffer);
void spriteDrawClip (SpritePtr sprite, unsigned char FAR* buffer, int transparent);
void spriteUnderClip(SpritePtr sprite, unsigned char FAR* buffer);
void spriteEraseClip(SpritePtr sprite, unsigned char FAR* buffer);

#ifdef VBE_SUPPORT
// Tinted sprites - net-new, for effects that used to be palette-register
// tricks (shield glow, engine flicker) and silently no-op above 8bpp. No
// book-original equivalent, hence VBE_SUPPORT-only, same as PcxPicture.bpp
// and Sprite.tintMask/transparentColor above.
// A source pixel equal to tintKeys[i] is recorded in tintMask as region
// (i+1); spriteDrawTinted substitutes tintColors[region-1] for those
// pixels live, at draw time, while everything else blits normally.
// spriteDrawTinted always clips - unlike spriteDraw/spriteUnder/spriteErase
// above, it has no separate unclamped fast-path sibling, so every call is
// clipped whether or not the sprite can go off-screen. Keep it that way: an
// earlier unclamped version let blazerx's off-screen remote ship overflow
// the heap and corrupt other sprites.
void pcxGetSpriteTinted(PcxPicturePtr image, SpritePtr sprite, int spriteFrame,
                int cellX, int cellY, const unsigned long* tintKeys, int numTintKeys);
void spriteDrawTinted(SpritePtr sprite, unsigned char FAR* buffer, int transparent,
                const unsigned long* tintColors, int numTintColors);
#endif

// Parallax layers (horizontally-wrapping scroll strips) - layerCreate's
// buffer is sized DisplayBpp-wide under VBE_SUPPORT (width*height*2 in a
// plain 32-bit or 16-bit build, matching what layerBuild/layerDraw actually
// read/write at that bpp)
typedef struct { int width, height; unsigned char FAR* buffer; } Layer;
int  layerCreate(LayerPtr destLayer, int width, int height);
void layerDelete(LayerPtr layer);
void layerBuild(LayerPtr destLayer, int destX, int destY, unsigned char FAR* sourceBuffer,
                int sourceX, int sourceY, int width, int height);
void layerDraw(LayerPtr sourceLayer, int sourceX, int sourceY, unsigned char FAR* destBuffer,
                int destY, int destHeight, int transparent);

// Screen transitions
#define SCREEN_DARKNESS  0
#define SCREEN_WHITENESS 1
#define SCREEN_WARP      2   // unimplemented in the book - unimplemented here too
#define SCREEN_SWIPE_X   3
#define SCREEN_SWIPE_Y   4
#define SCREEN_DISSOLVE  5
void screenTransition(int effect);
```

`screenTransition`'s `SCREEN_DARKNESS`/`WHITENESS` fades through the DAC at
8bpp (the book's own logic, untouched) or re-renders every pixel from a
one-time snapshot at bpp>8 (there's no DAC to fade). Both paths run 20
steps via `timeDelay(1)`, so the fade takes the same real time at any
resolution/bpp. `SCREEN_SWIPE_X`/`Y` wipe via `lineV`/`lineH` and work
unchanged at any bpp (they draw literal color `0`).

---

## The demo-port recipe

There is **one source file per demo**, `chapXX/foo.c` — the book original —
and it gains an `#ifdef VBE_SUPPORT` block wherever VESA behavior differs from
the book's. With `VBE_SUPPORT` undefined the file compiles to exactly the
book-original mode-13h demo; with it defined (always alongside `-dDOS_32_BIT`,
since `setGraphicsModeVesa` only exists there), it compiles to the VESA port.
There is no second copy anywhere, and no `.tgt`-level `VESA_BPP`/resolution
macro — resolution and bpp are literal arguments hardcoded inside each
file's own `#ifdef VBE_SUPPORT` block, same numbers as before, just no longer
duplicated into a whole second file to hold them.

Since the drawing API is identical between mode-13h and VESA (see **Header
API reference** above), the diff folded into each `#ifdef VBE_SUPPORT` block
is only ever one or more of:

**1. The mode-set call.** `setGraphicsMode(GRAPHICS_MODE13)` under `#else`
becomes `setGraphicsModeVesa(width, height, bpp)` under `#ifdef VBE_SUPPORT`,
with literal resolution/bpp arguments. `setGraphicsMode(TEXT_MODE)` on exit
is shared/unwrapped — same call either way.

**2. Coordinate constants**, for animated/scrolling demos. Going from
320×200 to a higher resolution still means screen-relative values need to
scale (X ×`(width/320)`, Y ×`(height/200)`) — see **Coordinate constants**
and the **camera pattern** below for the full story, including the
two-category split (screen-space UI vs. in-world objects) and the
scanner/starfield mistakes that motivated it. Every `spriteInit`/
`bitmapAllocate` size, movement delta, and screen-position literal that
differs gets its own small `#ifdef VBE_SUPPORT` / `#else` pair (or, for a demo
with many such sites, e.g. `chap09/blazer.c`, dozens of them scattered
through the file — this doesn't collapse into fewer, larger blocks, since
each site's book-original number needs to survive untouched under `#else`).

**3. Font loading**, if the demo loads a font at all beyond the ROM 8×8 set.
`loadFontSet(filename, size)` loads any square, bit-packed font file at a
custom cell size (`GlyphWidth`/`GlyphHeight` become runtime state, read by
`printChar`/`printString`/`printCharDb`/`printStringDb`) — call it once near
program start under `#ifdef DOS_32_BIT` inside the `#ifdef VBE_SUPPORT`
branch, picking whichever of `font16.bin`/`font24.bin`/`font32.bin` matches
the demo's resolution (see **Assets** below). `loadFontSet` and
`GlyphWidth`/`GlyphHeight` don't exist at all outside `DOS_32_BIT &&
VBE_SUPPORT` (see **Three build configurations** above) — real-mode 16-bit
builds read the fixed 8×8 BIOS font directly and never need font loading at
all, and a `DOS_32_BIT`-alone build (no `VBE_SUPPORT`) at mode-13h's
320×200 calls `initRomCharSet()`/`freeRomCharSet()` instead (see **Header
API reference** above). `chap14`-`chap17` (out of scope for this merge,
plain `DOS_32_BIT` builds with no `VBE_SUPPORT`) call the same pair, for
the same reason. A demo with its own
bitmap font sheet, like `chap09`'s `blazefnt.pcx`, bypasses this system
entirely and draws via `Bitmap`/`bitmapGet`/`bitmapPut` instead (unwrapped,
since `bitmapGet`'s one signature works the same — just at different
internal strides — in every build configuration).

**4. Genuinely structural differences**, rare but real — e.g.
`chap04/spheres.c` uses Mode Z (`setModeZ`/`writePixelZ`, no VESA
equivalent) in its book-original `#else` branch, and software double
buffering (`createDoubleBuffer`/`writePixelDb`/`displayDoubleBuffer`) in its
`#ifdef VBE_SUPPORT` branch — the two branches don't share a rendering
approach at all for this one file. `chap08/jelly.c` similarly swaps a real
timer ISR (INT 0x1C, `#else`) for an inlined per-frame position update
(`#ifdef VBE_SUPPORT` — `_asm sti` is a ring-3 GPF under DOS/4GW and the ISR's
EOI is redundant there). New globals/arrays/`#define`s that only the VESA
branch needs (e.g. `chap09/blazer.c`'s `CAM_ZOOM`/`VIEW_WIDTH`/`VIEW_HEIGHT`,
`chap04/spheres.c`'s precomputed sphere-position arrays) go inside
`#ifdef VBE_SUPPORT` with no `#else`, since the book original has no
equivalent to preserve.

**5. Stale mode-13h comments**, only inside the `#ifdef VBE_SUPPORT` branch —
the book-original comments under `#else` stay exactly as the book wrote
them.

Everything else stays verbatim in both branches — do not rename variables,
reflow switch blocks, or reword logic beyond what a given `#ifdef` boundary
requires.

**Never reference `vbe/PORTING.md`, `README.md`, or any other doc file by
name in a code comment** (`.c`/`.h`), in either branch. A comment has to
carry its own reasoning inline — state the *why* directly, not "see
vbe/PORTING.md for the derivation." Doc file paths rot (files get renamed,
sections get restructured or deleted) in a way the compiler can't catch,
where a stale in-code explanation at least stays readable even after it
falls behind. This applies to every source file this guide touches,
including `engine/black11.c`/`black15.c` and their sibling engine files,
not just the `chapXX/foo.c` demo files.

---

## Coordinate constants & the camera pattern

Going from 320×200 to a higher resolution means screen-relative values
scale with the resolution: X ×`(width/320)`, Y ×`(height/200)`. So
`320→640`, `200→480`, `319→639`, `199→479`, `160→320`, grid spacing
`20→40`, a `% 190` y-range → `% 470`. Loop counts, color indices, and
string widths do not change. This flat scaling works for demos that draw
directly to a fixed screen (menus, HUDs, static scenes).

It breaks down for a **game with a scrolling world bigger than one screen**
(a player-locked window into a universe of positions, like chap09's
Starblazer): hand-doubling every constant that happens to be screen-relative
is error-prone, because nothing distinguishes a screen-relative constant
from a world-relative one just by looking at it. chap09 shipped with
exactly this bug for a while — ship speed, AI engagement distances, and
effect positions were all silently wrong by a factor of 2, because "double
it" had been applied inconsistently.

The fix is a **camera**: keep the entire simulation in the book's original
units (speeds, AI distances, collision sizes, world object sizes — literally
the same numbers as the book), and apply exactly one transform at the
world-to-screen boundary:

```c
#define CAM_ZOOM     2                   // world units -> screen pixels
#define VIEW_WIDTH   (640 / CAM_ZOOM)    // world units visible across
#define VIEW_HEIGHT  (480 / CAM_ZOOM)    // world units visible down

// at each existing window-mapping site:
screenX = (worldX - windowOriginX) * CAM_ZOOM;
screenY = (worldY - windowOriginY) * CAM_ZOOM;
```

### The two categories

Sort everything a demo draws into exactly one of two buckets — the question is
**"does this depict something that exists in the game world?"**, not "how is
its position tracked":

1. **Screen-only UI** — menus, HUD text, the scanner display, static full-screen
   art. Scale the PCX asset (per Assets below) and hand-double the handful of
   coordinates drawn over it. This is the original demo recipe; nothing about
   it changes.
2. **In-world visual assets** — anything depicting an object or effect that
   lives in the game world: ships, asteroids, missiles, explosion sparks, and
   —non-obviously— background parallax elements like a starfield. These must
   render at `CAM_ZOOM` size to stay visually proportional to everything else
   in the world, **even if their position is tracked in screen/view units
   rather than world units.** A starfield is the clean example: the book
   deliberately keeps stars in screen coordinates (see its own comment —
   "otherwise we'd need thousands of stars to fill the universe instead of
   50") purely as a performance/design trick, but a star is still conceptually
   a thing in the universe's backdrop. Track it in *view* units (same scale as
   `VIEW_WIDTH`/`VIEW_HEIGHT`, i.e. pre-camera) exactly like a missile is
   tracked in world units, and multiply by `CAM_ZOOM` only at the draw call —
   don't leave it as a bare screen pixel just because its coordinates happen
   to already look like screen coordinates.

Consequences that follow from bucket 2:

- **Sprite bitmaps are pre-scaled ×`CAM_ZOOM`** (asset creation time, per the
  Assets section below) — so `spriteInit` is called with `width * CAM_ZOOM`,
  `height * CAM_ZOOM`, using **named book-value constants**, not inline doubled
  literals — give every in-world sprite its own `#define ..._WIDTH`/`_HEIGHT`
  (mirroring `SHIP_WIDTH`/`ASTEROID_*_WIDTH`) even if nothing else references
  it, so the value is traceable back to the book and survives a `CAM_ZOOM`
  change. A literal `56, 44` in a `spriteInit` call is a code smell — it's
  either an unscaled book value that's wrong, or a scaled one with the ×2
  invisible in the diff.
- **Collision tests stay in world units.** A test against a sprite's pixel
  size (`rock.width`) must divide back down: `astX + rock.width / CAM_ZOOM`.
- **Single-world-pixel particles** (missiles, spark cinders, stars) need
  explicit `CAM_ZOOM`×`CAM_ZOOM`-block draw/erase/save helpers — a lone
  `writePixel` no longer covers one world pixel on screen. This is the one
  place scaling doesn't fall out for free: a sprite blit already touches
  `width×height×CAM_ZOOM²` bytes because the *bitmap* is bigger, but a raw
  pixel write has no bitmap to inherit the scale from. The `back` field used to
  save-under for erasing must become a `CAM_ZOOM*CAM_ZOOM`-element array (not a
  scalar) for the same reason.

**Why this matters beyond correctness:** rendering more screen pixels always
costs more per-pixel work — a 640×480 buffer is ~4.8× the pixels of the book's
320×200, and this engine's draw/erase-under-yourself model (no full-frame
redraw) means every moving object pays that multiplier every frame, camera or
not. The camera doesn't add that cost; it just makes it consistent and
impossible to apply half-correctly, unlike scattering `* 2` across every call
site by hand. See chap09 below for the worked example.

---

## Naming & layout

One folder per chapter for the demo source: `chapXX/foo.c` (the book
original, gaining `#ifdef VBE_SUPPORT`). A parallel `vbe/chapXX/` folder
holds that demo's VESA `.tgt`/`.wpj` project (source-referencing
`..\..\chapXX\foo.c`, built with `-dVBE_SUPPORT -dDOS_32_BIT`) and all VESA-
specific runtime assets (`font16.bin`, resolution-scaled `*.PCX`) — the
book's own `chapXX/` assets (`font.bin`, 320×200 `*.PCX`, `*.PLG`) are used
directly by the mode-13h build and never duplicated.

Keep the book's filename for each port — there's only one `.c` file per
demo now, so there's no separate VESA filename to choose (`chap03/mode13.c`
serves both the book-original `mode13.tgt`-style build and the VESA
`svga.tgt` build, with `VBE_SUPPORT` selecting between them, even though the
project's own `.exe` name can still be `svga.exe`). Drop demos that only
showcase a VGA hardware mode with no SVGA equivalent (e.g. `modez`).

Current status: chap03 ✓, chap04 ✓, chap05 ✓, chap07 ✓, chap08 ✓, chap09 ✓ —
all merged into their `chapXX/*.c` book originals via `#ifdef VBE_SUPPORT`;
`vbe/chapXX/*.c` no longer exist. `chap03/mode13.c` additionally carries
its former 16bpp/32bpp high-color showcases (`svga_l32.tgt`/`svga_m16.tgt`)
as two more compile-time variants, `VBE_DEMO_L32` (1024×768×32) and
`VBE_DEMO_M16` (800×600×16) — see **`VBE_DEMO_L32`/`VBE_DEMO_M16`**
below.

### `VBE_DEMO_L32` / `VBE_DEMO_M16`

`chap03/mode13.c`'s SVGA mode-intro demo has three VESA targets instead of
one, all compiled from the same source: plain `VBE_SUPPORT` (640×480×8,
`svga.exe`), `VBE_DEMO_L32` (1024×768×32 true color, `svga_l32.exe`), and
`VBE_DEMO_M16` (800×600×16 high color, `svga_m16.exe`). **Both extra flags
are defined *alongside* `VBE_SUPPORT`, never instead of it** — `svga_l32.tgt`
sets `-dVBE_SUPPORT -dVBE_DEMO_L32`, `svga_m16.tgt` sets `-dVBE_SUPPORT
-dVBE_DEMO_M16` — since `VBE_SUPPORT` is still what gates the whole
`setGraphicsModeVesa`-vs-`setGraphicsMode(GRAPHICS_MODE13)` fork; the two
extra flags only refine *which* VESA target within that fork. Defining
`VBE_DEMO_L32`/`VBE_DEMO_M16` without `VBE_SUPPORT` is a misconfiguration —
the resolution constants would be set for the high-color target, but the
mode-set call would silently fall back to mode-13h.

At 16/32bpp there's no DAC, so the palette-manipulation section at the end
of the demo (`readPalette`/`writeColorReg` loop/`writePalette`) is replaced
by a full-screen hue-cycle gradient instead (`hueToRgb` sweeping 0-359° across
the screen width, drawn with `lineV`) — impossible to show in 8bpp palette
mode without 16 million palette entries. Everything before that (text,
fill, pixel plot, line draw) uses the same runtime-computed `DEMO_WIDTH`/
`DEMO_HEIGHT` as the plain `VBE_SUPPORT` build, just resolved to a different
resolution per flag, and colors are packed with `RGB32`/`RGB16` (a local
macro, mirroring `chap03/svga_m16.c`'s original one — there's no engine
equivalent since packed 16bpp isn't a general engine concern) instead of
palette indices.

**chap09 notes** — `blazer.c` (Starblazer) uses `black3.h`/`black4.h`
directly in both branches, including its `TechFont`/`fontEngine1`/
`techPrint` bitmap-font code, which ports 1:1 onto `Bitmap`/
`bitmapAllocate`/`bitmapGet`/`bitmapPut` in the `VBE_SUPPORT` branch — this is
the demo that introduced `Bitmap` to real use in a VESA build, since it's
the only chapter so far whose bitmap font doesn't use the ROM ceiling-font
path. `blazefnt.pcx` is scaled cell-by-cell to 8×14 (2×, 1-pixel grid
preserved) with `scale_assets.py`, living in `vbe/chap09/` alongside the
`.tgt`/`.wpj`. `VideoBuffer`/`DoubleBuffer` are the same globals the book
original uses in both branches — no surface wrapper of any kind.

The gameplay world uses the **camera pattern** described above (`CAM_ZOOM 2`, `VIEW_WIDTH`/`VIEW_HEIGHT` 320×240): every simulation constant — `MotionDx`/`MotionDy`, velocity clamps, asteroid/ship/alien world sizes, collision tests — is the book's original value, byte-for-byte. Only the draw code applies `CAM_ZOOM`: window-mapping sites, `spriteInit` sizes (every in-world sprite — ships, asteroids, the wormhole, fuel cells, the alien, explosions — has a named `#define ..._WIDTH`/`_HEIGHT` at the book's value, multiplied by `CAM_ZOOM` at the init call), the three sprite-vs-world collision checks (divide the sprite's pixel size back down), and the `cameraPixel*` block helpers for single-world-pixel effects — missiles, nova cinders, **and the starfield**. The star field is the trap case: the book tracks it in screen coordinates on purpose (its own comment explains why — 50 stars would need "thousands" to fill the universe if tracked in world units), which made it easy to miss during the initial camera pass, since its coordinates don't obviously look "in-world." Fixed by tracking `Stars[].x/y` in *view* units (same scale as `VIEW_WIDTH`/`VIEW_HEIGHT`, moved by the book's unscaled `PlayersDx`/`Dy` parallax deltas) and multiplying by `CAM_ZOOM` only at `cameraPixelDraw`/`Under`/`Erase` — the same pre-scale-then-transform-at-draw-time shape as every other in-world particle.

`chap09/blazer.c` has **no single-player AI and no IPX**, in either branch: Play Solo drops you in against a stationary, book-faithful practice target, and Make/Wait Connection use the real modem link. Both features (and their `AI_*` engagement-distance constants) live only in `blazerx/blazerx.c`, a top-level fork with IPX LAN play, the AI, and the 1024×768×32bpp true-color migration merged in (see **blazerx: 1024×768, 32-bit true color** below) — see also the repo README's [Networking](../README.md#networking-lan-play) section. It's a separate copy, not built from `chap09/blazer.c` via a relative path or a shared `#ifdef`, so a fix made in one needs to be manually re-applied to the other if it isn't specific to AI/networking/resolution.

Screen-space UI, which the camera doesn't touch, is ×2 of the book's — except y-coordinates drawn over the full-screen 320×200 backgrounds, which scale ×2.4 (all four backgrounds stretch to 640×480, reproducing the original 4:3 tall-pixel look): the menu cursor rows (`151 + n×29` for the book's `63 + n×12`), the setup display box (`DISPLAY_Y` 163, `DISPLAY_HEIGHT` 67), the briefing text rows (`58 + n×19`), the briefing page buttons (y 408), and **the scanner** (`SCANNER_Y` 399 for the book's 166, grid 128×76 for the book's 64×32, cell step 38 for 16, blip divisor `/34` for `/40`). Strings and page layout are unchanged.

The scanner is the cautionary example for this whole ×2-vs-×2.4 split: it shipped for a while at `SCANNER_Y=332` (a plain ×2 of the book's 166), which is wrong the same way an unscaled AI distance or ship speed is wrong — it's screen-relative, not world-relative, so it needed the y-axis's true ×2.4, not the x-axis's ×2. The symptom was visual, not a crash (the grid just floated above the bottom edge instead of hugging it, and was undersized relative to the screen), which is exactly why it's easy to miss: nothing about the code looks broken, it just looks a little off. Checked against the book original, the scanner hugs the bottom (166+32=198 of 200, a ~1% margin) and occupies a fixed proportion of the screen (64/320=20% wide, 32/200=16% tall) — the right derivation is those two facts (screen-edge-relative position, screen-proportional size) applied fresh to the target resolution, not a scale factor inherited from whichever other coordinate was scaled last.

Sprite sheets (including `blazefnt.pcx`) scaled cell-by-cell with `scale_assets.py` in `vbe/chap09/`. Runtime assets: all `.VOC` + `BLAZEMUS.XMI` + `GENMID.ADD` copied from `chap09/` (no engine font.bin needed — chap09 never loads one).

The scanner grid is also drawn thicker than the book's 1px: `lineH2`/`lineV2` (the book's `line2`, unchanged in both branches — its body is `#ifdef VBE_SUPPORT`-branched only for the hardcoded-320-shift-vs-runtime-`DisplayPitch` addressing, same as `black3.c`'s own `lineH`/`lineV`) gained `lineH2Thick`/`lineV2Thick` wrappers (VESA-only) that redraw the same line at `thickness` adjacent offsets, centered on the requested position (`SCANNER_LINE_THICK` 2 here). This is scoped to `eraseScanner`/`drawScanner` only — `lineH2`/`lineV2` themselves are used unthickened elsewhere too (`blazerx.c` reuses its own copy of them unthickened for `panelFx`'s per-row scrolling-light fill, see below), where redrawing each row multiple times would be redundant, not wrong, but pointless work. (`lineH2`/`lineV2` are per-file helpers — defined once in `chap09/blazer.c`, again independently in `blazerx.c` since it's a separate fork — not part of `black3.h`/`black4.h`, since they draw into an arbitrary caller-supplied buffer, which neither `lineH`/`lineV` (`VideoBuffer` only) nor any generic-buffer equivalent in the shared engine covers.)

Blips (`UnderPlayersBlip`/`UnderRemotesBlip`) are the same story one level down: the book plots/reads/restores a single pixel per blip, which is likewise a speck at this resolution. `blipSave`/`blipDraw`/`blipRestore` replace the bare `readPixelDb`/`writePixelDb` calls with a `BLIP_SIZE`×`BLIP_SIZE` filled block, `BLIP_SIZE` = `SCANNER_LINE_THICK` (2 here, since it's the same "make a book-original single-pixel UI mark visible at this resolution" call); `UnderPlayersBlip`/`UnderRemotesBlip` widened from a scalar to a `BLIP_SIZE*BLIP_SIZE`-element array to save/restore the whole block instead of one pixel.

---

## blazerx: 1024×768, 32-bit true color

`blazerx/blazerx.c` pushes past 640×480×256 to 1024×768 at 32bpp true color
(16.7M colors, no palette) — a second resolution step on top of everything
above. `chap09/blazer.c`'s `VBE_SUPPORT` branch stays at 640×480×256
untouched; this section covers only what's specific to `blazerx`.

### Engine support

Everything `blazerx` needs — a runtime `bpp` argument to
`setGraphicsModeVesa`, bpp-generic `PcxPicture`/`Sprite`/`Bitmap` handling,
tinted sprites, a true per-pixel `screenTransition` fade above 8bpp — is
now built into `black3.c`/`black4.c` directly (see **Header API reference**
above), not a `blazerx`-specific or even VESA-specific extension. There is
no `VESA_BPP` compile-time macro anywhere in this design: `blazerx.tgt`
builds the exact same `black3.c`/`black4.c` as every other target; the only
thing that makes `blazerx` run at 1024×768×32 is its own
`setGraphicsModeVesa(1024, 768, 32)` call.

`PcxPicture` carries a real `bpp` field because it's the one place bpp is
genuinely per-instance data — a specific PCX *file's* own intrinsic depth
(8 for indexed, 32 for a widened true-color file), independent of whatever
`DisplayBpp` the program is currently running at. `pcxLoad` decodes either a
256-color indexed PCX (1 color plane, `bpp`=8) or a 24-bit true-color PCX (3
color planes, no palette — a real, standard PCX variant PIL/Pillow can write
directly) and widens it to 32bpp on load, and fails if the file's depth
doesn't match the current `DisplayBpp` — there's no separate "native" format
or loader, PCX genuinely supports true color.

**Tint mechanism** (`pcxGetSpriteTinted`, `spriteDrawTinted`): replaces the
palette-register tricks (shield glow, engine flicker, panel scrolling
lights) that don't work once there's no DAC to write. A sprite's `tintMask`
marks pixels equal to one of N `tintKeys` (recorded at frame-extraction
time) with a 1-based region number; drawing with `spriteDrawTinted`
substitutes `tintColors[region-1]` for those pixels live, at draw time,
while everything else blits normally. Supports multiple independent tinted
regions per sprite (a ship's shield-glow patch and its engine-flame patch
are separately recolorable). `blazerx.c` identifies which sprite-sheet
pixels are which region by the specific colors its own art already
reserved for the old palette registers 240–243 (see
`PlayersTintKeys`/`RemotesTintKeys`).

### `CAM_ZOOM` is 4, not 2

Doubling the resolution (640→1024, 480→768 is a clean, aspect-ratio-preserving
1.6×, unlike the 320×200→640×480 step above) doesn't by itself change how big
anything *in the game world* looks — see the camera pattern's two-category
split above. `blazerx` doubled `CAM_ZOOM` from 2 to 4 instead, a deliberate,
separate choice: it means ships/asteroids/etc. render bigger (matching the
book's proportions more closely than leaving `CAM_ZOOM` at 2 would, which
would just reveal more of the universe around a same-sized ship), using clean
integer math throughout the pervasive `CAM_ZOOM` arithmetic (positions,
collision distances, the `backColor[CAM_ZOOM*CAM_ZOOM]` particle buffers) —
the resolution's own 1.6× ratio isn't an integer, so it was never a candidate
for `CAM_ZOOM` itself. `VIEW_WIDTH`/`VIEW_HEIGHT` are 256×192 world-units
visible, a bit more zoomed in than the book's 320×240, which is the accepted
tradeoff for that clean math.

**This does not apply to UI.** Screen-space UI (scanner, HUD text/numbers/
gauge, buttons, cursor, tech font) scales with the resolution ratio (1.6×,
same category-1 rule as everywhere else in this doc), never with `CAM_ZOOM` —
they're unrelated axes. Conflating them is an easy, recurring mistake:
`blazerx`'s scanner briefly ended up sized by `CAM_ZOOM`'s 2× ratio instead of
the resolution's 1.6× ratio, which desynced it from its own (correctly
1.6×-scaled) position and made it sit wrong relative to the screen edge —
same category of bug as the chap09 scanner mistake above, just against a
different pair of resolutions. Because 640×480→1024×768 doesn't distort
aspect ratio (unlike 320×200→640×480), a plain uniform 1.6× is correct for
every UI element here — no ×2-vs-×2.4-style split needed at this step.

Scanner grid-line thickness follows the same 1.6×-of-640×480 rule as
everything else in this section: `SCANNER_LINE_THICK` is 3 here for
`chap09/blazer.c`'s `VBE_SUPPORT` branch's 2 (see `lineH2Thick`/`lineV2Thick`
above), and
`BLIP_SIZE` (the scanner blips' size, `blipSave`/`blipDraw`/`blipRestore`)
tracks it 1:1 for the same reason.

**A worked example of the same mistake recurring at the size level, not just
the axis level:** `HeadsText`'s sprite height was correctly scaled 1.6× (12
to 19), but the per-row `HeadsText.y += 16` step that walks down its label
stack in `drawHeads`/`eraseHeads` was left at the bare 640×480-era literal —
missed because it's a bare arithmetic step next to sprite draws, not a
`#define` that stands out in a scan for hardcoded numbers. Its sibling
`HeadsNumbers`, in the very same functions, *had* been correctly scaled to a
26px step. Fixed to `+= 26`, matching `HeadsNumbers`' rounding of 16×1.6=25.6.
The lesson: every literal that walks a UI element across the screen — not
just the elements' own `#define`d sizes and positions — needs the same
resolution-ratio scaling, and sibling elements in the same function are a
good cross-check when auditing for this.

### Asset pipeline (`blazerx/*.py`)

Three offline scripts, run once, not part of the build:

- `regen_assets.py` — converts every `blazerx/*.PCX` from 256-color indexed
  to 24-bit true color (lossless — `img.convert('RGB')`), and Lanczos-resizes
  the four full-screen backgrounds (`BLAZECON`/`BLAZEINS`/`BLAZEINT`/`WAITE`)
  640×480→1024×768. Sprite sheets keep their pixel dimensions in this pass
  (resized separately, below, by whichever axis actually applies to them).
  Sprite sheets that are ever drawn transparently get their palette index 0
  remapped to magic magenta `(255,0,255)` first — the engine's transparent
  key, since pure black is a real, heavily-used color in ship/asteroid art
  and can't stay the sentinel once assets are true RGB.
- `rescale_ui_chrome.py` — rescales the **screen-space UI** sprite sheets
  (`BLAZEBT1`/`BT3`/`DIS` buttons, `BLAZEHU1`/`HU2` HUD text/numbers/gauge,
  `BLAZEFNT` tech font) by the resolution ratio, 1.6×.
- `rescale_game_world.py` — rescales the **game-world** sprite sheets
  (`BLAZESHL`/`SHR` ships, `BLAZEALN` alien, `BLAZEEXP` explosions,
  `BLAZEFUL` fuel cells, `BLAZELAS`/`MAS`/`SAS` asteroids, `BLAZEWRM`
  wormhole) by `CAM_ZOOM`'s ratio, a clean 2×.

Both sprite-sheet scripts resize **per-cell with nearest-neighbor**, not
Lanczos, reusing the grid-layout math from `scale_assets.py` above (1-pixel
borders, cell positions from the `pcxGetSprite` calls) — nearest-neighbor
because several of these sheets use exact-color-key transparency, and any
smooth resampling filter blends the magenta key into a visible fringe at
sprite edges. Full-screen backgrounds have no transparency key, so Lanczos
is safe and strictly better there (smoother scaling of painted scenes vs.
blocky nearest-neighbor).

---

## Assets

**Fonts** — bit-packed square-cell font files, generated by the DOS tool
`exp_font` (`exp_font/exp_font.c`), which extracts the BIOS 8×8 ROM font and
writes `font.bin` (8×8) plus scaled `fontNN.bin` variants (`font16.bin`,
`font24.bin`, `font32.bin`, ...) in the same run. `loadFontSet(filename,
size)` (`VBE_SUPPORT` only) loads any of them —
`GlyphWidth`/`GlyphHeight` become runtime state that
`printChar`/`printString`/`printCharDb`/`printStringDb` read, so the same
drawing calls work at any glyph size, resolution, or bpp. Pick the size that
matches the demo's resolution: `font16.bin` (16×16) for 640×480, `font24.bin`
(24×24) for 800×600, `font32.bin` (32×32) for 1024×768 — same mapping as the
book's own mode-13h-to-SVGA scale ratios. A `DOS_32_BIT`-alone build (no
`VBE_SUPPORT`) at mode-13h's 320×200 calls `initRomCharSet()` instead — the
pre-VESA fixed 8×8, no-argument font bridge, `font.bin` only (see
**Three build configurations** above). A demo with its own bitmap font sheet
(e.g. `chap09`'s `blazefnt.pcx`) doesn't use either system — scale the sheet
itself instead (see the chap09 notes above).

**PCX files** — must be **256-color (8-bit indexed) PCX** at 8bpp, or
**24-bit true-color PCX** (3 color planes, no palette) at 32bpp — see
**blazerx** above. RLE-encoded with a standard palette trailer (indexed
only). Demos load by bare filename, so they must be in the working
directory at runtime — for the mode-13h build that's the book's own
`chapXX/`; for the `VBE_SUPPORT` build, the resolution-scaled copies live in
`vbe/chapXX/` instead (the demo's *source* is `chapXX/foo.c` either way —
only the runtime asset directory differs between the two builds of the same
file).

The book's original PCX files are 320×200. Resized copies for the
`VBE_SUPPORT` build go in `vbe/chapXX/` at the target resolution.

**Backgrounds** — scale the whole image to the target resolution:

```python
from PIL import Image
img = Image.open('in.pcx')
img.resize((640, 480), Image.NEAREST).save('out.pcx')
```

**Sprite sheets** — do NOT scale the whole sheet. The book's sprite sheets have 1-pixel grid lines between cells. Scaling the whole image 2× makes those grid lines 2 pixels wide, but `pcxGetSprite` always assumes 1-pixel grid lines. The extraction offsets compound across columns and the wrong pixels get extracted.

The correct approach is to scale each cell individually and reconstruct the sheet with 1-pixel grid lines:

```python
from PIL import Image

def scale_sprite_sheet(src_path, dst_path, cell_w, cell_h, layout):
    # layout = list of row cell counts, e.g. [12, 4]
    src = Image.open(src_path)
    dw, dh = cell_w * 2, cell_h * 2
    max_cols = max(layout)
    out = Image.new('P', (max_cols*(dw+1)+1, len(layout)*(dh+1)+1), 0)
    out.putpalette(src.getpalette())
    for row_idx, cols in enumerate(layout):
        for col_idx in range(cols):
            sx = (cell_w + 1) * col_idx + 1
            sy = (cell_h + 1) * row_idx + 1
            cell = src.crop((sx, sy, sx + cell_w, sy + cell_h))
            cell = cell.resize((dw, dh), Image.NEAREST)
            out.paste(cell, ((dw+1)*col_idx+1, (dh+1)*row_idx+1))
    out.save(dst_path)
```

`layout` comes from reading the `pcxGetSprite` calls in the demo — each call's `(cellX, cellY)` arguments tell you the grid layout. `cell_w` and `cell_h` are half the doubled dimensions in `spriteInit`.

**PLG files** (chap11+, 3D) — ASCII geometry, resolution-independent. Copy as-is.

---

## Watcom IDE project files (`.wpj` / `.tgt`)

These are **line-based text** (a serialized object graph), not binary. Every
`WString` / `MItem` / `WFileName` line is preceded by a **length-prefix
line** whose value must equal the exact character count of the string that
follows. Get this wrong and the IDE silently corrupts or ignores the entry.
A field holding multiple values (e.g. several `-d` compiler defines) is one
`WString` containing all of them space-separated — the same convention the
include-path field (`d????WLANG_i`) uses for `"$(%watcom)/h" "../../engine"`.

Every `vbe/chapXX/*.tgt` file's engine-source entries point at
`..\..\engine\black3.c` and `..\..\engine\black4.c` (plus `dpmi.c`, needed
internally by `setGraphicsModeVesa`, and `black5.c`/`black6.c`/`black8.c`/
`black9.c` where the demo needs them, e.g. `blazer.tgt`), and its demo-source
entry points at `..\..\chapXX\foo.c` — see **Naming & layout** above for the
mapping, including `chap03/mode13.c` serving `svga.tgt`, `svga_l32.tgt`, and
`svga_m16.tgt` all three. `blazerx.tgt` has the same engine-source entries.

The **Build & verify** section below gives manual `wcc386`/`wcl386`
commands that already reference `black3.c`/`black4.c`/`chapXX/foo.c`
correctly and work today — useful for a syntax/link check without going
through the IDE project files at all.

### Things to change in a new `foo.tgt` (copy from an existing chapter's `.tgt`)

| What | Line(s) | Rule |
|---|---|---|
| EXE filename | after `MItem` near top | `foo.exe` — prefix = `len("foo.exe")` |
| `WPickList` count | `WPickList` line | Counts the `*.c` filter entry + every source file. If too small, the demo `.c` is silently dropped. |
| Demo `.c` path | last `MItem` block | `..\..\chapXX\foo.c` — prefix = `len("..\..\chapXX\foo.c")`. The engine source blocks above it are fixed and need no change. |
| Compiler defines | `WCC` define list (`?????WLANG_d`) | Add `VBE_SUPPORT` alongside the existing `DOS_32_BIT` entry. |

**Length-prefix rule:** count the characters in the string on the next
line. The engine paths: `..\..\engine\black3.c`=21, `..\..\engine\black4.c`=21,
`..\..\engine\dpmi.c`=19, `..\..\engine\black5.c`=21. The demo path is
chapter-dependent — e.g. `..\..\chap09\blazer.c`=21, `..\..\chap04\alien.c`=20.

Source files for a 2D demo without input: `..\..\engine\black3.c`,
`..\..\engine\black4.c`, `..\..\engine\dpmi.c`, `..\..\chapXX\foo.c`. Add
`..\..\engine\black5.c` before the demo source for demos that use keyboard,
mouse, or joystick.

### Things to change in `foo.wpj` (copy from an existing chapter's `.wpj`)

The `.tgt` filename appears twice, each preceded by its length (e.g.
`jumper.tgt`=10) — unchanged, since the `.wpj`/`.tgt` filenames themselves
stay in `vbe/chapXX/` even though the `.tgt`'s own source-file reference
points to `..\..\engine\`/`..\..\chapXX\`.

### Validate length prefixes before committing

```sh
awk '/^(WString|MItem|WFileName)$/{getline n; getline s; if(length(s)!=n+0){print "MISMATCH: prefix="n" actual="length(s)" ["s"]"; bad=1} else ok++} END{printf "%d prefixes %s\n", ok, (bad?"-- MISMATCH":"ok")}' vbe/chapXX/foo.tgt
```

---

## Build & verify

Open Watcom 2.0. Use the **64-bit-host** tools (`binnt64`).

```sh
export WATCOM="C:\\WATCOM"
export PATH="/c/WATCOM/binnt64:$PATH"
export INCLUDE="C:\\WATCOM\\H"

# syntax check (VBE_SUPPORT build)
wcc386 -zs -mf -6r -bt=dos -i=engine -dDOS_32_BIT -dVBE_SUPPORT chapXX/foo.c

# compile + link (2D demo, no input) - .exe/assets run from vbe/chapXX/
wcl386 -mf -6r -bt=dos -l=dos4g -i=engine -dDOS_32_BIT -dVBE_SUPPORT -fe=vbe/chapXX/foo.exe \
    chapXX/foo.c engine/black3.c engine/black4.c engine/dpmi.c

# compile + link (2D demo with keyboard/mouse/joystick)
wcl386 -mf -6r -bt=dos -l=dos4g -i=engine -dDOS_32_BIT -dVBE_SUPPORT -fe=vbe/chapXX/foo.exe \
    chapXX/foo.c engine/black3.c engine/black4.c engine/dpmi.c engine/black5.c

# the same source, without -dVBE_SUPPORT, builds the plain book-original
# mode-13h demo instead (16-bit or 32-bit, same as any other chapXX/*.c)
```

Do not pass `-za99` — it rejects the `_asm` blocks and `#pragma aux`
functions in `black3.c`/`black4.c`/`dpmi.c`. After verifying, delete build
artifacts (`*.obj *.exe *.map *.sym *.lk1 *.mk *.mk1 *.err`) — do not commit
binaries.

### Run

DOSBox-X with `machine=svga_s3` provides VBE 2.0 and a linear framebuffer.
Mount the repo, `cd vbe/chapXX` (where the `.exe` and its VESA-scaled
runtime assets live), run the `.exe`. On real DOS or plain DOSBox, put
`dos4gw.exe` in the run directory.

---

## Assembly in the engine layer

This applies to the `VBE_SUPPORT` build's bulk fill/blit paths in
`black3.c`/`black4.c` only — demo ports stay pure C, and the
single-pixel/per-row functions (`writePixel`, `lineV`, etc.) are plain C
too, with no assembly involved (see **Header API reference** above — the
runtime bpp `switch` plus a pointer computation is all they need).

A non-`VBE_SUPPORT` build uses none of this — it keeps a separate, much
smaller set of hand-written `_asm` blocks instead (fixed 320×200×8, no bpp
dispatch to fill), not `#pragma aux` helpers at all: the book's own blocks
in a 16-bit build, or their pre-VESA 32-bit port otherwise (see **Three
build configurations** above).

`black3.h` declares these helpers once, `#ifdef VBE_SUPPORT`-gated, but
each is `static` — so `black3.c` and
`black4.c` (both include the header) each get their **own, file-private
compiled copy**, not a shared symbol — since Watcom's `#pragma aux` binds a
specific register-parameter convention at the declaration site, so each
translation unit that calls one of these needs its own matching
declaration+pragma pair.

### Use `#pragma aux`, not `_asm`

`#pragma aux` defines a function whose body is assembly, with explicit `parm` and `modify` clauses so the compiler knows exactly which registers are touched:

```c
static void asmFill(void* dst, unsigned long val, unsigned dwords, unsigned rem);
#pragma aux asmFill = \
    "cld"           \
    "rep stosd"     \
    "mov ecx, ebx"  \
    "rep stosb"     \
    parm   [edi] [eax] [ecx] [ebx] \
    modify [edi ecx ebx];
```

### Always `cld` first

The x86 direction flag can be left set by a BIOS call or interrupt handler. `cld` as the first instruction of any REP helper costs 1 cycle and prevents silent backwards-memory corruption.

### Per-bpp fill instruction

| bpp | Pack EAX | Instruction | Pixels/op |
|---|---|---|---|
| 8 | `c\|=c<<8; c\|=c<<16` | `rep stosd` | 4 |
| 16 | `c\|=c<<16` | `rep stosd` | 2 |
| 32 | direct | `rep stosd` | 1 |
| 24 | n/a | C loop | 1 |

24bpp has never had a real code path in this engine (its 3-byte,
non-power-of-2 stride doesn't fit the `rep stosd`-based fill tricks the
8/16/32 paths use) — every drawing function's bpp `switch` handles only 8,
16, and 32 (`default:` is the 32bpp case), matching what `setGraphicsModeVesa`
and every PCX/sprite/bitmap path actually produce.

### `displayDoubleBuffer` fast path (`VBE_SUPPORT` build)

When the double buffer's pitch equals `DisplayPitch` (always true in a
`VBE_SUPPORT` build — it's allocated at `DisplayPitch` per row specifically
so this holds), the entire frame copies in one `rep movsd`-based call via
the shared `asmMovDwordsBytes` helper (DWORDs, then a trailing-byte tail):

```c
static void asmMovDwordsBytes(void* dst, const void* src, unsigned dwords, unsigned rem);
#pragma aux asmMovDwordsBytes = \
    "cld" "rep movsd" "mov ecx, ebx" "rep movsb" \
    parm [edi] [esi] [ecx] [ebx] modify [edi esi ecx ebx];
```

### Sprite optimization

Clip the sprite rectangle to surface bounds once before the row loop, then use `memcpy` per row (opaque) or a per-pixel color-key check (transparent). Eliminates per-pixel bounds checks from the inner loop.

### When not to use assembly

| Operation | Verdict |
|---|---|
| `lineV` | C — memory latency dominates; asm gains nothing |
| Bresenham line | C — Watcom `-otexan` handles it well |
| MMX | Avoid — EMMS flush conflicts with FPU use in demos |
| MTRR write-combining | Impossible — `WRMSR` is ring-0; DOS/4GW is ring 3 |

---

## chap11: 3-D projection & the resolution-aware rasterizer

`engine/black11.c` (the wireframe/flat/gouraud/textured 3-D pipeline shared by
chap11-13, also reused as-is by chap14) predated the runtime `DisplayWidth`/
`DisplayHeight`/`PITCH_OFFSET` generalization above — every rasterizer and
the perspective-projection math itself was hardcoded to 320×200. This
section covers how that was generalized.

### What's hardcoded

**Rasterizer addressing** — `drawLine`, `drawTopTriangle`,
`drawBottomTriangle`, `drawTriangle2DGouraud`, and `drawTriangle2DText` all
compute the first row's address as `(y<<8)+(y<<6)` (`y*320`) and step
`+= 320` per row:
```c
vbStart = vbStart + ((y0 << 6) + (y0 << 8) + x0);   // drawLine
...
yInc = 320;                                          // drawLine, dy>=0 case
...
destAddr = DoubleBuffer + (y1 << 8) + (y1 << 6);      // the four triangle fills
for (...; ...; destAddr += 320) { ... }
```
All writes are `unsigned char` (`destAddr[x] = color`) — the whole file
assumes 8bpp, one byte per pixel, x used directly as a byte offset.

**Perspective projection** — every draw function (`drawObjectWire`, and the
flat/gouraud/textured fill paths reached through `drawObjectSolid`/
`drawPolyList`) independently inlines the same formula at its own call site
(no shared "project point" function — consistent with the book's style
elsewhere in this file):
```c
x1 = HALF_SCREEN_WIDTH  + x1 * ViewingDistance / z1;
y1 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y1 * ViewingDistance / z1;
```
`HALF_SCREEN_WIDTH`/`HALF_SCREEN_HEIGHT` are `#define`d 160/100.
`ViewingDistance` is a runtime `int` (book default 200, some demos set 250)
— the focal length of the pinhole camera, in the *same units as
HALF_SCREEN_WIDTH*, i.e. pixels. Two visibility/culling tests
(`clipObject3D`'s `x1Compare`/`y1Compare`, `removeObject`'s bounding-sphere
`xCompare`/`yCompare`) use the same constants.

**Clip rect** — `PolyClipMinX/MinY/MaxX/MaxY` are already runtime `int`s
(defaulting from `POLY_CLIP_MIN/MAX_X/Y`, 0/0/319/199) — this part of the
generalization is already done, just needs setting correctly.

### Addressing: same fix as black3.c/black4.c

Replace `(y<<8)+(y<<6)` with `PITCH_OFFSET(y)` and `320` with `DisplayPitch`
at every site above. No `DisplayBppShift` needed — chap11-14 stay 8bpp-only
(see Scope below), so x is still a direct byte offset. `triangleLine`'s
`#pragma aux` span-fill (fills `xe-xs+1` bytes at whatever address it's
given) needs **no change** — it doesn't know about resolution, only about a
byte count and an address, both already correct once the caller's `destAddr`
is.

**`PITCH_OFFSET`/`DisplayPitch` only exist under `VBE_SUPPORT`** (`black3.h`
declares them inside `#ifdef VBE_SUPPORT`, same as `DisplayWidth`/
`DisplayHeight` — every `black3.c` function that uses them wraps its body
in `#ifdef VBE_SUPPORT ... #else` with the book's original fixed-320
addressing). `black11.c` is shared by every chap11-14 build — 16-bit,
32-bit `DOS_32_BIT`-without-`VBE_SUPPORT`, and 32-bit `VBE_SUPPORT` — so
referencing them unconditionally is a hard compile error outside
`VBE_SUPPORT`, not just a latent bug. Two macros in `black11.h` (not a
`.c` file — `black15.c` needs the identical pair too, see the chap15
section below, so this lives in the shared header rather than being
copy-pasted per engine file) resolve this once instead of an `#ifdef`/
`#else` at every one of the ten call sites (several of which are inside a
`for(...)` loop's increment clause, where a bare `#ifdef` can't be
embedded mid-statement):
```c
#ifdef VBE_SUPPORT
#define ROW_OFFSET(y) PITCH_OFFSET(y)
#define ROW_PITCH     DisplayPitch
#else
#define ROW_OFFSET(y) (((y) << 6) + ((y) << 8))
#define ROW_PITCH     320
#endif
```
Every `PITCH_OFFSET(y)`/`DisplayPitch` reference in `drawLine`,
`drawTopTriangle`, `drawBottomTriangle`, `drawTriangle2DGouraud`, and
`drawTriangle2DText` becomes `ROW_OFFSET(y)`/`ROW_PITCH` instead.

### Projection: HALF_SCREEN_WIDTH/HEIGHT and ViewingDistance must scale together

Naively setting `HALF_SCREEN_WIDTH = DisplayWidth/2` while leaving
`ViewingDistance` at its book value would **change the field of view** — FOV
is `2*atan(HALF_SCREEN_WIDTH/ViewingDistance)`, so widening the numerator
alone widens the FOV (fisheye-like distortion at higher resolution, not just
more pixel density). To render the same shot at higher resolution, both must
scale by the same factor:
```c
HalfScreenWidth  = DisplayWidth  / 2;
HalfScreenHeight = DisplayHeight / 2;
ViewingDistance  = (int)(BOOK_VIEWING_DISTANCE * ((float)DisplayWidth / 320.0f));
```
`HalfScreenWidth`/`HalfScreenHeight` are new runtime `int`s (`HALF_SCREEN_WIDTH`/
`HALF_SCREEN_HEIGHT` stay as-is — same relationship as the `POLY_CLIP_MIN_X`
macro to the `PolyClipMinX` variable above) that every projection/culling
call site switches to reading. `ViewingDistance` scaling is the demo's own
responsibility — same as it already picks its own book-value
`ViewingDistance` (200 default, 250 in `wiredemo.c`/`solidemo.c`/
`sol2demo.c`) — so each demo's existing `ViewingDistance = 250;` line becomes
`ViewingDistance = (int)(250 * ((float)DisplayWidth / 320.0f));` under
`VBE_SUPPORT`.

### ASPECT_RATIO: a mode-13h-only correction, not a general one

`ASPECT_RATIO` (0.8) and `INVERSE_ASPECT_RATIO` (1.25) correct for mode-13h's
non-square pixels: 320×200 pixels stretched across a 4:3 CRT means each pixel
is physically ~0.833 units wide per unit tall (`(4/3) / (320/200) = 0.833`,
the book rounds to 0.8), so the Y-projection multiplies by that factor to
keep world-space squares looking square on screen. Every VESA resolution
this project targets (640×480, 1024×768) is exactly 4:3 with pixel count
matching that ratio — **square pixels, no correction needed**. General
formula, correct for any resolution including non-4:3 ones if this ever
targets a widescreen VESA mode:
```c
AspectRatio        = (4.0f/3.0f) / ((float)DisplayWidth / DisplayHeight);
InverseAspectRatio = 1.0f / AspectRatio;
```
At 640×480 or 1024×768 this evaluates to `1.0` exactly. `ASPECT_RATIO`/
`INVERSE_ASPECT_RATIO` macros stay as the mode-13h book defaults; every call
site switches to the runtime `AspectRatio`/`InverseAspectRatio` variables.

### No demo ever calls anything - it's all internal to black11.c

The first version of this design had a public `set3DViewport()` that any
demo drawing a 3D `Object` (or calling `drawTriangle2D`) had to remember to
call once after its mode-set. That's a bad API in practice: it makes the
*caller* responsible for knowing an internal fact about `black11.c` -
which of its functions happen to read `HalfScreenWidth`/`AspectRatio`/
`PolyClip*` and which don't (`drawTriangle2DGouraud`/`drawTriangle2DText`
don't; `drawObjectWire`/`drawObjectSolid`/`drawTriangle2D`/`drawPolyList`/
`clipObject3D`/`removeObject` do) - a distinction nothing about a demo's
own code makes obvious, and a wrong guess fails silently (stale clip rect
or projection extents, not a compile error).

The fix: there is no public function at all. `HalfScreenWidth`/
`HalfScreenHeight`/`AspectRatio`/`InverseAspectRatio`/`PolyClip*` are pure
functions of `DisplayWidth`/`DisplayHeight`, which are always valid - 320×200
under `GRAPHICS_MODE13`, the negotiated VESA mode under `VBE_SUPPORT`. A
`static` (file-private, not in `black11.h`) helper keeps them in sync,
cheaply skipping the work when nothing has changed - and, since
`DisplayWidth`/`DisplayHeight` themselves only exist under `VBE_SUPPORT`
(same reason `PITCH_OFFSET` needed the `ROW_OFFSET`/`ROW_PITCH` wrapper
above), the whole thing is wrapped in `#ifdef VBE_SUPPORT`, not just its
body - a non-`VBE_SUPPORT` build has no `setGraphicsModeVesa` to ever
change the display away from mode-13h's fixed 320×200, so the mode-13h
defaults `HalfScreenWidth` etc. already carry are always correct and this
mechanism should compile out to nothing there, not merely become a no-op
function that still gets called:
```c
#ifdef VBE_SUPPORT
static int CachedDisplayWidth  = MODE13_WIDTH;   // seeded to the mode-13h
static int CachedDisplayHeight = MODE13_HEIGHT;  // default - already correct
                                                  // for HalfScreenWidth etc.'s
                                                  // own mode-13h-macro defaults

static void resyncCachedSettings(void) {
    if (DisplayWidth == CachedDisplayWidth && DisplayHeight == CachedDisplayHeight) {
        return; // skip the float divide below if the display hasn't changed
    }
    HalfScreenWidth    = DisplayWidth  / 2;
    HalfScreenHeight   = DisplayHeight / 2;
    AspectRatio        = (4.0f/3.0f) / ((float)DisplayWidth / DisplayHeight);
    InverseAspectRatio = 1.0f / AspectRatio;
    PolyClipMinX = 0;              PolyClipMinY = 0;
    PolyClipMaxX = DisplayWidth-1; PolyClipMaxY = DisplayHeight-1;
    CachedDisplayWidth = DisplayWidth; CachedDisplayHeight = DisplayHeight;
}
#endif
```
`resyncCachedSettings()` is called as the first statement of every function
that reads this state - `clipObject3D`, `removeObject`, `drawObjectWire`,
`drawObjectSolid`, `drawPolyList`, `drawTriangle2D` - but each of those six
call sites is itself wrapped in `#ifdef VBE_SUPPORT`/`#endif` too, for the
same reason: under a non-`VBE_SUPPORT` build there's no function to call at
all, so this isn't just "a cheap call that does nothing" there, it's zero
added instructions, matching the book's original zero-overhead addressing
exactly. Under `VBE_SUPPORT` it's always correct with zero calls from any
demo, and the expensive `AspectRatio` division only actually reruns the
first time a demo's frame loop touches one of those functions after a
resolution change, not every frame. `drawTriangle2DGouraud`/
`drawTriangle2DText` never call it at all, in any build, because they don't
read any of this state - not because a demo author had to notice that.

The perf reasoning behind caching `AspectRatio`/`InverseAspectRatio` at all
(rather than also computing them inline everywhere, the way `HalfScreenWidth`/
`HalfScreenHeight` safely could - `DisplayWidth >> 1` costs the same as a
cached read): this project targets 486-era DOS hardware, some of it without
an FPU, where a float divide can run to hundreds of cycles in software
emulation. Recomputing it per vertex/frame would be a real cost; the
dirty-check cache avoids that while still needing no caller involvement.

Demo call sites only need `#ifdef VBE_SUPPORT` around the mode-set and
`ViewingDistance` scaling, same shape as every existing ported demo - no
separate viewport call at all:
```c
#ifdef VBE_SUPPORT
    setGraphicsModeVesa(640, 480, 8);
#else
    setGraphicsMode(GRAPHICS_MODE13);
#endif
#ifdef VBE_SUPPORT
    ViewingDistance = (int)(250 * ((float)DisplayWidth / 320.0f));
#else
    ViewingDistance = 250;
#endif
```

### Scope

8bpp only, same as every other VBE port in this repo except blazerx —
`black11.c`'s colors are `unsigned char` palette indices baked into every
rasterizer (`destAddr[x] = (unsigned char)color`); extending to 16/32bpp
would need the same widened-color-type work blazerx's 2D sprites needed, and
nothing has asked for a true-color 3-D demo. The engine fix applies
uniformly to every chap11-14 demo that calls into `black11.c` (wireframe,
flat, gouraud, textured) — one engine-layer fix, not a per-demo one, same
as the black3.c/black4.c generalization applied to every 2D demo at once.

### Status

Both chap11 demos are ported. `chap11/linedemo.c` (wireframe only) was the
proof of concept — it only calls `drawLine` directly, which reads none of
`black11.c`'s resolution-aware state. `chap11/wiredemo.c` is the first demo
to exercise the actual projection path (`drawObjectWire`, driven by a `.PLG`
object file passed on the command line — `CUBEW.PLG`/`PYRAMIDW.PLG`, copied
into `vbe/chap11/`): it scales its book-value `ViewingDistance = 250` by
`DisplayWidth / 320.0f` to keep the same field of view at 640×480 (see
"Projection: HalfScreenWidth/Height and ViewingDistance must scale
together" above) — `HalfScreenWidth`/`AspectRatio`/the clip rect resync
automatically the first time `drawObjectWire` runs after the mode-set, no
call needed. It also switches from `initRomCharSet()` to
`loadFontSet("font16.bin", 16)` for its on-screen position readout, same as
every other `VBE_SUPPORT` demo that prints text. `chap12/tridemo.c` is
ported next — it draws random flat triangles directly via `drawTriangle2D`
(`drawTopTriangle`/`drawBottomTriangle`), no `Object`/projection involved,
but the clip rect still matters: `drawTopTriangle`/`drawBottomTriangle`
clip against `PolyClipMinX`/`MinY`/`MaxX`/`MaxY`, and `drawTriangle2D`'s own
internal resync keeps those matching the active display rather than stuck
at mode-13h's 0/0/319/199. `chap12/gourdemo.c` is ported too — it draws a
single interactive gouraud-shaded triangle via `drawTriangle2DGouraud`
directly (arrow/number keys retune each vertex's intensity), the first demo
to exercise that function. `drawTriangle2DGouraud` doesn't read
`PolyClipMinX`/`MinY`/`MaxX`/`MaxY`/`HalfScreenWidth`/`AspectRatio` at all,
so nothing resyncs for it (nor should it) - but its triangle position and
text layout are **screen-only UI** (fixed demo furniture, not a projected
`Object`), so they follow the flat-scaling convention from "Coordinate
constants & the camera pattern" above: every screen-relative literal
(vertex offsets, label margins, HUD line spacing) is doubled ×(640/320) for
X and ×(480/200) for Y under `VBE_SUPPORT`, same as `chap03/light.c`'s
`playerX`/`playerY`. `chap12/solidemo.c` mirrors `wiredemo.c`'s port
exactly (`plgLoadObject` from argv[1] — `CUBE.PLG`, plus a `standard.pal` 3D
color palette, both copied into `vbe/chap12/`; `ViewingDistance` scaled by
`DisplayWidth / 320.0f`; `loadFontSet("font16.bin", 16)`/`freeFontSet()`),
just calling `drawObjectSolid` (backface removal + shading via
`removeBackfacesAndShade`) instead of `drawObjectWire`. `chap12/textdemo.c`
is the last chap12 demo — a texture-mapped triangle (`drawTriangle2DText`,
no clip-rect/projection dependency either, same as `drawTriangle2DGouraud`)
sampling from a 4-cell texture strip (`textures.pcx`: stone/wood/slime/
lava). Its screen position/offsets got the same screen-only-UI doubling as
`gourdemo.c`, but the texture atlas itself needed the **sprite sheet**
scaling recipe from Assets below (per-cell rescale + 1px grid-line
reconstruction, not a whole-image resize) — `spriteInit`'s cell size moves
from 64 to 128 to match.

Porting `textdemo.c` surfaced a genuine engine bug in `drawTriangle2DText`,
independent of the addressing/projection generalization above: its texel
sampling had its own hardcoded `63`/`64` (`xIndex = abs((int)(uCurr * 63 +
0.5))`, `text[yIndex * 64 + xIndex]`) baked in from the book's fixed 64×64
texture size — a *texture-stride* constant, a third category this file
hardcoded beyond screen addressing and projection. At the book's own 64×64
it's numerically identical to `Textures.width`/`height - 1` and `.width`,
so it was invisible until a demo actually used a differently-sized texture
(the VBE port's 128×128 atlas) - sampling silently stayed inside the
top-left quarter of the bitmap with the wrong row stride, corrupting the
output. Fixed to read `Textures.width`/`Textures.height` instead, in both
loops - unconditional, not `VBE_SUPPORT`-gated, byte-identical at 64×64.

`chap13/sol2demo.c` completes the set — it moves the camera (`ViewPoint`/
`ViewAngle`, via `createWorldToCamera`/`worldToCameraObject`) instead of the
object, otherwise mirroring `solidemo.c`'s port exactly (`CUBE.PLG`/
`standard.pal` copied into `vbe/chap13/`, `ViewingDistance` scaled by
`DisplayWidth / 320.0f`, `loadFontSet`/`freeFontSet`). It prints two
stacked `printStringDb` lines (viewpoint, view angle) 10px apart in the
book - with `font16.bin`'s 16px-tall glyphs that would overlap, so the
line spacing doubles to 20 under `VBE_SUPPORT`, same reasoning as
`gourdemo.c`'s HUD spacing.

All 7 chap11-13 demos are now ported (`linedemo`, `wiredemo`, `tridemo`,
`gourdemo`, `solidemo`, `textdemo`, `sol2demo`), exercising every rasterizer
path this design covers: wireframe, flat, gouraud, textured, and both
object-movement and camera-movement solid viewers.

A review after porting turned up a real, build-breaking oversight in the
engine work above: `PITCH_OFFSET`/`DisplayPitch`/`DisplayWidth`/
`DisplayHeight` only exist under `VBE_SUPPORT` (`black3.h`), but the
initial `black11.c` generalization referenced them unconditionally - a hard
compile error for the 16-bit and plain-32-bit (`DOS_32_BIT` without
`VBE_SUPPORT`) builds every chap11-13 demo also ships as. Fixed with local
`ROW_OFFSET`/`ROW_PITCH` macros (see "Addressing: same fix as
black3.c/black4.c" above) and by wrapping `resyncCachedSettings()` and its
six call sites entirely in `#ifdef VBE_SUPPORT`, so a non-`VBE_SUPPORT`
build compiles away the whole mechanism rather than paying for a call that
does nothing.

`chap14/objects.c` is also ported, reusing `black11.c` directly - chap14
has no separate engine snapshot of its own. It's `sol2demo.c`'s camera
pattern plus `removeObject`'s bounding-sphere culling test
(`OBJECT_CULL_XYZ_MODE`, first demo in this project to exercise it), four
objects arranged side by side. Its "Objects Removed" status line and
per-object index labels sit near the bottom of the screen (book y=180 of
200) and scale the same way as `gourdemo.c`'s HUD (×2 X, ×2.4 Y, plus the
10→20 line-spacing bump for `font16.bin`). Note: the existing
`32bit/chap14/objects.tgt` links `black15.c`/`black15.h` even though
`objects.c` neither includes `black15.h` nor calls any of its functions
(`drawTbTri3DZ`, `createZBuffer`, `bspWorldToCamera`, etc.) - an extraneous
dependency from earlier module-list work, not carried into
`vbe/chap14/objects.tgt`.

---

## chap15: Z-buffered rendering & BSP trees

`engine/black15.c` (`#include "black11.h"`, reusing black11.c's projection
and clip-rect machinery rather than duplicating it) adds Z-buffered
rendering and BSP-tree wall traversal on top of chap11-14's already-ported
pipeline. Four demos sit on it: `chap15/zdemo.c`, `sortdemo.c`,
`solzdemo.c`, `bspdemo.c`. This section covers how the engine was
generalized; `zdemo.c` is ported as the proof of concept, the other three
are not yet.

### What's hardcoded

**Rasterizer addressing** — `drawTbTri3DZ` is the only rasterizer in the
file, and has the exact same book-fixed `(y<<8)+(y<<6)`/`320` pattern
chap11-14's rasterizers had before their port:
```c
destAddr = DoubleBuffer + (y1 << 8) + (y1 << 6);   // black15.c:171
ZBuffer  = ZBank1 + (y1 << 8) + (y1 << 6);          // black15.c:175, 179 (ZBank2)
...
destAddr += 320; ZBuffer += 320;                    // black15.c:238-239, 313-314
```

**Stale projection macros** — `drawPolyListZ` still reads the *compile-time*
`HALF_SCREEN_WIDTH`/`HALF_SCREEN_HEIGHT`/`ASPECT_RATIO` macros directly:
```c
x1 = HALF_SCREEN_WIDTH + x1 * ViewingDistance / z1;   // black15.c:464-471
y1 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y1 * ViewingDistance / z1;   // black15.c:487-488
```
This is different from the other two items — it's not undesigned, it's
*behind*: `black11.c`'s own functions already switched these four macro
references to the runtime globals `HalfScreenWidth`/`HalfScreenHeight`/
`AspectRatio`/`InverseAspectRatio` (kept correct by `resyncCachedSettings()`,
see the chap11 section above). `black15.c` was never updated to match.

**Z-buffer sizing — a genuinely new problem, not a repeat of chap11-14's**
— `createZBuffer` sizes its two banks from a hardcoded row width:
```c
#ifdef DOS_32_BIT
ZBankSize = ZHeight2 * (unsigned int)1280;  // 320 * 4 bytes (32-bit int)
#else
ZBankSize = ZHeight2 * (unsigned int)640;   // 320 * 2 bytes (16-bit int)
#endif
```
`1280`/`640` bake in *both* the fixed 320-pixel row width *and*
`sizeof(int)`. This can't be expressed with `PITCH_OFFSET`/`DisplayPitch` —
those track the *color* framebuffer's byte-pitch, which varies with
`DisplayBpp` (8/16/32 bits per pixel); the Z-buffer is always one `int` per
pixel column, completely independent of `DisplayBpp`. A VBE port needs its
own runtime Z-buffer row-stride, derived from `DisplayWidth * sizeof(int)`,
not reused from the color-buffer machinery. The same `(y<<8)+(y<<6)`/`320`
Z-buffer addressing above needs this new stride, not `ROW_OFFSET`/
`ROW_PITCH` — those two are pixel-width-based, not byte-pitch-based.

(The Z-buffer's `unsigned int` size computation could in principle overflow
at high resolutions in a 16-bit build, but `VBE_SUPPORT` always implies
`DOS_32_BIT`, where `unsigned int` is 32-bit flat — not a real risk for
this port.)

**No texture-stride-class bug** — chap15 has no texture sampling (flat
shading only), so the class of bug found in chap11-13's `drawTriangle2DText`
doesn't recur here.

**BSP tree code itself needs no changes** — `bspWorldToCamera`/
`bspTranslate`/`bspShade`/`bspTraverse`/`bspDelete`/`bspPrint`/`bspView`/
`buildBspTree`/`intersectLines` operate on `Wall`/`Point3D`/`Vector3D`
floats and reuse `ClipNearZ`/`ClipFarZ`/`PolyClipMinX` etc. from black11.c
(already resolution-aware) and `drawPolyList` (not `drawPolyListZ` — the
BSP demo doesn't Z-buffer, it sorts).

### The fix

1. **`drawTbTri3DZ`'s color-buffer addressing**: same `ROW_OFFSET(y)`/
   `ROW_PITCH` macros already defined in `black11.h` (shared, not
   duplicated, since both `black11.c` and `black15.c` need the identical
   definition) — no new macro needed, just apply the existing ones to the
   five sites above.
2. **`drawPolyListZ`'s projection math**: swap the four macro references
   for the four runtime globals, exactly matching what black11.c's own
   functions already do. No new mechanism.
3. **New Z-buffer stride macros**, parallel to `ROW_OFFSET`/`ROW_PITCH` but
   pixel-width based instead of byte-pitch based:
```c
#ifdef VBE_SUPPORT
#define ZROW_OFFSET(y) ((unsigned long)(y) * DisplayWidth)
#define ZROW_PITCH     DisplayWidth
#else
#define ZROW_OFFSET(y) (((y) << 6) + ((y) << 8))
#define ZROW_PITCH     320
#endif
```
   and `createZBuffer`'s size computation becomes
   `ZHeight2 * (unsigned long)DisplayWidth * sizeof(int)` under
   `VBE_SUPPORT`, keeping the existing `1280`/`640` literals for the
   `#else` book-original path.

### Scope

`bspdemo.c` additionally has its own demo-level hardcoded constants
unrelated to `black15.c` itself — `WORLD_SCALE_X/Y/Z`/`SCREEN_TO_WORLD_X/Z`
(in `black15.h`, shared with the rest of chap15) and GUI bounding boxes
(`BSP_MIN_X/MAX_X`, `BSP_MAX_Y`, `INTERFACE_MIN_X/MAX_X`,
`GADGET_WIDTH/HEIGHT`, all in `bspdemo.c` itself), mapping its mouse-driven
2D line-editor screen pixels into 3D wall world coordinates. These need the
same demo-level `#ifdef VBE_SUPPORT`/screen-only-UI-doubling treatment as
any other chapter's fixed coordinates — separate work from the engine fix
above.

### Demo order

| Demo | Uses | Scope |
|---|---|---|
| `zdemo.c` (64 lines) | Z-buffer directly (`createZBuffer`/`fillZBuffer`/`drawTri3DZ`), two hardcoded triangles, no loop | Simplest — no keyboard, no assets, no PLG loading |
| `sortdemo.c` (276 lines) | Painter's-algorithm sort (`sortPolyList`/`drawPolyList`, already in black11.c), no Z-buffer | Interactive, PLG objects |
| `solzdemo.c` (301 lines) | Z-buffer via `drawPolyListZ` | Same interactive loop as `sortdemo.c` |
| `bspdemo.c` (691 lines) | BSP tree + mouse GUI editor, 3 PCX assets (`bspbutt.pcx`/`bsppoint.pcx`/`bspint.pcx`), its own world-scale/GUI constants | Most complex — a separate coordinate-mapping job on top of the engine fix |

`zdemo.c` is the recommended proof of concept: it exercises both new pieces
of engine work (Z-buffer stride fix, projection-macro catch-up) with none
of the demo-level coordinate-scaling noise the other three carry.

### Status

`chap15/zdemo.c` is ported. It calls `drawTri3DZ` directly with fixed
screen-space pixel coordinates for its two triangles (no projection
involved, `z` is only a depth-comparison value between the two triangles,
so it doesn't scale) — screen-only UI, doubled the same way as
`gourdemo.c`'s HUD. `createZBuffer(200)` becomes `createZBuffer(480)` to
match `createDoubleBuffer`'s existing VBE_SUPPORT convention.

Porting it surfaced one thing the design pass above missed:
`resyncCachedSettings()` was `static` (private to `black11.c`), but
`drawPolyListZ` needs it for the same reason `black11.c`'s own
`drawPolyList` does (both read `HalfScreenWidth`/`AspectRatio`), and
**`drawTri3DZ` needs it too** — it reads `PolyClipMinX/MinY/MaxX/MaxY`
directly for its trivial-rejection test, exactly like `black11.c`'s
`drawTriangle2D` does, and `zdemo.c` calls `drawTri3DZ` directly, bypassing
`drawPolyListZ` entirely. Missing this would have left `zdemo.c`'s
triangles silently clipped against the mode-13h clip rect (0/0/319/199)
even at 640×480, since nothing else in its call path would have triggered
the resync. Fixed by removing `static` from `resyncCachedSettings()` and
declaring it in `black11.h` (`#ifdef VBE_SUPPORT`-guarded, same as its
definition) — a legitimate engine-to-engine contract between cooperating
files, not a demo-facing API: no demo calls it directly, only `black11.c`'s
own functions and now `black15.c`'s `drawTri3DZ`/`drawPolyListZ`.

`chap15/sortdemo.c` is ported too, and needed no new engine work at all —
it's a multi-object (4 `Object`s) painter's-algorithm demo using
`generatePolyList`/`clipObject3D`/`removeObject`/`sortPolyList`/
`drawPolyList`, all already VBE-aware from the chap11-14 work; it never
touches `black15.c`'s Z-buffer path. The book leaves `ViewingDistance` at
its 200 default rather than setting it explicitly, so the port adds an
explicit `#ifdef VBE_SUPPORT` scaled assignment where the book has none —
without it, `ViewingDistance` would silently stay at the unscaled default
under `VBE_SUPPORT` too, since nothing else in the file ever sets it. Its
per-object "culled" index labels (`index * 26`) are screen-only UI, doubled
the same way as `gourdemo.c`'s HUD.

`chap15/solzdemo.c` is ported too — the same interactive multi-object demo
as `sortdemo.c`, but through the Z-buffer path (`drawPolyListZ`) instead of
`sortPolyList`/`drawPolyList`. `createZBuffer(200)` becomes
`createZBuffer(480)`, matching `createDoubleBuffer`'s existing convention;
same explicit `#ifdef VBE_SUPPORT` `ViewingDistance` scaling as `sortdemo.c`
(the book leaves it at its 200 default in both files); same screen-only-UI
doubling for its "culled" index labels.

`chap15/bspdemo.c` is ported too, completing chap15 — see below for its
design and implementation notes.

---

## chap15/bspdemo.c: the BSP level editor

`bspdemo.c` is a mouse-driven 2D level editor (draw wall lines, build a BSP
tree from them, fly through the result) rather than a passive demo, and
raises a design question the other three chap15 demos didn't: what should
happen to the *world* the editor produces when the editor's own screen
gets bigger?

### Screen-only UI: the editor GUI itself

Straightforward doubling, same recipe as every other chapter's fixed
coordinates — `BSP_MIN_X/MAX_X/MIN_Y/MAX_Y` (editor canvas, 0/222/0/199),
`INTERFACE_MIN_X/MAX_X/MIN_Y/MAX_Y` (control panel, 226/319/0/199),
`GADGET_WIDTH/HEIGHT` (64/12), the six `BspControls[]` gadget positions
(240,94 through 240,179), and the mouse-position readout at
`printStringDb(228, 56, ...)`. All `#ifdef VBE_SUPPORT`/`#else` pairs,
doubled ×(640/320) for X and ×(480/200) for Y, no different from
`gourdemo.c`'s HUD.

One thing generalizes for free: `convertLinesToWalls`'s wall-number label
(`printString` at the midpoint of `Lines[index]`) is computed from
`Lines[].sx/sy/ex/ey`, which already hold whatever coordinate space the
mouse mapping produces — no separate `#ifdef` needed there, only the
*hardcoded* literals above need one.

`deleteLine`'s nearest-line search (`testError = 100*abs((length1+length2)
- lengthLine)/lengthLine`) is a dimensionless percentage — scale-invariant
by construction, so it needs no changes at all regardless of resolution.

### Sprite sheets and background

`bspbutt.pcx` (14 control-button frames, 2 rows × 7 cols, 64×12 cells) and
`bsppoint.pcx` (4 pointer frames, 1 row × 4 cols, 9×9 cells) are **sprite
sheets** — the per-cell rescale recipe from Assets above (not a whole-image
resize, which would corrupt the 1px grid lines `pcxGetSprite` relies on),
doubling to 128×24 and 18×18 cells respectively. `bspint.pcx` (the
full-screen interface background) is a **background** — whole-image
rescale to 640×480, same as any other chapter's full-screen PCX.
`spriteInit`'s width/height arguments for `ControlsSpr`/`PointerSpr` move
from 64×12/9×9 to 128×24/18×18 to match.

### Mouse coordinate mapping: precedent already exists

```c
mouseX = PointerSpr.x = (mouseX >> 1) - 16;
mouseY = PointerSpr.y = mouseY;
```
`chap05/mousetst.c` (already ported) establishes the pattern for this
exact situation:
```c
#ifdef VBE_SUPPORT
HammerSprite.x = mouseX - 22;
HammerSprite.y = mouseY - 20;
#else
HammerSprite.x = (mouseX >> 1) - 16;
HammerSprite.y = mouseY;
#endif
```
The DOS mouse driver (INT 33h) reports X in a virtual range that's fixed
regardless of video mode (hence the book's `>>1` to halve it down to
mode-13h's 320-wide range) - under `VBE_SUPPORT` at 640-wide, that virtual
range already matches the screen directly, so the divide is dropped
entirely, not replaced with a different divisor. The replacement offset
isn't a clean multiple of the book's (mousetst.c's own hammer offset went
from `-16` to `-22`, not `-32`) - it has to be re-tuned by hand against the
actual rescaled sprite's hotspot, the same way mousetst.c's was. `bspdemo.c`
needs the same treatment for `PointerSpr`'s offset, and - since Y gets no
adjustment at all in the book (not even a divide, an existing asymmetry in
the mouse driver's own X-vs-Y convention) - whether Y needs adjustment
under a 480-tall `VBE_SUPPORT` mode isn't obvious from reading the code
alone and should be verified once buildable, not assumed unchanged.

### The world-scale question

The 2D lines drawn in the editor become 3D wall coordinates via:
```c
wallWorld[i].x = WORLD_SCALE_X * (SCREEN_TO_WORLD_X + Lines[index].sx);
wallWorld[i].z = WORLD_SCALE_Z * (SCREEN_TO_WORLD_Z + Lines[index].sy);
```
(`WORLD_SCALE_Y` is defined in `black15.h` but never actually referenced
anywhere - dead, book-original, ignore it.) If the editor canvas simply
gets bigger under `VBE_SUPPORT` and `WORLD_SCALE_X/Z`/`SCREEN_TO_WORLD_X/Z`
are left at their book values, the resulting BSP world is proportionally
**bigger** too - a level drawn edge-to-edge in the bigger editor spans more
world-units than the same-shaped level would in the book. That's the same
class of bug the camera pattern (`chap09/blazer.c`) exists to prevent:
`bspView`'s viewpoint movement deltas (`ViewPoint.y += 20`, etc.) are
book-tuned world-unit constants, and if the world silently grows around
them, movement feels proportionally slower/smaller relative to the level -
exactly the "everything feels different, nothing looks broken" failure
mode the camera pattern's whole existence is about.

The fix, matching the camera pattern's own principle (world stays in book
units; only the screen-facing conversion scales): grow `SCREEN_TO_WORLD_X/Z`
by the same ×2/×2.4 factor the editor canvas grew by (so the offset still
centers correctly against the bigger pixel range), and *shrink*
`WORLD_SCALE_X/Z` by the same factor (so each of the now-more-numerous
editor pixels represents a proportionally smaller world distance) - net
effect: the same-shaped level produces the exact same world geometry,
just with finer editor precision to draw it:
```c
#ifdef VBE_SUPPORT
#define SCREEN_TO_WORLD_X   -224   // -112 * (640/320)
#define SCREEN_TO_WORLD_Z   -240   // -100 * (480/200)
#define WORLD_SCALE_X       1.0f   // 2 / (640/320)
#define WORLD_SCALE_Z       (-2.0f / 2.4f)   // -0.8(3), not a clean number
#else
#define SCREEN_TO_WORLD_X   -112
#define SCREEN_TO_WORLD_Z   -100
#define WORLD_SCALE_X       2
#define WORLD_SCALE_Z      -2
#endif
```
`WORLD_SCALE_Z` landing on a non-integer (`-0.8(3)`) is a direct, unavoidable
consequence of mode-13h's non-square pixels (the same ×2-vs-×2.4 split
documented in "Coordinate constants & the camera pattern" above) - not a
mistake, just worth writing as `-2.0f / 2.4f` in code rather than a bare
magic float, so the derivation stays visible. `WALL_CEILING`/`WALL_FLOOR`
(20/-20, wall height) and `WORLD_POS_X/Y/Z` (0/0/300, post-build
translation) are pure world-space constants with no screen-coordinate
involvement at all - they stay exactly as the book wrote them, in both
branches.

### Font

No `loadFontSet` currently (the book calls bare `initRomCharSet()`) - add
the same `loadFontSet("font16.bin", 16)`/`freeFontSet()` pair every other
`VBE_SUPPORT` demo that prints text already has.

### Status

Ported, matching the design above. One consistency fix surfaced during
implementation, not covered by the design pass: the book records each new
wall line's endpoints at the mouse position plus a fixed `+4` offset
(half of the book's 9×9 pointer sprite, centering the recorded point on the
crosshair rather than the sprite's top-left corner) - since `PointerSpr`'s
own centering offset became `-9` under `VBE_SUPPORT` (half of the doubled
18×18 sprite, following the `mousetst.c` precedent), the matching endpoint
offset needed to become `+9` too, or recorded lines would land 5px off from
where the crosshair was actually drawn. Pulled into a `crosshairOffset`
local (9 under `VBE_SUPPORT`, 4 under book) and applied at all six call
sites that previously hardcoded `+4` (line-endpoint recording,
`deleteLine`'s search point, and the coordinate readout).

`bspbutt.pcx` (14 button frames, cells 64×12 doubled to 128×24) and
`bsppoint.pcx` (4 pointer frames, cells 9×9 doubled to 18×18) were rescaled
with the per-cell nearest-neighbor recipe from Assets above, each into a
tight canvas sized to just the doubled grid (259×176 and 77×20
respectively) - same convention as `vbe/chap05/hammer.pcx` and
`vbe/chap12/textures.pcx`. `bspint.pcx` was whole-image resized to 640×480.

- **Phase 1 — 2D** ✓: chap03, chap04, chap05, chap07, chap08, chap09 (Starblazer —
  the first full-game port; introduced the camera pattern above and
  `waitForVerticalRetrace`).
- **Phase 2 — 3D** ✓ (chap11-15 complete): chap11–18 (`black11`/`black15`/
  `black17`/`black18`) remain otherwise unported. chap11-13 (`black11`) are
  fully ported — see "chap11: 3-D projection & the resolution-aware
  rasterizer" above for the design, and its Status section for the
  demo-by-demo breakdown. chap14/objects.c is ported too (reuses
  `black11.c` directly, no separate engine snapshot). chap15 (`black15`) is
  fully ported too — see "chap15: Z-buffered rendering & BSP trees" and
  "chap15/bspdemo.c: the BSP level editor" above — all four demos
  (`zdemo.c`/`sortdemo.c`/`solzdemo.c`/`bspdemo.c`) are done. chap17/18
  (`black17`/`black18`) are still undesigned. Apply the camera pattern to
  chap17 (Starblazer 3-D) and chap18 (Kill or Be Killed) when that work
  starts, same as chap09.

### chap08 notes

`timer.c` (PIT reprogramming menu) and `vblank.c` (VGA CRT register interrupt) are VGA/text-mode specific — no VESA port. Ported: `jelly.c` and `volcano.c`.

`jelly.c`: the 16-bit version drove jellyfish position via a timer ISR (INT 0x1C). In 32-bit protected mode under DOS/4GW, `_asm sti` is a GPF (ring 3), and the EOI from INT 0x1C is redundant (DOS/4GW's INT 0x08 handler already sent it). The VESA port inlines the position-update logic into the main loop; behavior is identical at 18Hz.
