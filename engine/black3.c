#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <bios.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

#include "black3.h"   // the header file for this module

#ifdef VBE_SUPPORT
#include "dpmi.h"
#endif

#define COLOR_MASK          0x3C6   // the bit mask register
#define COLOR_REGISTER_RD   0x3C7   // set read index at this I/O
#define COLOR_REGISTER_WR   0x3C8   // set write index at this I/O
#define COLOR_DATA          0x3C9   // the R/W data is here

#define ROM_CHAR_SET_SEG 0xF000 // segment of 8x8 ROM character set
#define ROM_CHAR_SET_OFF 0xFA6E // beginning offset of 8x8 ROM character set

// the vga card controller ports
#define SEQUENCER               0x3C4   // the sequencer's index port
#define GFX_CONTROLLER          0x3CE   // the graphics controller's index port
#define ATTR_CONTROLLER_FF      0x3DA   // the attribute controller's Flip Flop
#define ATTR_CONTROLLER_DATA    0x3C0   // the attribute controller's data port

// defines for the CRT controller registers of interest
#define CRT_MAX_SCANLINE    0x09    // the maximum scanline register
                                    // used to select how many scanlines
                                    // per row

#define CRT_ADDR_MODE       0x14    // the address mode register
                                    // used to select byte addressing
                                    // for VGA
                                    // also known as the underline register

#define CRT_MODE_CONTROL    0x17    // the mode control register
                                    // used to select single byte addressing

// defines for the GFX controller registers of interest
#define GFX_WRITE_MODE      0x05    // the memory write mode register
                                    // used to deselect even/odd plane
                                    // addressing

#define GFX_MISC            0x06    // the miscellaneous register
                                    // used to deselect the chaining
                                    // of memory

// defines for the SEQUENCER registers of interest
#define SEQ_PLANE_ENABLE    0x02    // plane enable register, used to select
                                    // which planes are written to by a
                                    // CPU write

#define SEQ_MEMORY_MODE     0x04    // the memory mode register
                                    // used to deselect memory chain mode
                                    // and odd/even memory addressing

#ifdef DOS_32_BIT
unsigned char* RomCharSet = NULL;       // will be loaded from font.bin
unsigned char FAR* VideoBuffer = (unsigned char FAR*)0xA0000;
#else
unsigned char FAR* RomCharSet = (unsigned char FAR*)0xF000FA6EL;
unsigned char FAR* VideoBuffer = (unsigned char FAR*)0xA0000000L;
#endif

#ifdef VBE_SUPPORT
// single source of truth for the CURRENT mode's geometry, kept up to date by
// setGraphicsMode/setGraphicsModeVesa; every drawing function reads these
// instead of assuming mode-13h's fixed 320x200x8
int DisplayPitch  = MODE13_WIDTH;
int DisplayWidth  = MODE13_WIDTH;
int DisplayHeight = MODE13_HEIGHT;
int DisplayBpp    = 8;
int DisplayBppShift = 0;
int DisplayPitchShift1 = 8;   // 320 = (1<<8) + (1<<6), matching MODE13_WIDTH default above
int DisplayPitchShift2 = 6;
#endif

#ifdef VBE_SUPPORT
// size of the CURRENTLY LOADED font's glyph cell, kept up to date by
// loadFontSet; printChar/printString (and their *Db counterparts in
// black4.c) read these instead of assuming 8x8
int GlyphWidth  = ROM_CHAR_WIDTH;
int GlyphHeight = ROM_CHAR_HEIGHT;

// defined in black4.c - a mode change has to keep this in sync too (see
// setGraphicsMode/setGraphicsModeVesa below). Callers that never call
// createDoubleBuffer() (pcxCopyToBuffer/displayDoubleBuffer straight into
// VideoBuffer, and every sprite function's Y-clip check) all read this as
// "how tall is the thing I'm currently drawing into" - without this sync it
// stays frozen at its SCREEN_HEIGHT (200) static initializer even after a
// setGraphicsModeVesa to a taller mode, silently truncating anything that
// relies on the default.
extern unsigned int DoubleBufferHeight;
#endif

void timeDelay(int clicks) {
    // this function uses the internal timer to delay a number of clock ticks
#ifdef DOS_32_BIT
    volatile long FAR* clock = (volatile long FAR*)0x0000046C;
#else
    volatile long FAR* clock = (volatile long FAR*)0x0000046CL;
#endif

    long now;

    // get current time
    now = *clock;

    // wait for number of click to pass
    while (labs(*clock - now) < clicks) {}
}

