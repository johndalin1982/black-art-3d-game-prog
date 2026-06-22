#ifndef BLACK3_H
#define BLACK3_H

// VBE_SUPPORT requires 32-bit flat mode - DPMI-based VBE mode-set, 32-bit-
// only assembly helpers, and no 16-bit BIOS-ROM-font access all mean a
// VBE_SUPPORT build is never valid without DOS_32_BIT too. Defining it here
// means a .tgt only has to set VBE_SUPPORT to get a correct 32-bit+VESA
// build even if DOS_32_BIT was left off by mistake - every engine/demo file
// that checks DOS_32_BIT includes this header directly or transitively, so
// there's nowhere else that combination needs to be re-verified.
#if defined(VBE_SUPPORT) && !defined(DOS_32_BIT)
#define DOS_32_BIT
#endif

// Memory model macros for DOS4GW 32-bit compatibility
#ifdef DOS_32_BIT
    #define FAR
    #define _FAR
    #define MALLOC(size)         malloc(size)
    #define FREE(ptr)            free(ptr)
    #define MEMCPY(dest, src, n) memcpy((dest), (src), (n))
    #define MEMSET(dest, val, n) memset((dest), (val), (n))
#else
    #define FAR                  far
    #define _FAR                 _far
    #define MALLOC(size)         _fmalloc(size)
    #define FREE(ptr)            _ffree(ptr)
    #define MEMCPY(dest, src, n) _fmemcpy((dest), (src), (n))
    #define MEMSET(dest, val, n) _fmemset((dest), (val), (n))
#endif

#ifdef VBE_SUPPORT
// ---------------------------------------------------------------------------
// Assembly helpers for the bulk fill/blit paths in black3.c/black4.c
// (bpp16/bpp32 - VESA only, hence only needed under VBE_SUPPORT, which by
// this point always also means DOS_32_BIT, since these are 32-bit-only
// assembly. Without VBE_SUPPORT the engine is mode-13h-only and uses the
// book's own original, differently-shaped _asm blocks instead - see
// fillScreen etc.)
// - defined via #pragma aux, the Watcom-preferred mechanism. _asm blocks
// give the optimizer no visibility into which registers are clobbered and
// carry no direction-flag guarantee; #pragma aux solves both: 'parm' pins
// each argument to a register, 'modify' tells the optimizer every register
// the asm touches, and 'cld' is the first instruction in every helper so
// the direction flag is always clear before REP. Each helper is a genuine
// (non-inlined) function call whose body IS the assembly, matched to the
// caller's register assignment - no stack overhead, no hidden clobber.
//
// #pragma aux binds the calling convention at the declaration site, so
// every translation unit that calls one of these needs its own matching
// declaration+pragma pair - each is `static` here so black3.c and black4.c
// (both include this header) each get their own private compiled copy,
// while the source itself is written once.
// ---------------------------------------------------------------------------

// Fill dwords DWORDs at [edi] with eax, then rem trailing bytes.
static void asmStosDwordsBytes(void* dst, unsigned long fill, unsigned dwords, unsigned rem);
#pragma aux asmStosDwordsBytes = \
    "cld"           \
    "rep stosd"     \
    "mov ecx, ebx"  \
    "rep stosb"     \
    parm   [edi] [eax] [ecx] [ebx] \
    modify [edi ecx ebx];

// Fill dwords DWORDs at [edi] with eax, then rem trailing words (0 or 1).
static void asmStosDwordsWords(void* dst, unsigned long fill, unsigned dwords, unsigned rem);
#pragma aux asmStosDwordsWords = \
    "cld"           \
    "rep stosd"     \
    "mov ecx, ebx"  \
    "rep stosw"     \
    parm   [edi] [eax] [ecx] [ebx] \
    modify [edi ecx ebx];

// Fill cnt DWORDs at [edi] with eax (no tail).
static void asmStosDwords(void* dst, unsigned long fill, unsigned cnt);
#pragma aux asmStosDwords = \
    "cld"           \
    "rep stosd"     \
    parm   [edi] [eax] [ecx] \
    modify [edi ecx];