void setGraphicsMode(int mode) {
    // use the video interrupt 10h and the C interrupt function to set
    // the video mode
    union REGS inregs, outregs;

    inregs.h.ah = 0;                    // set video mode subfunction
    inregs.h.al = (unsigned char)mode;  // video mode to change to

#ifdef DOS_32_BIT
    int386(0x10, &inregs, &outregs);
#else
    int86(0x10, &inregs, &outregs);
#endif

#ifdef VBE_SUPPORT
    if (mode == GRAPHICS_MODE13) {
        DisplayPitch  = MODE13_WIDTH;
        DisplayWidth  = MODE13_WIDTH;
        DisplayHeight = MODE13_HEIGHT;
        DisplayBpp    = 8;
        DisplayBppShift = 0;
        // fixed physical VGA pitch, no VBE renegotiation available - use its
        // own two-shift decomposition (320 = 256+64) instead
        DisplayPitchShift1 = 8;
        DisplayPitchShift2 = 6;
        DoubleBufferHeight = MODE13_HEIGHT;
    }
#endif
}

#ifdef VBE_SUPPORT

// ---- VBE VbeInfoBlock / ModeInfoBlock field offsets (VESA VBE spec) --------
#define VIB_VIDEO_MODE_PTR    14    // dword far ptr to the mode-number list
#define MIB_ATTRIBUTES         0    // word, mode attributes
#define MIB_BYTES_PER_LINE    16    // word, bytes per scan line
#define MIB_XRES               18    // word, horizontal resolution
#define MIB_YRES              20    // word, vertical resolution
#define MIB_BPP               25    // byte, bits per pixel
#define MIB_PHYS_BASE_PTR     40    // dword, linear framebuffer phys addr (VBE 2.0+)

// Find a VBE mode for width x height x targetBpp with a linear framebuffer,
// filling *pitch and *physBase; returns the mode number or -1. Scans the
// card's own mode list (4F00h -> VideoModePtr), so any resolution the
// hardware exposes works.
static int vesaFindMode(int width, int height, int targetBpp, int* pitch, unsigned long* physBase) {
    DpmiRealModeRegs regs;
    unsigned short   mibSeg, mibSel, vbeSeg, vbeSel, modeSeg, modeOff;
    unsigned char*   mib;
    unsigned char*   vbe;
    unsigned char*   modeList;
    int              i, result = -1;

    // A 256-byte ModeInfoBlock in DOS memory the real-mode BIOS can fill in.
    if (!dpmiAllocDos(16, &mibSeg, &mibSel)) {
        return -1;
    }
    mib = (unsigned char*)((unsigned long)mibSeg << 4);

    // Enumerate the card's mode list: 4F00h -> VbeInfoBlock.VideoModePtr.
    if (!dpmiAllocDos(32, &vbeSeg, &vbeSel)) {          // 512-byte VbeInfoBlock
        dpmiFreeDos(mibSel);
        return -1;
    }
    vbe = (unsigned char*)((unsigned long)vbeSeg << 4);
    vbe[0] = 'V'; vbe[1] = 'B'; vbe[2] = 'E'; vbe[3] = '2';   // ask for VBE 2.0 info

    memset(&regs, 0, sizeof(regs));
    regs.eax = 0x4F00;                                 // get controller info
    regs.es  = vbeSeg;
    regs.edi = 0;
    if (!dpmiRealModeInt(0x10, &regs) && (regs.eax & 0xFFFF) == 0x004F) {
        // VideoModePtr is a real-mode seg:off far pointer to a 0xFFFF-terminated
        // list of mode numbers; its flat address is reachable in the DOS/4GW map.
        modeOff  = vbe[VIB_VIDEO_MODE_PTR]     | (vbe[VIB_VIDEO_MODE_PTR + 1] << 8);
        modeSeg  = vbe[VIB_VIDEO_MODE_PTR + 2] | (vbe[VIB_VIDEO_MODE_PTR + 3] << 8);
        modeList = (unsigned char*)(((unsigned long)modeSeg << 4) + modeOff);

        for (i = 0; ; i += 2) {
            int candidate = modeList[i] | (modeList[i + 1] << 8);
            int attr, xres, yres, bpp;

            if (candidate == 0xFFFF) {
                break;                                 // end of the list
            }
            memset(&regs, 0, sizeof(regs));
            regs.eax = 0x4F01;
            regs.ecx = (unsigned)candidate;
            regs.es  = mibSeg;
            regs.edi = 0;
            if (dpmiRealModeInt(0x10, &regs) || (regs.eax & 0xFFFF) != 0x004F) {
                continue;
            }
            attr = mib[MIB_ATTRIBUTES] | (mib[MIB_ATTRIBUTES + 1] << 8);
            xres = mib[MIB_XRES]       | (mib[MIB_XRES + 1] << 8);
            yres = mib[MIB_YRES]       | (mib[MIB_YRES + 1] << 8);
            bpp  = mib[MIB_BPP];

            // supported (bit 0) + graphics (bit 4) + linear framebuffer (bit 7)
            if ((attr & 0x91) == 0x91 && bpp == targetBpp && xres == width && yres == height) {
                *pitch    = mib[MIB_BYTES_PER_LINE] | (mib[MIB_BYTES_PER_LINE + 1] << 8);
                *physBase =  (unsigned long)mib[MIB_PHYS_BASE_PTR]
                          | ((unsigned long)mib[MIB_PHYS_BASE_PTR + 1] << 8)
                          | ((unsigned long)mib[MIB_PHYS_BASE_PTR + 2] << 16)
                          | ((unsigned long)mib[MIB_PHYS_BASE_PTR + 3] << 24);
                result = candidate;
                break;
            }
        }
    }

    dpmiFreeDos(vbeSel);
    dpmiFreeDos(mibSel);
    return result;
}

int setGraphicsModeVesa(int width, int height, int bpp) {
    DpmiRealModeRegs regs;
    unsigned long    physBase, linear;
    int              mode, pitch;

    mode = vesaFindMode(width, height, bpp, &pitch, &physBase);
    if (mode < 0 || physBase == 0) {
        return 0;
    }

    // INT 10h 4F02h - set the mode with the linear-framebuffer bit (0x4000).
    memset(&regs, 0, sizeof(regs));
    regs.eax = 0x4F02;
    regs.ebx = (unsigned)mode | 0x4000;
    if (dpmiRealModeInt(0x10, &regs) || (regs.eax & 0xFFFF) != 0x004F) {
        return 0;
    }

    // Widen the hardware's logical scan line length to the next power of
    // two (INT 10h 4F06h/BL=02h, VBE 2.0's "Set Logical Scan Line Length in
    // Bytes") so DisplayPitch itself becomes a single power of two - lets
    // every "y * DisplayPitch" in the engine become "y << shift" instead of
    // a runtime multiply (see PITCH_OFFSET in black3.h), the same way
    // DisplayBppShift covers the x-offset half of the same address
    // computation. This reconfigures the ACTUAL hardware stride (not just
    // our own bookkeeping), so addressing stays correct - it costs extra
    // VRAM for widths that weren't already a power of two (e.g. 800x600x32:
    // 3200 bytes/row padded to 4096, +28%). Requests the smallest power of
    // two that still fits the visible width; fails the mode outright if the
    // card/VRAM can't support even that (same as any other unsupported
    // width/height/bpp combination above).
    {
        unsigned long minPitch = (unsigned long)width * (bpp / 8);
        unsigned long tryPitch = 1;

        while (tryPitch < minPitch) {
            tryPitch <<= 1;
        }

        memset(&regs, 0, sizeof(regs));
        regs.eax = 0x4F06;
        regs.ebx = 0x0002;
        regs.ecx = tryPitch;
        if (dpmiRealModeInt(0x10, &regs) || (regs.eax & 0xFFFF) != 0x004F
            || (regs.ebx & 0xFFFF) != tryPitch) {
            return 0;
        }
        pitch = (int)tryPitch;
    }

    // Try to map two pages so page-flip demos can use a second page; a
    // high-res 32bpp mode may exceed the card's VRAM when doubled, so fall
    // back to one page so the mode still works.
    if (!dpmiMapPhysical(physBase, (unsigned long)pitch * height * 2, &linear)) {
        if (!dpmiMapPhysical(physBase, (unsigned long)pitch * height, &linear)) {
            return 0;
        }
    }
    VideoBuffer  = (unsigned char*)linear;
    DisplayWidth  = width;
    DisplayHeight = height;
    DisplayPitch  = pitch;
    DisplayBpp    = bpp;
    DisplayBppShift = (bpp == 8) ? 0 : (bpp == 16) ? 1 : 2;   // 32bpp
    DoubleBufferHeight = height;

    // pitch is guaranteed a single power of two by the 4F06h negotiation
    // above, so no second shift term is needed (contrast GRAPHICS_MODE13's
    // fixed 320, which needs both - see black3.h's PITCH_OFFSET).
    {
        int shift = 0;
        unsigned long v = (unsigned long)pitch;
        while (v > 1) {
            v >>= 1;
            shift++;
        }
        DisplayPitchShift1 = shift;
        DisplayPitchShift2 = -1;
    }
    return 1;
}

#endif