// Copy dwords DWORDs then rem trailing bytes: [esi] -> [edi].
static void asmMovDwordsBytes(void* dst, const void* src, unsigned dwords, unsigned rem);
#pragma aux asmMovDwordsBytes = \
    "cld"           \
    "rep movsd"     \
    "mov ecx, ebx"  \
    "rep movsb"     \
    parm   [edi] [esi] [ecx] [ebx] \
    modify [edi esi ecx ebx];
#endif

#define GRAPHICS_MODE13 0x13    // 320x200x256
#define TEXT_MODE       0x03    // 80x25 text mode

#define MODE13_WIDTH   (unsigned int)320 // mode 13h screen dimensions
#define MODE13_HEIGHT  (unsigned int)200

#define ROM_CHAR_WIDTH      8     // width of ROM 8x8 character set in pixels
#define ROM_CHAR_HEIGHT     8     // height of ROM 8x8 character set in pixels

// the vga card controller ports
#define CRT_CONTROLLER          0x3D4   // the crt controller's index port

#define SET_BITS(x, bits)   ((x) | (bits))  // used to set bits in word
#define RESET_BITS(x, bits) ((x) & ~(bits)) // used to reset bits in a word

#ifdef VBE_SUPPORT
// Pack R, G, B (each 0..255) into a 24/32bpp colour value - R=bits 16-23,
// G=bits 8-15, B=bits 0-7. Only meaningful when DisplayBpp is 32, which is
// only possible under VBE_SUPPORT (mode-13h is always 8bpp).
#define RGB32(r,g,b) \
    (((unsigned long)(r)<<16)|((unsigned long)(g)<<8)|(unsigned long)(b))
#endif

extern unsigned char FAR* VideoBuffer;

#ifdef VBE_SUPPORT
// single source of truth for the CURRENT mode's geometry - only exists under
// VBE_SUPPORT. Without it, every drawing function is compiled against the
// book's original fixed 320x200x8 addressing (see fillScreen/writePixel/
// etc.), so there's nothing for these to vary at runtime - the original
// 16-bit build and a DOS_32_BIT build with no VBE_SUPPORT both stay
// mode-13h-only, matching the book exactly (just with flat vs. segmented
// pointers), with none of this runtime-generic machinery compiled in at all.
extern int DisplayPitch;    // bytes per row of the CURRENT mode
extern int DisplayWidth;    // pixel width of the CURRENT mode
extern int DisplayHeight;   // pixel height of the CURRENT mode
extern int DisplayBpp;      // bits per pixel of the CURRENT mode (8, 16, or 32)
// log2(bytes/pixel): 0/1/2 for 8/16/32bpp. DisplayBpp isn't a compile-time
// constant, so the compiler can't turn an "x * (DisplayBpp/8)" byte offset
// into a shift on its own the way it does the book's own fixed-320 address
// math - this lets every such site use "x << DisplayBppShift" instead of a
// runtime multiply, kept in sync with DisplayBpp everywhere it's set.
extern int DisplayBppShift;

// DisplayPitch itself isn't always a single power of two, so a plain
// DisplayPitchShift can't always replace "y * DisplayPitch" the way
// DisplayBppShift replaces "x * (DisplayBpp/8)". Two cases:
//  - setGraphicsModeVesa forces the hardware's logical scan line length to
//    the next power of two via VBE 4F06h/BL=02h (see there), so a VESA
//    mode's DisplayPitch is always a single power of two:
//    DisplayPitchShift1 = log2(DisplayPitch), DisplayPitchShift2 = -1.
//  - GRAPHICS_MODE13's DisplayPitch is a fixed physical VGA constant (320)
//    that VBE has no equivalent renegotiation for, and 320 isn't a power of
//    two - but it IS the sum of exactly two (256 + 64), the same
//    decomposition the book's own non-VBE_SUPPORT code already uses
//    directly ((y<<8)+(y<<6)): DisplayPitchShift1 = 8, DisplayPitchShift2 = 6.
// A VBE_SUPPORT build can switch into true mode-13h at runtime
// (setGraphicsMode(GRAPHICS_MODE13) is always available, not just under
// non-VBE_SUPPORT builds), so every VBE_SUPPORT drawing function has to
// handle both cases - hence PITCH_OFFSET(y) below instead of a bare shift,
// with DisplayPitchShift2 < 0 meaning "no second term needed".
extern int DisplayPitchShift1;
extern int DisplayPitchShift2;
#define PITCH_OFFSET(y) \
    (((unsigned long)(y) << DisplayPitchShift1) + \
     (DisplayPitchShift2 < 0 ? 0UL : ((unsigned long)(y) << DisplayPitchShift2)))