void fillScreen(int color) {
    // this function will fill the entire screen with the sent color
    // use the inline assembler for speed
#ifdef VBE_SUPPORT
    {
        unsigned long bytes = PITCH_OFFSET(DisplayHeight);
        unsigned dwords, rem;

        switch (DisplayBpp) {
            case 8: {
                unsigned long fill = (unsigned char)color;
                fill |= fill << 8;
                fill |= fill << 16;
                dwords = (unsigned)(bytes >> 2);
                rem    = (unsigned)(bytes & 3);
                asmStosDwordsBytes(VideoBuffer, fill, dwords, rem);
                break;
            }
            case 16: {
                unsigned long c    = (unsigned short)color;
                unsigned long fill = c | (c << 16);
                dwords = (unsigned)(bytes >> 2);
                rem    = (unsigned)(bytes & 3);
                asmStosDwordsWords(VideoBuffer, fill, dwords, rem);
                break;
            }
            default: {   // 32
                dwords = (unsigned)(bytes >> 2);
                asmStosDwords(VideoBuffer, (unsigned long)color, dwords);
                break;
            }
        }
    }
#else
    // mode-13h only, no VBE_SUPPORT: fixed 320x200 fill
#ifdef DOS_32_BIT
    _asm {
        mov edi, VideoBuffer        ; load flat pointer into EDI
        mov al, BYTE PTR color      ; move the color into al
        mov ah, al                  ; replicate color: ax = [color][color]
        mov bx, ax                  ; copy to bx
        shl eax, 16                 ; shift ax to upper word of eax
        mov ax, bx                  ; eax now has 4 copies of color
        mov ecx, 320*200/4          ; number of dwords to fill
        rep stosd                   ; fill with dwords
    }
#else
    _asm {
        les di,VideoBuffer      ; point es:di to video buffer
        mov al,BYTE PTR color   ; move the color into al
        mov ah,al               ; replicate color into ah
        mov cx,320*200/2        ; number of words to fill (using word is faster then bytes)
        rep stosw               ; move the color into the video buffer really fast!
    }
#endif
#endif
}

void writePixel(int x, int y, int color) {
#ifdef VBE_SUPPORT
    // plots the pixel at (x,y) in the current mode, whatever it is
    unsigned char FAR* p = VideoBuffer + PITCH_OFFSET(y) + ((unsigned long)x << DisplayBppShift);

    switch (DisplayBpp) {
        case 8:  *p = (unsigned char)color; break;
        case 16: *(unsigned short FAR*)p = (unsigned short)color; break;
        default: *(unsigned long FAR*)p  = (unsigned long)color; break;   // 32
    }
#else
    // plots the pixel in the desired color a little quicker using binary shifting
    // to accomplish the multiplications
    // use the fact that 320*y = 256*y + 64*y = y<<8 + y<<6
    VideoBuffer[((y << 8) + (y << 6)) + x] = (unsigned char)color;
#endif
}

int readPixel(int x, int y) {
#ifdef VBE_SUPPORT
    // this function reads a pixel from the screen buffer in the current mode
    unsigned char FAR* p = VideoBuffer + PITCH_OFFSET(y) + ((unsigned long)x << DisplayBppShift);

    switch (DisplayBpp) {
        case 8:  return (int)(*p);
        case 16: return (int)(*(unsigned short FAR*)p);
        default: return (int)(*(unsigned long FAR*)p);   // 32
    }
#else
    // this function reads a pixel from the screen buffer
    // use the fact that 320*y = 256*y + 64*y = y<<8 + y <<6
    return (int)VideoBuffer[((y << 8) + (y << 6)) + x];
#endif
}

void lineH(int x1, int x2, int y, int color) {
    // draw a horizontal line; this function does not do clipping hence
    // x1,x2 and y must all be within the bounds of the current mode
#ifdef VBE_SUPPORT
    unsigned char FAR* p;
    int temp, len;   // temp: endpoint-swap storage; len: pixel count

    // sort x1 and x2, so that x2 > x1
    if (x1 > x2) {
        temp = x1;
        x1 = x2;
        x2 = temp;
    }
    len = x2 - x1 + 1;
    p = VideoBuffer + PITCH_OFFSET(y) + ((unsigned long)x1 << DisplayBppShift);

    switch (DisplayBpp) {
        case 8:
            // one byte per pixel: the book's own MEMSET approach, generalized
            // to the current pitch instead of a hardcoded 320
            MEMSET((char FAR*)p, (unsigned char)color, len);
            break;
        case 16: {
            // align to a DWORD boundary, then fill 2 pixels at once with rep stosd
            unsigned short c    = (unsigned short)color;
            unsigned long  fill = c | ((unsigned long)c << 16);
            unsigned       dwords, rem;
            if (((unsigned long)p & 2) && len > 0) {
                *(unsigned short*)p = c;      // plot one leading pixel to align
                p += 2;
                len--;
            }
            dwords = (unsigned)len >> 1;      // 2 pixels per DWORD
            rem    = (unsigned)len & 1;
            asmStosDwordsWords(p, fill, dwords, rem);
            break;
        }
        default:   // 32 - one pixel per stosd
            asmStosDwords(p, (unsigned long)color, (unsigned)len);
            break;
    }
#else
    // mode-13h only, no VBE_SUPPORT: the book's original fixed addressing
    int temp;   // used for temporary storage during endpoint swap

    // sort x1 and x2, so that x2 > x1
    if (x1 > x2) {
        temp = x1;
        x1 = x2;
        x2 = temp;
    }

    // draw the row of pixels
    MEMSET((char FAR*)(VideoBuffer + ((y << 8) + (y << 6)) + x1), (unsigned char) color, x2 - x1 + 1);
#endif
}

void lineV(int y1, int y2, int x, int color) {
#ifdef VBE_SUPPORT
    // draw a vertical line; pixel addresses are not contiguous in memory,
    // so this writes one pixel per row instead of using MEMSET
    // note that the end points of the line must be on the screen
    unsigned char FAR* p;  // current row's pixel
    int temp,   // used for temporary storage during swap
        y,      // loop index
        pitch;  // bytes per row of the current mode

    // make sure y2 > y1
    if (y1 > y2) {
        temp = y1;
        y1 = y2;
        y2 = temp;
    }

    pitch = DisplayPitch;
    p = VideoBuffer + PITCH_OFFSET(y1) + ((unsigned long)x << DisplayBppShift);

    switch (DisplayBpp) {
        case 8:
            for (y = y1; y <= y2; y++) { *p = (unsigned char)color; p += pitch; }
            break;
        case 16:
            for (y = y1; y <= y2; y++) { *(unsigned short FAR*)p = (unsigned short)color; p += pitch; }
            break;
        default:   // 32
            for (y = y1; y <= y2; y++) { *(unsigned long FAR*)p = (unsigned long)color; p += pitch; }
            break;
    }
#else
    // mode-13h only, no VBE_SUPPORT: draw a vertical line, note that a
    // memset function can no longer be used since the pixel addresses are
    // no longer contiguous in memory; note that the end points of the line
    // must be on the screen
    unsigned char FAR* startOffset; // starting memory offset of line
    int index,  // loop index
        temp,   // used for temporary storage during swap
        length; // length of line

    // make sure y2 > y1
    if (y1 > y2) {
        temp = y1;
        y1 = y2;
        y2 = temp;
    }

    // compute starting position
    startOffset = VideoBuffer + ((y1 << 8) + (y1 << 6)) + x;

    // pre-compute length of line
    length = y2 - y1;

    for (index = 0; index <= length; index++) {
        // set the pixel
        *startOffset = (unsigned char)color;

        // move downward to next line
        startOffset += 320;
    }
#endif
}

void drawRectangle(int x1, int y1, int x2, int y2, int color) {
    // this function will draw a rectangle from (x1,y1) - (x2,y2)
    // it is assumed that each endpoint is within the screen boundaries
    // and (x1,y1) is the upper left hand corner and (x2,y2) is the lower
    // right hand corner
#ifdef VBE_SUPPORT
    int y;

    // draw the rectangle one horizontal line per row - lineH already
    // handles every DisplayBpp case (including the VESA-only asm fast paths)
    for (y = y1; y <= y2; y++) {
        lineH(x1, x2, y, color);
    }
#else
    // mode-13h only, no VBE_SUPPORT: the book's original fixed addressing
    unsigned char FAR* startOffset; // starting memory offset of first row
    int width;  // width of rectangle

    // compute starting offset of first row
    startOffset = VideoBuffer + ((y1 << 8) + (y1 << 6)) + x1;

    // compute width of rectangle
    width = 1 + x2 - x1;    // the "1" to reflect the true width in pixels

    // draw the rectangle row by row
    while (y1++ <= y2) {
        // draw the line
        MEMSET((char FAR*)startOffset, (unsigned char)color, width);

        // move the memory pointer to the next line
        startOffset += 320;
    }
#endif
}

void writeColorReg(int index, RgbColorPtr color) {
    // this function is used to write a color register with the RGB value
    // within "color"

    // tell vga card which color register to update
    outp(COLOR_REGISTER_WR, index);

    // update the color register RGB triple, note the same port is used each time
    // the hardware will make sure each of the components is stored in the
    // proper location
    outp(COLOR_DATA, color->red);
    outp(COLOR_DATA, color->green);
    outp(COLOR_DATA, color->blue);
}

RgbColorPtr readColorReg(int index, RgbColorPtr color) {
    // this function reads the RGB triple out of a palette register and places it
    // into "color"

    // tell vga card which register to read
    outp(COLOR_REGISTER_RD, index);

    // now extract the data
    color->red   = (unsigned char)inp(COLOR_DATA);
    color->green = (unsigned char)inp(COLOR_DATA);
    color->blue  = (unsigned char)inp(COLOR_DATA);

    // return a pointer to color so that the function can be used as an RVALUE
    return color;
}

void readPalette(int startReg, int endReg, RgbPalettePtr palette) {
    // this function reads the palette registers in the range startReg to
    // endReg and saves them into the appropriate positions in palette
    int index;
    RgbColor color;

    // read all the registers
    for (index = startReg; index <= endReg; index++) {
        // read the color register
        readColorReg(index, &color);

        // save it in database

        palette->colors[index].red = color.red;
        palette->colors[index].green = color.green;
        palette->colors[index].blue = color.blue;
    }

    // save the interval of registers that were saved
    palette->startReg = startReg;
    palette->endReg = endReg;
}