// named Display* (not Screen*) because black11.h already uses ScreenWidth/
// ScreenHeight for an unrelated concept - the 3D system's projection-plane
// size (a float, hardcoded 320x200, part of the perspective math) - the 3D
// chapters are never touched by the VESA/mode-13h merge work, so this
// header can't reuse that name without a type-mismatch conflict
#endif

#ifdef VBE_SUPPORT
// size of the CURRENTLY LOADED font's glyph cell - only exists alongside
// loadFontSet (see below): variable glyph sizes only matter for VESA's
// higher resolutions, so without VBE_SUPPORT a DOS_32_BIT build uses
// initRomCharSet's fixed 8x8 ROM-equivalent font instead, same as the book.
extern int GlyphWidth;
extern int GlyphHeight;
#endif

// this structure holds a RGB triple in three unsigned bytes
typedef struct RgbColorType {
    unsigned char red;      // red component of color 0-63
    unsigned char green;    // green component of color 0-63
    unsigned char blue;     // blue component of color 0-63
} RgbColor, *RgbColorPtr;

typedef struct RgbPaletteType {
    int startReg;           // index of the starting register that is saved
    int endReg;             // index of the ending register that is saved
    RgbColor colors[256];   // the storage area for the palette
} RgbPalette, *RgbPalettePtr;

void timeDelay(int clicks);
void setGraphicsMode(int mode);
#ifdef VBE_SUPPORT
int setGraphicsModeVesa(int width, int height, int bpp);
#endif
void fillScreen(int color);
void writePixel(int x, int y, int color);
int readPixel(int x, int y);
void lineH(int x1, int x2, int y, int color);
void lineV(int y1, int y2, int x, int color);
void drawRectangle(int x1, int y1, int x2, int y2, int color);
void writeColorReg(int index, RgbColorPtr color);
RgbColorPtr readColorReg(int index, RgbColorPtr color);
void readPalette(int startReg, int endReg, RgbPalettePtr palette);
void writePalette(RgbPalettePtr palette);
void printChar(int xc, int yc, char c, int color, int transparent);
void printString(int x, int y, int color, char* string, int transparent);
void setModeZ(void);
void fillScreenZ(int color);
void writePixelZ(int x, int y, int color);

#ifdef VBE_SUPPORT
// Load a square, bit-packed font file at a custom cell size (e.g. the
// exp_font-generated font.bin/font16.bin/font24.bin/font32.bin, ceil(size/8)
// bytes per row, 256 glyphs) - sets GlyphWidth/GlyphHeight to size on
// success. loadFontSet("font.bin", ROM_CHAR_WIDTH) is the plain 8x8
// ROM-equivalent set - there is no book-original init function to match,
// since the variable-size font-loading bridge itself isn't part of the
// 16-bit book source (see initRomCharSet for that, the fixed-8x8 original).
int loadFontSet(const char* filename, int size);
void freeFontSet(void);
#elif defined(DOS_32_BIT)
int initRomCharSet(void);
void freeRomCharSet(void);
#endif

// Exit directly to DOS, bypassing C runtime cleanup
// Use this for clean game exit in both 16-bit and 32-bit modes
void exitToDos(int returnCode);

#endif