void writePalette(RgbPalettePtr palette) {
    // this function will write to the color registers using the data in the
    // sent palette structure
    int index;

    // write all the registers
    for (index = palette->startReg; index <= palette->endReg; index++) {
        // write the color registers using the data from the sent palette
        writeColorReg(index, &palette->colors[index]);
    }
}

void printChar(int xc, int yc, char c, int color, int transparent) {
#ifdef VBE_SUPPORT
    // this function is used to print a character on the screen, using
    // whichever font is currently loaded (GlyphWidth x GlyphHeight, bit-
    // packed MSB-left, ceil(GlyphWidth/8) bytes per row - the 8x8 ROM font
    // is just the default case of this, GlyphWidth==GlyphHeight==8)
    int x, y, bpr;
    unsigned char FAR* workChar;    // pointer to character being printed
    unsigned char FAR* rowPtr;      // current row's pixel in video memory
    unsigned char bitMask;          // bit mask used to extract proper bit

    if (RomCharSet == NULL) {      // no font loaded (loadFontSet failed/never called yet)
        return;
    }

    // bytes per glyph row, and offset of this character in the font table
    bpr = (GlyphWidth + 7) / 8;
    workChar = RomCharSet + (unsigned long)(unsigned char)c * GlyphHeight * bpr;

    rowPtr  = VideoBuffer + PITCH_OFFSET(yc) + ((unsigned long)xc << DisplayBppShift);

    // draw the character row by row - clip against DisplayWidth/DisplayHeight
    // so a string can be positioned anywhere without the caller having to
    // pre-compute a safe range (printChar/printString never did clipping,
    // so a caller that lets a string run past the edge - e.g. a random
    // on-screen position with a wide font - would otherwise corrupt
    // whatever memory follows the video buffer)
    for (y = 0; y < GlyphHeight; y++) {
        if (yc + y >= 0 && yc + y < DisplayHeight) {
            // draw each pixel of this row
            for (x = 0; x < GlyphWidth; x++) {
                if (xc + x >= 0 && xc + x < DisplayWidth) {
                    bitMask = (unsigned char)(0x80 >> (x % 8));

                    // test for transparent pixel i.e. 0, if not transparent then draw
                    if (workChar[x / 8] & bitMask) {
                        unsigned char FAR* p = rowPtr + ((unsigned long)x << DisplayBppShift);
                        switch (DisplayBpp) {
                            case 8:  *p = (unsigned char)color; break;
                            case 16: *(unsigned short FAR*)p = (unsigned short)color; break;
                            default: *(unsigned long FAR*)p  = (unsigned long)color; break;   // 32
                        }
                    } else if (!transparent) {
                        // takes care of transparency - make black part opaque
                        unsigned char FAR* p = rowPtr + ((unsigned long)x << DisplayBppShift);
                        switch (DisplayBpp) {
                            case 8:  *p = 0; break;
                            case 16: *(unsigned short FAR*)p = 0; break;
                            default: *(unsigned long FAR*)p  = 0; break;   // 32
                        }
                    }
                }
            }
        }

        // move to next line in video buffer and in font table
        rowPtr += DisplayPitch;
        workChar += bpr;
    }
#else
    // mode-13h only, no VBE_SUPPORT: the book's original fixed 8x8 ROM font.
    // It uses the internal 8x8 character set to do this. Note that each
    // character is 8 bytes where each byte represents the 8 pixels that
    // make up the row of pixels
    int offset,     // offset into video memory
        x,
        y;
    unsigned char FAR* workChar;    // pointer to character being printed
    unsigned char bitMask;          // bit mask used to extract proper bit

    // compute starting offset in rom character lookup table
    // multiply the character by 8 and add the result to the starting address
    // of the ROM character sety
    workChar = RomCharSet + c * ROM_CHAR_HEIGHT;

    // compute offset of character in video buffer, use shifting to multiply
    offset = (yc << 8) + (yc << 6) + xc;

    // draw the character row by row
    for (y = 0; y < ROM_CHAR_HEIGHT; y++) {
        // reset bit mask
        bitMask = 0x80; // 10000000b

        // draw each pixel of this row
        for (x = 0; x < ROM_CHAR_WIDTH; x++) {
            // test for transparent pixel i.e. 0, if not transparent then draw
            if (*workChar & bitMask) {
                VideoBuffer[offset + x] = (unsigned char)color;
            } else if (!transparent) {
                // takes care of transparency
                VideoBuffer[offset + x] = 0;    // make black part opaque
            }

            // shift bit mask
            bitMask >>= 1;
        }

        // move to next line in video buffer and in rom character data area
        offset += MODE13_WIDTH;
        workChar++;
    }
#endif
}

void printString(int x, int y, int color, char* string, int transparent) {
    // this function prints an entire string on the screen with fixed spacing
    // (the current font's glyph width) between each character by calling
    // the printChar() function
    int currentX = x;
    char* currentChar = string;

    // print the string a character at a time
    while (*currentChar != '\0') {
        printChar(currentX, y, *currentChar, color, transparent);
#ifdef VBE_SUPPORT
        currentX += GlyphWidth;
#else
        currentX += ROM_CHAR_WIDTH;
#endif
        currentChar++;
    }
}

void setModeZ(void) {
    // this function will set the video mode to 320x400x256
    int data;

    // set system to mode 13h and use it as a foundation to base 320x400 mode on
    _asm {
        mov ax,0013h    ; ah=function number 00 (set graphics mode), al=13h
        int 10h         ; video interrupt 10h
    }

    // make changes to the crt controller first

    // set number of scanlines to 1
    outp(CRT_CONTROLLER, CRT_MAX_SCANLINE);
    data = inp(CRT_CONTROLLER + 1);
    outp(CRT_CONTROLLER + 1, RESET_BITS(data, 0x0f));

    // use byte addressing instead of word
    outp(CRT_CONTROLLER, CRT_ADDR_MODE);
    data = inp(CRT_CONTROLLER + 1);
    outp(CRT_CONTROLLER + 1, RESET_BITS(data, 0x40));
    // second register that needs to reflect byte addressing
    outp(CRT_CONTROLLER, CRT_MODE_CONTROL);
    data = inp(CRT_CONTROLLER + 1);
    outp(CRT_CONTROLLER + 1, SET_BITS(data, 0x40));

    // make changes to graphics controller

    // set addressing to not use odd/even memory writes
    outp(GFX_CONTROLLER, GFX_WRITE_MODE);
    data = inp(GFX_CONTROLLER + 1);
    outp(GFX_CONTROLLER + 1, RESET_BITS(data, 0x10));

    // don't chain the memory maps together
    outp(GFX_CONTROLLER, GFX_MISC);
    data = inp(GFX_CONTROLLER + 1);
    outp(GFX_CONTROLLER + 1, RESET_BITS(data, 0x02));

    // make changes to sequencer

    // again we must select no chaining and no odd/even memory addressing
    outp(SEQUENCER, SEQ_MEMORY_MODE);
    data = inp(SEQUENCER + 1);
    data = RESET_BITS(data, 0x08);
    data = SET_BITS(data, 0x04);
    outp(SEQUENCER + 1, data);

    // now clear the screen
    outp(SEQUENCER, SEQ_PLANE_ENABLE);
    outp(SEQUENCER + 1, 0x0f);

    // clear the screen, remember it is 320x400, but that is divided into four
    // planes, hence we need only to clear 32k out since there are four planes
    // each being cleared in parallel for a total of 4*32k or 128 = 320x400
    // note: "k" in this example means 1000 not 1024
#ifdef DOS_32_BIT
    _asm {
        mov edi, VideoBuffer        ; load flat pointer
        xor eax, eax                ; zero out eax
        mov ecx, 320*400/16         ; number of dwords to fill (divide by 16 instead of 8)
        rep stosd                   ; clear with dwords - twice as fast!
    }
#else
    _asm {
        les di,VideoBuffer  ; point es:di to video buffer, same address for mode Z
        xor ax,ax           ; move a zero into al and ah
        mov cx,320*400/8    ; number of words to fill (using word is faster than bytes)
        rep stosw           ; move the color into the video buffer really fast!!
    }
#endif
}

void fillScreenZ(int color) {
    // this function will fill the mode Z video buffer with the sent color

    // use the inline assembler for speed
#ifdef DOS_32_BIT
    _asm {
        mov dx, SEQUENCER           ; address the sequencer
        mov al, SEQ_PLANE_ENABLE    ; select the plane enable register
        mov ah, 0fh                 ; enable all four planes
        out dx, ax                  ; write to sequencer
        mov edi, VideoBuffer        ; load flat pointer
        mov al, BYTE PTR color      ; move the color into al
        mov ah, al                  ; replicate to ax
        mov bx, ax                  ; copy to bx
        shl eax, 16                 ; shift to upper word
        mov ax, bx                  ; eax has 4 copies of color
        mov ecx, 320*400/16         ; number of dwords (divide by 16 instead of 8)
        rep stosd                   ; fill with dwords - twice as fast!
    }
#else
    _asm {
        mov dx,SEQUENCER        ; address the sequencer
        mov al,SEQ_PLANE_ENABLE ; select the plane enable register
        mov ah,0fh              ; enable all four planes
        out dx,ax               ; do it baby!
        les di,VideoBuffer      ; point es:di to video buffer
        mov al,BYTE PTR color   ; move the color into al and ah
        mov ah,al               ; replicate color into ah
        mov cx,320*400/8        ; number of words to fill (using word is faster than bytes)
        rep stosw               ; move the color into the video buffer really fast!
    }
#endif
}

void writePixelZ(int x, int y, int color) {
    // this function will write a pixel to screen in mode Z

    // first select the proper color plane. Use inline for speed.
    // if we used C then there would be a function call and about 10-15 more
    // instructions!
    _asm {
        mov dx,SEQUENCER        ; address the SEQUENCER
        mov al,SEQ_PLANE_ENABLE ; select the plane enable register
        mov cl,BYTE PTR x       ; extract lower byte from x
        and cl,03h              ; extract the plane number = x MOD 4
        mov ah,1                ; a "1" selects the plane in the plane enable
        shl ah,cl               ; shift the "1" bit proper number of times
        out dx,ax               ; do it baby!
    }

    // write the pixel, offset = (y*320+x)/4
    VideoBuffer[(y << 6) + (y << 4) + (x >> 2)] = (unsigned char)color;
}

#ifdef VBE_SUPPORT
int loadFontSet(const char* filename, int size) {
    // this function loads a square, bit-packed font file (like font.bin,
    // but exp_font also writes larger cell sizes: font16.bin/font24.bin/
    // font32.bin) - ceil(size/8) bytes per row, size rows, 256 glyphs.
    // must be called during program initialization before any text rendering
    FILE* fontFile;
    unsigned long total = (unsigned long)((size + 7) / 8) * size * 256;
    unsigned long bytesRead;

    // drop whatever font (if any) is already loaded first
    freeFontSet();

    // allocate memory for the font data
    RomCharSet = (unsigned char*)MALLOC(total);
    if (!RomCharSet) {
        printf("Error: Could not allocate memory for font\n");
        return 0;
    }

    // open the font file
    fontFile = fopen(filename, "rb");
    if (!fontFile) {
        printf("Error: Could not open %s\n", filename);
        printf("Run exp_font.exe to generate it first!\n");
        FREE(RomCharSet);
        RomCharSet = NULL;
        return 0;
    }

    // read the entire font into memory
    bytesRead = (unsigned long)fread(RomCharSet, 1, total, fontFile);

    // close the file
    fclose(fontFile);

    // verify we read all the bytes we expected
    if (bytesRead != total) {
        printf("Error: Could not read font data (got %lu bytes, expected %lu)\n", bytesRead, total);
        FREE(RomCharSet);
        RomCharSet = NULL;
        return 0;
    }

    // success
    GlyphWidth  = size;
    GlyphHeight = size;
    return 1;
}

void freeFontSet(void) {
    // this function frees the memory allocated for the currently loaded font
    if (RomCharSet) {
        FREE(RomCharSet);
        RomCharSet = NULL;
    }
}
#elif defined(DOS_32_BIT)
int initRomCharSet(void) {
    // this function loads the ROM character set from font.bin file
    // must be called during program initialization before any text rendering
    FILE* fontFile;                 // file handle for font.bin
    unsigned int bytesRead;         // number of bytes read from file

    // allocate memory for the font data (256 characters * 8 bytes = 2048 bytes)
    RomCharSet = (unsigned char*)MALLOC(2048);
    if (!RomCharSet) {
        printf("Error: Could not allocate memory for font\n");
        return 0;
    }

    // open the font file
    fontFile = fopen("font.bin", "rb");
    if (!fontFile) {
        printf("Error: Could not open font.bin\n");
        printf("Run exp_font.exe to extract the ROM font first!\n");
        FREE(RomCharSet);
        RomCharSet = NULL;
        return 0;
    }

    // read the entire font into memory
    bytesRead = fread(RomCharSet, 1, 2048, fontFile);

    // close the file
    fclose(fontFile);

    // verify we read all 2048 bytes
    if (bytesRead != 2048) {
        printf("Error: Could not read font data (got %u bytes, expected 2048)\n", bytesRead);
        FREE(RomCharSet);
        RomCharSet = NULL;
        return 0;
    }

    // success
    return 1;
}

void freeRomCharSet(void) {
    // this function frees the memory allocated for the ROM character set
    if (RomCharSet) {
        FREE(RomCharSet);
        RomCharSet = NULL;
    }
}
#endif

void exitToDos(int returnCode) {
    // this function exits directly to DOS using INT 21h function 4Ch
    // bypassing C runtime cleanup
    //
    // This is the standard DOS program termination method and works
    // in both real mode (16-bit) and protected mode (DOS4GW 32-bit)
    //
    // In DOS4GW builds, this avoids a divide-by-zero bug in Watcom's
    // C runtime cleanup code
    //
    // return code: 0 = success, non-zero = error (0-255)

    _asm {
        mov al, byte ptr returnCode  ; AL = return code (0-255)
        mov ah, 4Ch                   ; AH = 4Ch (DOS terminate program)
        int 21h                       ; call DOS - never returns
    }
}
