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

#include "black3.h"
#include "black4.h"

#define VGA_INPUT_STATUS_1   0x3DA // VGA input status register number 1
                                   // D3 is vertical retrace bit
                                   // 1 - retrace in progress
                                   // 0 - no retrace

#define VGA_VRETRACE_MASK 0x08     // masks off unwanted bit of status reg

// these are used to change the visual page of the VGA
#define CRT_ADDR_LOW    0x0D    // the index of the low byte of the start address
#define CRT_ADDR_HI     0x0C    // the index of the hi byte of the start address

#ifdef DOS_32_BIT
unsigned char FAR* Page0Buffer = (unsigned char FAR*)0xA0000;
unsigned char FAR* Page1Buffer = (unsigned char FAR*)0xA8000;
#else
unsigned char FAR* Page0Buffer = (unsigned char FAR*)0xA0000000L;
unsigned char FAR* Page1Buffer = (unsigned char FAR*)0xA0008000L;
#endif

int ModeZPage = PAGE_0;

unsigned char FAR* DoubleBuffer = NULL;

// the default dimensions of the double buffer
unsigned int DoubleBufferHeight = SCREEN_HEIGHT;

// size of double buffer in WORDS (black17.c/black18.c's fquadset calls rely
// on this exact unit via ">> 1" for a dword count - do not reinterpret it as
// bytes)
unsigned int DoubleBufferSize = SCREEN_WIDTH * SCREEN_HEIGHT / 2;

void screenTransition(int effect) {
    // this function can be called to perform a myriad of screen transitions
    // to the video buffer, note I have left one for you to create!
    int palReg;
    long index;
    RgbColor color;

    switch (effect) {
        case SCREEN_DARKNESS:
        case SCREEN_WHITENESS:
        {
#ifdef VBE_SUPPORT
            if (DisplayBpp == 8) {
#endif
                // paletted: fade through the DAC, exactly as the book does
                if (effect == SCREEN_DARKNESS) {
                    for (index = 0; index < 20; index++) {
                        for (palReg = 1; palReg < 255; palReg++) {
                            readColorReg(palReg, &color);

                            if (color.red > 4)   { color.red   -= 3; } else { color.red   = 0; }
                            if (color.green > 4) { color.green -= 3; } else { color.green = 0; }
                            if (color.blue > 4)  { color.blue  -= 3; } else { color.blue  = 0; }

                            writeColorReg(palReg, &color);
                        }
                        timeDelay(1);
                    }
                } else {
                    for (index = 0; index < 20; index++) {
                        for (palReg = 0; palReg < 255; palReg++) {
                            readColorReg(palReg, &color);

                            color.red += 4;
                            if (color.red > 63) { color.red = 63; }
                            color.green += 4;
                            if (color.green > 63) { color.green = 63; }
                            color.blue += 4;
                            if (color.blue > 63) { color.blue = 63; }

                            writeColorReg(palReg, &color);
                        }
                        timeDelay(1);
                    }
                }
#ifdef VBE_SUPPORT
            } else {
                // no DAC to fade through at bpp>8: re-render every pixel from
                // a one-time snapshot of the current frame, recomputed fresh
                // each step (not compounded) so rounding doesn't drift over
                // the ramp. Same 20-step count as the bpp==8 path above, each
                // step pinned to one timeDelay(1) BIOS tick, so the fade
                // takes the same real time regardless of resolution.
                unsigned long total   = PITCH_OFFSET(DisplayHeight);
                unsigned char FAR* snapshot = (unsigned char FAR*)MALLOC(total);
                int bytespp = DisplayBpp / 8;
                unsigned char fadeLut[256];
                int step, y, x, v;

                if (snapshot == NULL) {
                    break;
                }
                MEMCPY(snapshot, VideoBuffer, total);

                for (step = 19; step >= 0; step--) {
                    if (effect == SCREEN_WHITENESS) {
                        for (v = 0; v < 256; v++) {
                            fadeLut[v] = (unsigned char)(v + (255 - v) * (19 - step) / 19);
                        }
                    } else {
                        for (v = 0; v < 256; v++) {
                            fadeLut[v] = (unsigned char)(v * step / 19);
                        }
                    }

                    if (bytespp == 2) {
                        for (y = 0; y < DisplayHeight; y++) {
                            unsigned char FAR* srow = snapshot   + PITCH_OFFSET(y);
                            unsigned char FAR* drow = VideoBuffer + PITCH_OFFSET(y);
                            for (x = 0; x < DisplayWidth; x++) {
                                unsigned short px = *(unsigned short FAR*)(srow + x * 2);
                                unsigned char  r  = (unsigned char)(((px >> 11) & 0x1F) << 3);
                                unsigned char  g  = (unsigned char)(((px >> 5)  & 0x3F) << 2);
                                unsigned char  b  = (unsigned char)((px & 0x1F) << 3);
                                unsigned short outv = (unsigned short)
                                    (((unsigned long)(fadeLut[r] >> 3) << 11) |
                                     ((unsigned long)(fadeLut[g] >> 2) << 5)  |
                                     (fadeLut[b] >> 3));
                                *(unsigned short FAR*)(drow + x * 2) = outv;
                            }
                        }
                    } else {   // 32bpp
                        for (y = 0; y < DisplayHeight; y++) {
                            unsigned long FAR* srow = (unsigned long FAR*)(snapshot   + PITCH_OFFSET(y));
                            unsigned long FAR* drow = (unsigned long FAR*)(VideoBuffer + PITCH_OFFSET(y));
                            for (x = 0; x < DisplayWidth; x++) {
                                unsigned long px = srow[x];
                                unsigned char r = fadeLut[(unsigned char)(px >> 16)];
                                unsigned char g = fadeLut[(unsigned char)(px >> 8)];
                                unsigned char b = fadeLut[(unsigned char)px];
                                drow[x] = ((unsigned long)r << 16) | ((unsigned long)g << 8) | b;
                            }
                        }
                    }
                    timeDelay(1);
                }
                FREE(snapshot);
            }
#endif

            break;
        }

        case SCREEN_WARP:
        {
            // this one you do!!
            break;
        }

        case SCREEN_SWIPE_X:
        {
            // do a screen swipe from right to left, left to right
#ifdef VBE_SUPPORT
            for (index = 0; index < (DisplayWidth / 2); index += 2) {
                waitForVerticalRetrace();
                lineV(0, DisplayHeight - 1, (DisplayWidth - 1) - (int)index, 0);
                lineV(0, DisplayHeight - 1, (int)index, 0);
                lineV(0, DisplayHeight - 1, (DisplayWidth - 1) - (int)(index + 1), 0);
                lineV(0, DisplayHeight - 1, (int)(index + 1), 0);
            }
#else
            for (index = 0; index < 160; index += 2) {
                waitForVerticalRetrace();
                lineV(0, 199, 319 - (int)index, 0);
                lineV(0, 199, (int)index, 0);
                lineV(0, 199, 319 - (int)(index + 1), 0);
                lineV(0, 199, (int)(index + 1), 0);
            }
#endif
            break;
        }

        case SCREEN_SWIPE_Y:
        {
            // do a screen swipe from top to bottom, bottom to top
#ifdef VBE_SUPPORT
            for (index = 0; index < (DisplayHeight / 2); index += 2) {
                waitForVerticalRetrace();
                lineH(0, DisplayWidth - 1, (DisplayHeight - 1) - (int)index, 0);
                lineH(0, DisplayWidth - 1, (int)index, 0);
                lineH(0, DisplayWidth - 1, (DisplayHeight - 1) - (int)(index + 1), 0);
                lineH(0, DisplayWidth - 1, (int)(index + 1), 0);
            }
#else
            for (index = 0; index < 100; index += 2) {
                waitForVerticalRetrace();
                lineH(0, 319, 199 - (int)index, 0);
                lineH(0, 319, (int)index, 0);
                lineH(0, 319, 199 - (int)(index + 1), 0);
                lineH(0, 319, (int)(index + 1), 0);
            }
#endif
            break;
        }

        case SCREEN_DISSOLVE:
        {
            // dissolve the screen by plotting zillions of little black dots
            for (index = 0; index <= 300000; index++) {
#ifdef VBE_SUPPORT
                writePixel(rand() % DisplayWidth, rand() % DisplayHeight, 0);
#else
                writePixel(rand() % 320, rand() % 200, 0);
#endif
            }
            break;
        }

        default:
            break;
    }
}

void waitForVerticalRetrace(void) {
    // this function waits for the start of a vertical retrace, if a vertical
    // retrace is in progress then it waits until the next one
    // therefore the function can wait a maximum of 2/70 th's of a second
    // before returning
    // this function can be used to synchronize video updates to the vertical blank
    // or as a high resolution time reference

    while (inp(VGA_INPUT_STATUS_1) & VGA_VRETRACE_MASK) {
        // do nothing, vga is already in retrace
    }

    // now wait for start of vertical retrace and exit
    while (!(inp(VGA_INPUT_STATUS_1) & VGA_VRETRACE_MASK)) {
        // do nothing, wait for start of retrace
    }

    // at this point a vertical retrace is occuring, so return back to caller
}

void printCharDb(int xc, int yc, char c, int color, int transparent) {
#ifdef VBE_SUPPORT
    // this function is used to print a character on the double buffer,
    // using whichever font is currently loaded (GlyphWidth x GlyphHeight,
    // bit-packed MSB-left, ceil(GlyphWidth/8) bytes per row - see black3.c's
    // printChar, which this mirrors exactly except for the destination buffer)
    int x, y, bpr;
    unsigned char FAR* workChar;
    unsigned char FAR* rowPtr;
    unsigned char bitMask;

    if (RomCharSet == NULL) {      // no font loaded (loadFontSet failed/never called yet)
        return;
    }

    bpr = (GlyphWidth + 7) / 8;
    workChar = RomCharSet + (unsigned long)(unsigned char)c * GlyphHeight * bpr;

    rowPtr  = DoubleBuffer + PITCH_OFFSET(yc) + ((unsigned long)xc << DisplayBppShift);

    // clip against DisplayWidth/DisplayHeight - same reasoning as black3.c's
    // printChar (a caller isn't required to pre-compute a safe range)
    for (y = 0; y < GlyphHeight; y++) {
        if (yc + y >= 0 && yc + y < DisplayHeight) {
            for (x = 0; x < GlyphWidth; x++) {
                if (xc + x >= 0 && xc + x < DisplayWidth) {
                    bitMask = (unsigned char)(0x80 >> (x % 8));

                    if ((workChar[x / 8] & bitMask)) {
                        unsigned char FAR* p = rowPtr + ((unsigned long)x << DisplayBppShift);
                        switch (DisplayBpp) {
                            case 8:  *p = (unsigned char)color; break;
                            case 16: *(unsigned short FAR*)p = (unsigned short)color; break;
                            default: *(unsigned long FAR*)p  = (unsigned long)color; break;   // 32
                        }
                    } else if (!transparent) {
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

        rowPtr += DisplayPitch;
        workChar += bpr;
    }
#else
    // mode-13h only: the book's original fixed 8x8 ROM font
    int offset, x, y;
    unsigned char FAR* workChar;
    unsigned char bitMask;

    workChar = RomCharSet + c * ROM_CHAR_HEIGHT;
    offset = (yc << 8) + (yc << 6) + xc;

    for (y = 0; y < ROM_CHAR_HEIGHT; y++) {
        bitMask = 0x80;

        for (x = 0; x < ROM_CHAR_WIDTH; x++) {
            if ((*workChar & bitMask)) {
                DoubleBuffer[offset + x] = (unsigned char)color;
            } else if (!transparent) {
                DoubleBuffer[offset + x] = 0;
            }
            bitMask = bitMask >> 1;
        }

        offset += MODE13_WIDTH;
        workChar++;
    }
#endif
}

void printStringDb(int x, int y, int color, char* string, int transparent) {
    // this function prints an entire string into the double buffer with
    // fixed spacing (the current font's glyph width) between each character
    // by calling the printCharDb() function
    int currentX = x;
    char* currentChar = string;

    while (*currentChar != '\0') {
        printCharDb(currentX, y, *currentChar, color, transparent);
#ifdef VBE_SUPPORT
        currentX += GlyphWidth;
#else
        currentX += ROM_CHAR_WIDTH;
#endif
        currentChar++;
    }
}

void writePixelDb(int x, int y, int color) {
#ifdef VBE_SUPPORT
    unsigned char FAR* p = DoubleBuffer + PITCH_OFFSET(y) + ((unsigned long)x << DisplayBppShift);

    switch (DisplayBpp) {
        case 8:  *p = (unsigned char)color; break;
        case 16: *(unsigned short FAR*)p = (unsigned short)color; break;
        default: *(unsigned long FAR*)p  = (unsigned long)color; break;   // 32
    }
#else
    DoubleBuffer[(y << 8) + (y << 6) + x] = (unsigned char)color;
#endif
}

int readPixelDb(int x, int y) {
#ifdef VBE_SUPPORT
    unsigned char FAR* p = DoubleBuffer + PITCH_OFFSET(y) + ((unsigned long)x << DisplayBppShift);

    switch (DisplayBpp) {
        case 8:  return (int)(*p);
        case 16: return (int)(*(unsigned short FAR*)p);
        default: return (int)(*(unsigned long FAR*)p);   // 32
    }
#else
    return (int)DoubleBuffer[((y << 8) + (y << 6)) + x];
#endif
}

int createDoubleBuffer(int numLines) {
#ifdef VBE_SUPPORT
    // allocate enough memory to hold the double buffer - one row's pitch
    // matches the current screen mode's DisplayPitch, whatever it is, so any
    // buffer this allocates can be blitted straight to VideoBuffer or passed
    // to the same bitmapPut/spriteDraw/etc. calls that also take VideoBuffer
    unsigned long allocBytes = PITCH_OFFSET(numLines + 1);
    unsigned long fillBytes  = PITCH_OFFSET(numLines);

    if ((DoubleBuffer = (unsigned char FAR*)MALLOC(allocBytes)) == NULL) {
        printf("\nCouldn't allocate double buffer.");
        return 0;
    }

    DoubleBufferHeight = numLines;
    DoubleBufferSize = (unsigned int)(fillBytes / 2);

    MEMSET(DoubleBuffer, 0, fillBytes);
    return 1;
#else
    // mode-13h only: fixed 320-wide rows
    if ((DoubleBuffer = (unsigned char FAR*)MALLOC(SCREEN_WIDTH * (numLines + 1))) == NULL) {
        printf("\nCouldn't allocate double buffer.");
        return 0;
    }

    DoubleBufferHeight = numLines;
    DoubleBufferSize = SCREEN_WIDTH * numLines / 2;

    MEMSET(DoubleBuffer, 0, SCREEN_WIDTH * numLines);
    return 1;
#endif
}

void fillDoubleBuffer(int color) {
#ifdef VBE_SUPPORT
    {
        unsigned long bytes = PITCH_OFFSET(DoubleBufferHeight);
        unsigned dwords, rem;

        switch (DisplayBpp) {
            case 8: {
                unsigned long fill = (unsigned char)color;
                fill |= fill << 8;
                fill |= fill << 16;
                dwords = (unsigned)(bytes >> 2);
                rem    = (unsigned)(bytes & 3);
                asmStosDwordsBytes(DoubleBuffer, fill, dwords, rem);
                break;
            }
            case 16: {
                unsigned long c    = (unsigned short)color;
                unsigned long fill = c | (c << 16);
                dwords = (unsigned)(bytes >> 2);
                rem    = (unsigned)(bytes & 3);
                asmStosDwordsWords(DoubleBuffer, fill, dwords, rem);
                break;
            }
            default: {   // 32
                dwords = (unsigned)(bytes >> 2);
                asmStosDwords(DoubleBuffer, (unsigned long)color, dwords);
                break;
            }
        }
    }
#else
    // mode-13h only: fixed fill, one WORD at a time
    int size = DoubleBufferSize;

#ifdef DOS_32_BIT
    _asm {
        mov ecx, size               ; size in WORDs
        mov al, BYTE PTR color      ; move the color into al
        mov ah, al                  ; replicate to ax
        mov bx, ax                  ; copy to bx
        shl eax, 16                 ; shift to upper word
        mov ax, bx                  ; eax has 4 copies of color
        mov edi, DoubleBuffer       ; load flat pointer
        shr ecx, 1                  ; convert word count to dword count
        rep stosd                   ; fill with dwords (2x faster)

        mov ecx, size
        and ecx, 1                  ; check if size was odd
        rep stosw                   ; fill remaining word if any
    }
#else
    _asm {
        mov cx,size             ; this is the size of buffer in WORDs
        mov al,BYTE PTR color   ; move the color into al
        mov ah,al               ; move the color in ah
        les di,DoubleBuffer     ; es:di points to the double buffer
        rep stosw               ; fill all the words
    }
#endif
#endif
}

void displayDoubleBuffer(unsigned char FAR* buffer, int y) {
    // this function copies the double buffer into the video buffer at the
    // starting y location
#ifdef VBE_SUPPORT
    {
        unsigned char* destination = VideoBuffer + PITCH_OFFSET(y);
        unsigned long  bytes       = PITCH_OFFSET(DoubleBufferHeight);
        unsigned dwords = (unsigned)(bytes >> 2);
        unsigned rem    = (unsigned)(bytes & 3);
        asmMovDwordsBytes(destination, buffer, dwords, rem);
    }
#else
    // mode-13h only: fixed 320-wide copy
    void FAR* destination = (void FAR*)VideoBuffer;
    int size = DoubleBufferSize;

#ifdef DOS_32_BIT
    _asm {
        mov ecx, size               ; size in WORDs
        mov edi, destination        ; load destination flat pointer
        movzx eax, WORD PTR y       ; load y
        imul eax, 320               ; multiply by screen width
        add edi, eax                ; add offset
        mov esi, buffer             ; load source flat pointer
        shr ecx, 1                  ; convert word count to dword count
        rep movsd                   ; move dwords (4 bytes at a time - 2x faster)

        mov ecx, size               ; handle odd word if size was odd
        and ecx, 1
        rep movsw                   ; move remaining word if any
    }
#else
    _asm {
        push ds                 ; save DS on stack
        mov cx,size             ; this is the size of buffer in WORDs
        les di,destination      ; es:di is destination of memory move
        mov ax,320              ; multiply y by 320 i.e. screen width
        mul y
        add di,ax               ; add result to es:di
        lds si,buffer           ; ds:si is source of memory move
        rep movsw               ; move all the words
        pop ds                  ; restore the data segment
    }
#endif
#endif
}

void deleteDoubleBuffer(void) {
    // this function frees up the memory allocated by the double buffer
    // make sure to use FAR version
    if (DoubleBuffer) {
        FREE(DoubleBuffer);
        DoubleBuffer = NULL;
    }
}

void bitmapPut(BitmapPtr image, unsigned char FAR* destination, int transparent) {
    // this function draws a bitmap on the destination buffer which can
    // be a double buffer or the video buffer
#ifdef VBE_SUPPORT
    int x, y, width, height;
    unsigned char FAR* bitmapData;
    unsigned char FAR* destBuffer;

    destBuffer = destination + PITCH_OFFSET(image->y) + ((unsigned long)image->x << DisplayBppShift);

    height = image->height;
    width = image->width;
    bitmapData = image->buffer;

    if (!transparent) {
        for (y = 0; y < height; y++) {
            MEMCPY((void FAR*)destBuffer, (void FAR*)bitmapData, (unsigned long)width << DisplayBppShift);
            destBuffer += DisplayPitch;
            bitmapData += (unsigned long)width << DisplayBppShift;
        }
        return;
    }

    // transparent: a tight per-width loop specialized for DisplayBpp - no
    // per-pixel format branching. 0 is the transparent value at every bpp.
    switch (DisplayBpp) {
        case 8:
            for (y = 0; y < height; y++) {
                for (x = 0; x < width; x++) {
                    unsigned char pixel = bitmapData[x];
                    if (pixel) { destBuffer[x] = pixel; }
                }
                destBuffer += DisplayPitch;
                bitmapData += width;
            }
            break;
        case 16:
            for (y = 0; y < height; y++) {
                unsigned short FAR* src = (unsigned short FAR*)bitmapData;
                unsigned short FAR* dst = (unsigned short FAR*)destBuffer;
                for (x = 0; x < width; x++) {
                    unsigned short pixel = src[x];
                    if (pixel) { dst[x] = pixel; }
                }
                destBuffer += DisplayPitch;
                bitmapData += (unsigned long)width * 2;
            }
            break;
        default:   // 32
            for (y = 0; y < height; y++) {
                unsigned long FAR* src = (unsigned long FAR*)bitmapData;
                unsigned long FAR* dst = (unsigned long FAR*)destBuffer;
                for (x = 0; x < width; x++) {
                    unsigned long pixel = src[x];
                    if (pixel) { dst[x] = pixel; }
                }
                destBuffer += DisplayPitch;
                bitmapData += (unsigned long)width * 4;
            }
            break;
    }
#else
    // mode-13h only: the book's original fixed 320-wide, 1 byte/pixel form
    int x, y, width, height;
    unsigned char FAR* bitmapData;
    unsigned char FAR* destBuffer;
    unsigned char pixel;

    destBuffer = destination + (image->y << 8) + (image->y << 6) + image->x;

    height = image->height;
    width = image->width;
    bitmapData = image->buffer;

    if (transparent) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (pixel = bitmapData[x]) {
                    destBuffer[x] = pixel;
                }
            }
            destBuffer += SCREEN_WIDTH;
            bitmapData += width;
        }
    } else {
        for (y = 0; y < height; y++) {
            MEMCPY((void FAR*)destBuffer, (void FAR*)bitmapData, width);
            destBuffer += SCREEN_WIDTH;
            bitmapData += width;
        }
    }
#endif
}

void bitmapGet(BitmapPtr image, PcxPicturePtr source) {
    // this function will scan a bitmap out of a loaded PCX picture (e.g.
    // blazer.c's/krk.c's TechFont extraction out of a font sheet PCX)
#ifdef VBE_SUPPORT
    // takes the whole PcxPicture, not just its buffer, because the row
    // stride has to be THIS FILE'S OWN width (pcxLoad packed image->buffer
    // at that stride), not DisplayWidth: a sprite-sheet PCX like a font is
    // its own small size, unrelated to the screen resolution (only a
    // full-screen background PCX happens to match DisplayWidth)
    unsigned long sourceOff, bitmapOff;
    int y, width, height, pcxWidth;
    unsigned char FAR* bitmapData;
    unsigned char FAR* sourceBuffer;

    pcxWidth = source->header.width - source->header.x + 1;
    sourceBuffer = source->buffer;

    sourceOff = ((unsigned long)image->y * pcxWidth << DisplayBppShift) + ((unsigned long)image->x << DisplayBppShift);
    bitmapOff = 0;

    height = image->height;
    width = image->width;
    bitmapData = image->buffer;

    for (y = 0; y < height; y++) {
        MEMCPY((void FAR*)&bitmapData[bitmapOff], (void FAR*)&sourceBuffer[sourceOff], (unsigned long)width << DisplayBppShift);
        sourceOff += (unsigned long)pcxWidth << DisplayBppShift;
        bitmapOff += (unsigned long)width << DisplayBppShift;
    }
#else
    // mode-13h only: the book's original form - every PCX buffer (source
    // or destination) is assumed 320 bytes/row, 1 byte/pixel
    unsigned int sourceOff, bitmapOff;
    int y, width, height;
    unsigned char FAR* bitmapData;
    unsigned char FAR* sourceBuffer = source->buffer;

    sourceOff = (image->y << 8) + (image->y << 6) + image->x;
    bitmapOff = 0;

    height = image->height;
    width = image->width;
    bitmapData = image->buffer;

    for (y = 0; y < height; y++) {
        MEMCPY((void FAR*)&bitmapData[bitmapOff], (void FAR*)&sourceBuffer[sourceOff], width);
        sourceOff += SCREEN_WIDTH;
        bitmapOff += width;
    }
#endif
}

int bitmapAllocate(BitmapPtr image, int width, int height) {
#ifdef VBE_SUPPORT
    if ((image->buffer = (unsigned char FAR*)MALLOC((unsigned long)width * height * (DisplayBpp / 8) + 1)) == NULL) {
        return 0;
    } else {
        return 1;
    }
#else
    if ((image->buffer = (unsigned char FAR*)MALLOC(width * height + 1)) == NULL) {
        return 0;
    } else {
        return 1;
    }
#endif
}

void bitmapDelete(BitmapPtr image) {
    // this function deletes the memory used by a bitmap
    if (image->buffer) {
        FREE(image->buffer);
    }

    image->buffer = NULL;
}

int pcxInit(PcxPicturePtr image) {
#ifdef VBE_SUPPORT
    // this function allocates the buffer that the image data will be loaded
    // into when a PCX file is decompressed - sized for the CURRENT mode,
    // since a PCX asset is expected to match the active screen's dimensions
    // and depth
    unsigned long bytes = (unsigned long)DisplayWidth * DisplayHeight * (DisplayBpp / 8) + 1;

    if (!(image->buffer = (unsigned char FAR*)MALLOC(bytes))) {
        printf("\nPCX SYSTEM - Couldn't allocate PCX image buffer");
        return 0;
    }
    image->bpp = DisplayBpp;
    return 1;
#else
    // mode-13h only: fixed 320x200
    if (!(image->buffer = (unsigned char FAR*)MALLOC(SCREEN_WIDTH * SCREEN_HEIGHT + 1))) {
        printf("\nPCX SYSTEM - Couldn't allocate PCX image buffer");
        return 0;
    }
    return 1;
#endif
}

void pcxDelete(PcxPicturePtr image) {
    // this function de-allocates the buffer region used for the pcx file load
    FREE(image->buffer);
}

#ifdef VBE_SUPPORT
// RLE-decode one scan line (bpl encoded bytes) into dst (width bytes,
// clipped). Same 0xC0-run-length scheme regardless of what a byte represents
// (a palette index, or one plane of an RGB triplet).
static void pcxDecodeLine(FILE* fp, unsigned char FAR* dst, int width, int bpl) {
    int col = 0;
    while (col < bpl) {
        int b = fgetc(fp);
        if (b == EOF) {
            break;
        }
        if ((b & 0xC0) == 0xC0) {              // run: low 6 bits = count
            int count = b & 0x3F;
            int val   = fgetc(fp);
            while (count-- > 0) {
                if (col < width) { dst[col] = (unsigned char)val; }
                col++;
            }
        } else {                               // literal pixel
            if (col < width) { dst[col] = (unsigned char)b; }
            col++;
        }
    }
}
#endif

int pcxLoad(char* filename, PcxPicturePtr image, int loadPalette) {
#ifdef VBE_SUPPORT
    // this function loads a PCX file into the image structure: 1. load the
    // header, 2. decode the image data (indexed 256-colour, or 24-bit true
    // colour widened to 32bpp on load) and 3. load the palette data and
    // update the VGA palette (indexed only; loaded only if loadPalette is 1)
    FILE* fp;
    int   width, height, bpl, numPlanes, row, col, index;
    char FAR* tempBuffer;

    if ((fp = fopen(filename, "rb")) == NULL) {
        printf("\nPCX SYSTEM - Couldn't find file: %s", filename);
        return 0;
    }

    tempBuffer = (char FAR*)&image->header;
    for (index = 0; index < 128; index++) {
        tempBuffer[index] = (char)getc(fp);
    }

    width     = image->header.width  - image->header.x + 1;   // width/height hold xmax/ymax
    height    = image->header.height - image->header.y + 1;
    numPlanes = image->header.numColorPlanes;   // 1 = 256-colour indexed, 3 = 24-bit RGB
    bpl       = image->header.bytesPerLine;     // encoded bytes per scan line, per plane

    // this file's own depth (8 for indexed, 32 for true colour) must match
    // the runtime DisplayBpp - every consumer (pcxShowBuffer/CopyToBuffer,
    // pcxGetSprite/Tinted) assumes it does and has no bpp check of its own,
    // so a mismatch would silently corrupt memory or the screen instead of
    // erroring. Fail here instead, before any decoding.
    if ((numPlanes == 3 ? 32 : 8) != DisplayBpp) {
        fclose(fp);
        return 0;
    }

    if (numPlanes == 3) {
        // 24-bit true colour: no palette - each scan line is a full plane of
        // R, then a full plane of G, then B. Widen to 32bpp on load.
        unsigned char* planes = (unsigned char*)malloc((unsigned long)bpl * 3);
        if (planes == NULL) {
            fclose(fp);
            return 0;
        }
        image->bpp = 32;
        for (row = 0; row < height; row++) {
            unsigned char FAR* dst = image->buffer + (unsigned long)row * width * 4;
            int plane;
            for (plane = 0; plane < 3; plane++) {
                pcxDecodeLine(fp, planes + plane * bpl, bpl, bpl);
            }
            for (col = 0; col < width; col++) {
                unsigned char r = planes[col];
                unsigned char g = planes[bpl + col];
                unsigned char b = planes[bpl * 2 + col];
                *(unsigned long FAR*)(dst + (unsigned long)col * 4) =
                    ((unsigned long)r << 16) | ((unsigned long)g << 8) | b;
            }
        }
        free(planes);
        fclose(fp);
        return 1;
    }

    // 256-colour indexed (numPlanes == 1)
    image->bpp = 8;
    for (row = 0; row < height; row++) {
        pcxDecodeLine(fp, image->buffer + (unsigned long)row * width, width, bpl);
    }

    // 256-colour palette: trailing 0x0C marker + 768 bytes (8-bit RGB -> 6-bit)
    if (fseek(fp, -769L, SEEK_END) == 0 && fgetc(fp) == 0x0C) {
        for (index = 0; index < 256; index++) {
            image->palette[index].red   = (unsigned char)(fgetc(fp) >> 2);
            image->palette[index].green = (unsigned char)(fgetc(fp) >> 2);
            image->palette[index].blue  = (unsigned char)(fgetc(fp) >> 2);
        }
        if (loadPalette) {
            for (index = 0; index < 256; index++) {
                writeColorReg(index, &image->palette[index]);
            }
        }
    }

    fclose(fp);
    return 1;
#else
    // mode-13h only: the book's original simple, flat 320x200 decoder - no
    // per-file dimensions, no true-colour support, just a raw 64000-byte
    // 256-colour RLE stream
    FILE* fp;
    int numBytes, index;
    long count;
    unsigned char data;
    char FAR* tempBuffer;

    if ((fp = fopen(filename, "rb")) == NULL) {
        printf("\nPCX SYSTEM - Couldn't find file: %s", filename);
        return 0;
    }

    tempBuffer = (char FAR*)image;
    for (index = 0; index < 128; index++) {
        tempBuffer[index] = (char)getc(fp);
    }

    count = 0;
    while (count < SCREEN_WIDTH * SCREEN_HEIGHT) {
        data = (unsigned char)getc(fp);

        if (data >= 192) {
            numBytes = data - 192;
            data = (unsigned char)getc(fp);
            while ((numBytes--) > 0) {
                image->buffer[count++] = data;
            }
        } else {
            image->buffer[count++] = data;
        }
    }

    fseek(fp, -768L, SEEK_END);
    for (index = 0; index < 256; index++) {
        image->palette[index].red   = (unsigned char)(getc(fp) >> 2);
        image->palette[index].green = (unsigned char)(getc(fp) >> 2);
        image->palette[index].blue  = (unsigned char)(getc(fp) >> 2);
    }

    fclose(fp);

    if (loadPalette) {
        for (index = 0; index < 256; index++) {
            writeColorReg(index, &image->palette[index]);
        }
    }

    return 1;
#endif
}

void pcxShowBuffer(PcxPicturePtr image) {
#ifdef VBE_SUPPORT
    // copy the PCX buffer into the video buffer, one row at a time - the PCX
    // buffer is packed tight (DisplayWidth*bytespp per row) but VideoBuffer's
    // DisplayPitch may include hardware padding, so a single contiguous copy
    // isn't safe in general
    int row;
    unsigned long rowBytes = (unsigned long)DisplayWidth << DisplayBppShift;
    unsigned char FAR* src = image->buffer;
    unsigned char FAR* dst = VideoBuffer;

    for (row = 0; row < DisplayHeight; row++) {
        MEMCPY((void FAR*)dst, (void FAR*)src, rowBytes);
        dst += DisplayPitch;
        src += rowBytes;
    }
#else
    // mode-13h only: single contiguous 320x200 copy - no padding possible
    // at a fixed 320-wide mode, so no row-by-row needed
    char FAR* data = image->buffer;

#ifdef DOS_32_BIT
    _asm {
        mov edi, VideoBuffer        ; load destination flat pointer
        mov esi, data               ; load source flat pointer
        mov ecx, 320*200/4          ; move 16000 dwords (instead of 32000 words)
        cld                         ; set direction to forward
        rep movsd                   ; copy dwords (4 bytes at a time - 2x faster!)
    }
#else
    _asm {
        push ds             ; save the data segment
        les di,VideoBuffer  ; point es:di to video buffer
        lds si,data         ; point ds:si to data area
        mov cx,320*200/2    ; move 32000 words
        cld                 ; set direction to forward
        rep movsw           ; do the string operation
        pop ds              ; restore the data segment
    }
#endif
#endif
}

void spriteInit(
    SpritePtr sprite,
    int x, int y,
    int width, int height,
    int c1, int c2, int c3,
    int t1, int t2, int t3) {

    // this function initializes a sprite
    int index;

    sprite->x = x;
    sprite->y = y;
    sprite->width = width;
    sprite->height = height;
    sprite->visible = 1;
    sprite->counter1 = c1;
    sprite->counter2 = c2;
    sprite->counter3 = c3;
    sprite->threshold1 = t1;
    sprite->threshold2 = t2;
    sprite->threshold3 = t3;
    sprite->currFrame = 0;
    sprite->state = SPRITE_DEAD;
    sprite->numFrames = 0;
#ifdef VBE_SUPPORT
    sprite->transparentColor = 0;   // "index 0 is transparent" (see spriteDrawTinted)
    sprite->background = (unsigned char FAR*)MALLOC((unsigned long)width * height * (DisplayBpp / 8) + 1);

    if (!sprite->background) {
        printf("\nERROR: spriteInit() - Out of memory allocating background (%ld bytes)\n",
            (long)width * height * (DisplayBpp / 8) + 1);
        return;
    }

    for (index = 0; index < MAX_SPRITE_FRAMES; index++) {
        sprite->frames[index] = NULL;
        sprite->tintMask[index] = NULL;
    }
#else
    sprite->background = (unsigned char FAR*)MALLOC(width * height + 1);

    if (!sprite->background) {
        printf("\nERROR: spriteInit() - Out of memory allocating background (%d bytes)\n", width * height + 1);
        return;
    }

    for (index = 0; index < MAX_SPRITE_FRAMES; index++) {
        sprite->frames[index] = NULL;
    }
#endif
}

void spriteDelete(SpritePtr sprite) {
    // this function deletes all the memory associated with a sprite
    int index;

    FREE(sprite->background);

    for (index = 0; index < MAX_SPRITE_FRAMES; index++) {
        FREE(sprite->frames[index]);
#ifdef VBE_SUPPORT
        FREE(sprite->tintMask[index]);
#endif
    }
}

void pcxGetSprite(
    PcxPicturePtr image,
    SpritePtr sprite,
    int spriteFrame,
    int cellX, int cellY) {

    // this function is used to load the images for a sprite into the sprite
    // frames array. It functions by using the size of the sprite and the
    // position of the requested cell to compute the proper location in the
    // PCX image buffer to extract the data from.
#ifdef VBE_SUPPORT
    int xOff, yOff, y, width, height,
        pcxWidth,   // this PCX file's own width - pcxLoad packed image->buffer
                    // at this stride, which is NOT necessarily DisplayWidth (a
                    // sprite sheet like alienimg.pcx is its own small size,
                    // only full-screen background PCXs happen to match)
        bytespp;
    unsigned char FAR* spriteData;

    width = sprite->width;
    height = sprite->height;
    bytespp = DisplayBpp / 8;
    pcxWidth = image->header.width - image->header.x + 1;

    sprite->frames[spriteFrame] = (unsigned char FAR*)MALLOC((unsigned long)width * height * bytespp + 1);
    spriteData = sprite->frames[spriteFrame];

    if (!sprite->frames[spriteFrame]) {
        printf("\nERROR: pcxGetSprite() - Out of memory allocating frame %d (%ld bytes)\n",
            spriteFrame, (long)width * height * bytespp + 1);
        return;
    }

    // 1-pixel grid lines between cells
    xOff = (width + 1) * cellX + 1;
    yOff = (height + 1) * cellY + 1;

    for (y = 0; y < height; y++) {
        MEMCPY(
            (void FAR*)&spriteData[(unsigned long)y * width << DisplayBppShift],
            (void FAR*)&image->buffer[((unsigned long)(yOff + y) * pcxWidth + xOff) << DisplayBppShift],
            (unsigned long)width << DisplayBppShift);
    }

    sprite->numFrames++;
#else
    // mode-13h only: the book's original fixed 320-wide, 1 byte/pixel form
    int xOff, yOff, y, width, height;
    unsigned char FAR* spriteData;

    width = sprite->width;
    height = sprite->height;

    sprite->frames[spriteFrame] = (unsigned char FAR*)MALLOC(width * height + 1);
    spriteData = sprite->frames[spriteFrame];

    if (!sprite->frames[spriteFrame]) {
        printf("\nERROR: pcxGetSprite() - Out of memory allocating frame %d (%d bytes)\n",
            spriteFrame, width * height + 1);
        return;
    }

    xOff = (width + 1) * cellX + 1;
    yOff = (height + 1) * cellY + 1;
    yOff = yOff * 320;   // 320 bytes per line

    for (y = 0; y < height; y++, yOff += 320) {
        MEMCPY(
            (void FAR*)&spriteData[y * width],
            (void FAR*)&image->buffer[yOff + xOff],
            width);
    }

    sprite->numFrames++;
#endif
}

void spriteDraw(SpritePtr sprite, unsigned char FAR* buffer, int transparent) {
    // this function draws a sprite on the screen row by row very quickly
    // if the transparent flag is true then pixels will be drawn one by one
    // else a memcpy will be used to draw each line
#ifdef VBE_SUPPORT
    unsigned char FAR* spriteData;
    unsigned char FAR* destBuffer;
    int x, y, width, height;

    spriteData = sprite->frames[sprite->currFrame];
    if (spriteData == NULL) {
        return;
    }

    width = sprite->width;
    height = sprite->height;

    destBuffer = buffer + PITCH_OFFSET(sprite->y) + ((unsigned long)sprite->x << DisplayBppShift);

    if (!transparent) {
        for (y = 0; y < height; y++) {
            MEMCPY((void FAR*)destBuffer, (void FAR*)spriteData, (unsigned long)width << DisplayBppShift);
            destBuffer += DisplayPitch;
            spriteData += (unsigned long)width << DisplayBppShift;
        }
        return;
    }

    // transparent: a tight per-width loop specialized for DisplayBpp - no
    // per-pixel format branching. Skips pixels equal to transparentColor
    // (0 by default, matching the historical "index 0 is transparent" rule).
    switch (DisplayBpp) {
        case 8: {
            unsigned char key = (unsigned char)sprite->transparentColor;
            for (y = 0; y < height; y++) {
                for (x = 0; x < width; x++) {
                    unsigned char pixel = spriteData[x];
                    if (pixel != key) { destBuffer[x] = pixel; }
                }
                destBuffer += DisplayPitch;
                spriteData += width;
            }
            break;
        }
        case 16: {
            unsigned short key = (unsigned short)sprite->transparentColor;
            for (y = 0; y < height; y++) {
                unsigned short FAR* src = (unsigned short FAR*)spriteData;
                unsigned short FAR* dst = (unsigned short FAR*)destBuffer;
                for (x = 0; x < width; x++) {
                    unsigned short pixel = src[x];
                    if (pixel != key) { dst[x] = pixel; }
                }
                destBuffer += DisplayPitch;
                spriteData += (unsigned long)width * 2;
            }
            break;
        }
        default: {   // 32
            unsigned long key = sprite->transparentColor;
            for (y = 0; y < height; y++) {
                unsigned long FAR* src = (unsigned long FAR*)spriteData;
                unsigned long FAR* dst = (unsigned long FAR*)destBuffer;
                for (x = 0; x < width; x++) {
                    unsigned long pixel = src[x];
                    if (pixel != key) { dst[x] = pixel; }
                }
                destBuffer += DisplayPitch;
                spriteData += (unsigned long)width * 4;
            }
            break;
        }
    }
#else
    // mode-13h only: the book's original fixed 320-wide, 1 byte/pixel form
    unsigned char FAR* spriteData;
    unsigned char FAR* destBuffer;
    int x, y, width, height;
    unsigned char pixel;

    spriteData = sprite->frames[sprite->currFrame];

    width = sprite->width;
    height = sprite->height;

    destBuffer = buffer + (sprite->y << 8) + (sprite->y << 6) + sprite->x;

    if (transparent) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if ((pixel = spriteData[x])) {
                    destBuffer[x] = pixel;
                }
            }
            destBuffer += SCREEN_WIDTH;
            spriteData += width;
        }
    } else {
        for (y = 0; y < height; y++) {
            MEMCPY((void FAR*)destBuffer, (void FAR*)spriteData, width);
            destBuffer += SCREEN_WIDTH;
            spriteData += width;
        }
    }
#endif
}

void spriteUnder(SpritePtr sprite, unsigned char FAR* buffer) {
    // this function scans the background under a sprite so that when the sprite
    // is drawn the background isn't obliterated
#ifdef VBE_SUPPORT
    unsigned char FAR* backBuffer;
    int y, width, height;

    backBuffer = sprite->background;
    width = sprite->width;
    height = sprite->height;

    buffer = buffer + PITCH_OFFSET(sprite->y) + ((unsigned long)sprite->x << DisplayBppShift);

    for (y = 0; y < height; y++) {
        MEMCPY((void FAR*) backBuffer, (void FAR*)buffer, (unsigned long)width << DisplayBppShift);
        buffer += DisplayPitch;
        backBuffer += (unsigned long)width << DisplayBppShift;
    }
#else
    unsigned char FAR* backBuffer;
    int y, width, height;

    backBuffer = sprite->background;
    width = sprite->width;
    height = sprite->height;

    buffer = buffer + (sprite->y << 8) + (sprite->y << 6) + sprite->x;

    for (y = 0; y < height; y++) {
        MEMCPY((void FAR*) backBuffer, (void FAR*)buffer, width);
        buffer += SCREEN_WIDTH;
        backBuffer += width;
    }
#endif
}

void spriteErase(SpritePtr sprite, unsigned char FAR* buffer) {
    // replace the background that was behind the sprite
#ifdef VBE_SUPPORT
    unsigned char FAR* backBuffer;
    int y, width, height;

    backBuffer = sprite->background;
    width = sprite->width;
    height = sprite->height;

    buffer = buffer + PITCH_OFFSET(sprite->y) + ((unsigned long)sprite->x << DisplayBppShift);

    for (y = 0; y < height; y++) {
        MEMCPY((void FAR*)buffer, (void FAR*)backBuffer, (unsigned long)width << DisplayBppShift);
        buffer += DisplayPitch;
        backBuffer += (unsigned long)width << DisplayBppShift;
    }
#else
    unsigned char FAR* backBuffer;
    int y, width, height;

    backBuffer = sprite->background;
    width = sprite->width;
    height = sprite->height;

    buffer = buffer + (sprite->y << 8) + (sprite->y << 6) + sprite->x;

    for (y = 0; y < height; y++) {
        MEMCPY((void FAR*)buffer, (void FAR*)backBuffer, width);
        buffer += SCREEN_WIDTH;
        backBuffer += width;
    }
#endif
}

void spriteDrawClip(SpritePtr sprite, unsigned char FAR* buffer, int transparent) {
    // this function draws a sprite on the screen row by row very quickly
    // if the transparent flag is true then pixels will be drawn one by one
    // else a memcpy will be used to draw each line
    // this function also performs clipping. It will test if the sprite
    // is totally visible/invisible and will only draw the portions that are visible
#ifdef VBE_SUPPORT
    unsigned char FAR* spriteData;
    unsigned char FAR* destBuffer;
    int x, y, sx, sy, width,
        bitmapX = 0, bitmapY = 0, bitmapWidth = 0, bitmapHeight = 0;

    width = sprite->width;
    bitmapWidth = width;
    bitmapHeight = sprite->height;
    sx = sprite->x;
    sy = sprite->y;

    if (sx >= (int)DisplayWidth ||
        sy >= (int)DoubleBufferHeight ||
        (sx + width) <= 0 ||
        (sy + bitmapHeight) <= 0 ||
        !sprite->visible) {
        sprite->visible = 0;
        return;
    }

    if (sx < 0) {
        bitmapX = -sx;
        sx = 0;
        bitmapWidth -= bitmapX;
    } else if (sx + width >= (int)DisplayWidth) {
        bitmapX = 0;
        bitmapWidth = (int)DisplayWidth - sx;
    }

    if (sy < 0) {
        bitmapY = -sy;
        sy = 0;
        bitmapHeight -= bitmapY;
    } else if (sy + bitmapHeight >= (int)DoubleBufferHeight) {
        bitmapY = 0;
        bitmapHeight = (int)DoubleBufferHeight - sy;
    }

    destBuffer = buffer + PITCH_OFFSET(sy) + ((unsigned long)sx << DisplayBppShift);

    spriteData = sprite->frames[sprite->currFrame];
    if (spriteData == NULL) {
        sprite->visible = 0;
        return;
    }
    spriteData += ((unsigned long)bitmapY * width + bitmapX) << DisplayBppShift;

    if (!transparent) {
        for (y = 0; y < bitmapHeight; y++) {
            MEMCPY((void FAR*)destBuffer, (void FAR*)spriteData, (unsigned long)bitmapWidth << DisplayBppShift);
            destBuffer += DisplayPitch;
            spriteData += (unsigned long)width << DisplayBppShift;    // note this width is the
                                    // actual width of the entire bitmap NOT the visible portion
        }
    } else {
        switch (DisplayBpp) {
            case 8: {
                unsigned char key = (unsigned char)sprite->transparentColor;
                for (y = 0; y < bitmapHeight; y++) {
                    for (x = 0; x < bitmapWidth; x++) {
                        unsigned char pixel = spriteData[x];
                        if (pixel != key) { destBuffer[x] = pixel; }
                    }
                    destBuffer += DisplayPitch;
                    spriteData += width;
                }
                break;
            }
            case 16: {
                unsigned short key = (unsigned short)sprite->transparentColor;
                for (y = 0; y < bitmapHeight; y++) {
                    unsigned short FAR* src = (unsigned short FAR*)spriteData;
                    unsigned short FAR* dst = (unsigned short FAR*)destBuffer;
                    for (x = 0; x < bitmapWidth; x++) {
                        unsigned short pixel = src[x];
                        if (pixel != key) { dst[x] = pixel; }
                    }
                    destBuffer += DisplayPitch;
                    spriteData += (unsigned long)width * 2;
                }
                break;
            }
            default: {   // 32
                unsigned long key = sprite->transparentColor;
                for (y = 0; y < bitmapHeight; y++) {
                    unsigned long FAR* src = (unsigned long FAR*)spriteData;
                    unsigned long FAR* dst = (unsigned long FAR*)destBuffer;
                    for (x = 0; x < bitmapWidth; x++) {
                        unsigned long pixel = src[x];
                        if (pixel != key) { dst[x] = pixel; }
                    }
                    destBuffer += DisplayPitch;
                    spriteData += (unsigned long)width * 4;
                }
                break;
            }
        }
    }

    sprite->xClip = sx;
    sprite->yClip = sy;
    sprite->widthClip = bitmapWidth;
    sprite->heightClip = bitmapHeight;
    sprite->visible = 1;
#else
    // mode-13h only: the book's original fixed 320-wide, 1 byte/pixel form
    unsigned char FAR* spriteData;
    unsigned char FAR* destBuffer;
    int x, y, sx, sy, width,
        bitmapX = 0, bitmapY = 0, bitmapWidth = 0, bitmapHeight = 0;
    unsigned char pixel;

    width = sprite->width;
    bitmapWidth = width;
    bitmapHeight = sprite->height;
    sx = sprite->x;
    sy = sprite->y;

    if (sx >= (int)SCREEN_WIDTH ||
        sy >= (int)DoubleBufferHeight ||
        (sx + width) <= 0 ||
        (sy + bitmapHeight) <= 0 ||
        !sprite->visible) {
        sprite->visible = 0;
        return;
    }

    if (sx < 0) {
        bitmapX = -sx;
        sx = 0;
        bitmapWidth -= bitmapX;
    } else if (sx + width >= (int)SCREEN_WIDTH) {
        bitmapX = 0;
        bitmapWidth = (int)SCREEN_WIDTH - sx;
    }

    if (sy < 0) {
        bitmapY = -sy;
        sy = 0;
        bitmapHeight -= bitmapY;
    } else if (sy + bitmapHeight >= (int)DoubleBufferHeight) {
        bitmapY = 0;
        bitmapHeight = (int)DoubleBufferHeight - sy;
    }

    destBuffer = buffer + (sy << 8) + (sy << 6) + sx;

    spriteData = sprite->frames[sprite->currFrame] + (bitmapY * width) + bitmapX;

    if (transparent) {
        for (y = 0; y < bitmapHeight; y++) {
            for (x = 0; x < bitmapWidth; x++) {
                if ((pixel = spriteData[x])) {
                    destBuffer[x] = pixel;
                }
            }
            destBuffer += SCREEN_WIDTH;
            spriteData += width;
        }
    } else {
        for (y = 0; y < bitmapHeight; y++) {
            MEMCPY((void FAR*)destBuffer, (void FAR*)spriteData, bitmapWidth);
            destBuffer += SCREEN_WIDTH;
            spriteData += width;
        }
    }

    sprite->xClip = sx;
    sprite->yClip = sy;
    sprite->widthClip = bitmapWidth;
    sprite->heightClip = bitmapHeight;
    sprite->visible = 1;
#endif
}

void spriteUnderClip(SpritePtr sprite, unsigned char FAR* buffer) {
    // this function scans the background under a sprite, but only those
    // portions that are visible
#ifdef VBE_SUPPORT
    unsigned char FAR* backBuffer;
    unsigned char FAR* sourceBuffer;
    int y, sx, sy, width,
        bitmapWidth = 0, bitmapHeight = 0;

    width = sprite->width;
    bitmapWidth = width;
    bitmapHeight = sprite->height;
    sx = sprite->x;
    sy = sprite->y;

    if (sx >= (int)DisplayWidth ||
        sy >= (int)DoubleBufferHeight ||
        (sx + width) <= 0 ||
        (sy + bitmapHeight) <= 0) {
        sprite->visible = 0;
        return;
    }

    if (sx < 0) {
        bitmapWidth += sx;
        sx = 0;
    } else if (sx + width >= (int)DisplayWidth) {
        bitmapWidth = (int)DisplayWidth - sx;
    }

    if (sy < 0) {
        bitmapHeight += sy;
        sy = 0;
    } else if (sy + bitmapHeight >= (int)DoubleBufferHeight) {
        bitmapHeight = (int)DoubleBufferHeight - sy;
    }

    sourceBuffer = buffer + PITCH_OFFSET(sy) + ((unsigned long)sx << DisplayBppShift);
    backBuffer = sprite->background;

    for (y = 0; y < bitmapHeight; y++) {
        MEMCPY((void FAR*)backBuffer, (void FAR*)sourceBuffer, (unsigned long)bitmapWidth << DisplayBppShift);
        sourceBuffer += DisplayPitch;
        backBuffer += (unsigned long)width << DisplayBppShift;    // note this width is the
                                // actual width of the entire bitmap NOT the visible portion
    }

    sprite->xClip = sx;
    sprite->yClip = sy;
    sprite->widthClip = bitmapWidth;
    sprite->heightClip = bitmapHeight;
    sprite->visible = 1;
#else
    unsigned char FAR* backBuffer;
    unsigned char FAR* sourceBuffer;
    int y, sx, sy, width,
        bitmapWidth = 0, bitmapHeight = 0;

    width = sprite->width;
    bitmapWidth = width;
    bitmapHeight = sprite->height;
    sx = sprite->x;
    sy = sprite->y;

    if (sx >= (int)SCREEN_WIDTH ||
        sy >= (int)DoubleBufferHeight ||
        (sx + width) <= 0 ||
        (sy + bitmapHeight) <= 0) {
        sprite->visible = 0;
        return;
    }

    if (sx < 0) {
        bitmapWidth += sx;
        sx = 0;
    } else if (sx + width >= (int)SCREEN_WIDTH) {
        bitmapWidth = (int)SCREEN_WIDTH - sx;
    }

    if (sy < 0) {
        bitmapHeight += sy;
        sy = 0;
    } else if (sy + bitmapHeight >= (int)DoubleBufferHeight) {
        bitmapHeight = (int)DoubleBufferHeight - sy;
    }

    sourceBuffer = buffer + (sy << 8) + (sy << 6) + sx;
    backBuffer = sprite->background;

    for (y = 0; y < bitmapHeight; y++) {
        MEMCPY((void FAR*)backBuffer, (void FAR*)sourceBuffer, bitmapWidth);
        sourceBuffer += SCREEN_WIDTH;
        backBuffer += width;
    }

    sprite->xClip = sx;
    sprite->yClip = sy;
    sprite->widthClip = bitmapWidth;
    sprite->heightClip = bitmapHeight;
    sprite->visible = 1;
#endif
}

void spriteEraseClip(SpritePtr sprite, unsigned char FAR* buffer) {
    // replace the background that was behind the sprite
#ifdef VBE_SUPPORT
    unsigned char FAR* backBuffer;
    int y, width, bitmapHeight, bitmapWidth;

    if (!sprite->visible) {
        return;
    }

    backBuffer = sprite->background;
    bitmapWidth = sprite->widthClip;
    bitmapHeight = sprite->heightClip;
    width = sprite->width;

    buffer = buffer + PITCH_OFFSET(sprite->yClip) + ((unsigned long)sprite->xClip << DisplayBppShift);

    for (y = 0; y < bitmapHeight; y++) {
        MEMCPY((void FAR*)buffer, (void FAR*)backBuffer, (unsigned long)bitmapWidth << DisplayBppShift);
        buffer += DisplayPitch;
        backBuffer += (unsigned long)width << DisplayBppShift;
    }
#else
    unsigned char FAR* backBuffer;
    int y, width, bitmapHeight, bitmapWidth;

    if (!sprite->visible) {
        return;
    }

    backBuffer = sprite->background;
    bitmapWidth = sprite->widthClip;
    bitmapHeight = sprite->heightClip;
    width = sprite->width;

    buffer = buffer + (sprite->yClip << 8) + (sprite->yClip << 6) + sprite->xClip;

    for (y = 0; y < bitmapHeight; y++) {
        MEMCPY((void FAR*)buffer, (void FAR*)backBuffer, bitmapWidth);
        buffer += SCREEN_WIDTH;
        backBuffer += width;
    }
#endif
}

#ifdef VBE_SUPPORT
void pcxGetSpriteTinted(
    PcxPicturePtr image,
    SpritePtr sprite,
    int spriteFrame,
    int cellX, int cellY,
    const unsigned long* tintKeys, int numTintKeys) {

    // like pcxGetSprite, but also marks tinted pixels: a source pixel equal
    // to tintKeys[i] is recorded in sprite->tintMask[spriteFrame] as region
    // (i+1) instead of being copied into the frame verbatim, so
    // spriteDrawTinted can substitute a live color for it at draw time
    int width = sprite->width, height = sprite->height, row, col, key, bytespp;
    int xOff, yOff, pcxWidth;
    unsigned char FAR* mask;

    pcxGetSprite(image, sprite, spriteFrame, cellX, cellY);
    if (sprite->frames[spriteFrame] == NULL) {
        return;
    }

    bytespp = DisplayBpp / 8;
    pcxWidth = image->header.width - image->header.x + 1;   // see pcxGetSprite
    xOff = (width + 1) * cellX + 1;
    yOff = (height + 1) * cellY + 1;

    mask = (unsigned char FAR*)MALLOC((unsigned long)width * height);
    sprite->tintMask[spriteFrame] = mask;
    if (mask == NULL) {
        return;
    }
    MEMSET(mask, 0, (unsigned long)width * height);

    for (row = 0; row < height; row++) {
        unsigned char FAR* src = &image->buffer[((unsigned long)(yOff + row) * pcxWidth + xOff) << DisplayBppShift];
        for (col = 0; col < width; col++) {
            unsigned long px;
            switch (bytespp) {
                case 1:  px = src[col]; break;
                case 2:  px = ((unsigned short FAR*)src)[col]; break;
                default: px = ((unsigned long FAR*)src)[col]; break;   // 4 (32bpp)
            }
            for (key = 0; key < numTintKeys; key++) {
                if (px == tintKeys[key]) {
                    mask[(unsigned long)row * width + col] = (unsigned char)(key + 1);
                    break;
                }
            }
        }
    }
}

void spriteDrawTinted(SpritePtr sprite, unsigned char FAR* buffer, int transparent,
                      const unsigned long* tintColors, int numTintColors) {
    // like spriteDrawClip, but a pixel marked region N (1-based) in
    // sprite->tintMask[currFrame] draws as tintColors[N-1] instead of its
    // own stored pixel - one or more live-recolored regions within an
    // otherwise static frame (e.g. blazerx's shield/engine-glow effect).
    // Always clips (no unclamped fast-path sibling, unlike spriteDraw/
    // spriteDrawClip): every tinted sprite in blazerx can end up off-screen
    // (e.g. the remote ship, positioned from world coordinates relative to
    // the player's camera), so clipping unconditionally here is the only
    // safe default - the extra bounds check is negligible next to the
    // per-pixel mask lookup this function already does.
    unsigned char FAR* spriteData;
    unsigned char FAR* mask;
    unsigned char FAR* destBuffer;
    int x, y, sx, sy, width,
        bitmapX = 0, bitmapY = 0, bitmapWidth = 0, bitmapHeight = 0;

    mask = sprite->tintMask[sprite->currFrame];
    if (mask == NULL) {           // no tint region recorded: a plain draw
        spriteDrawClip(sprite, buffer, transparent);
        return;
    }

    width = sprite->width;
    bitmapWidth = width;
    bitmapHeight = sprite->height;
    sx = sprite->x;
    sy = sprite->y;

    if (sx >= (int)DisplayWidth ||
        sy >= (int)DoubleBufferHeight ||
        (sx + width) <= 0 ||
        (sy + bitmapHeight) <= 0 ||
        !sprite->visible) {
        sprite->visible = 0;
        return;
    }

    if (sx < 0) {
        bitmapX = -sx;
        sx = 0;
        bitmapWidth -= bitmapX;
    } else if (sx + width >= (int)DisplayWidth) {
        bitmapX = 0;
        bitmapWidth = (int)DisplayWidth - sx;
    }

    if (sy < 0) {
        bitmapY = -sy;
        sy = 0;
        bitmapHeight -= bitmapY;
    } else if (sy + bitmapHeight >= (int)DoubleBufferHeight) {
        bitmapY = 0;
        bitmapHeight = (int)DoubleBufferHeight - sy;
    }

    destBuffer = buffer + PITCH_OFFSET(sy) + ((unsigned long)sx << DisplayBppShift);

    spriteData = sprite->frames[sprite->currFrame];
    if (spriteData == NULL) {
        sprite->visible = 0;
        return;
    }
    spriteData += ((unsigned long)bitmapY * width + bitmapX) << DisplayBppShift;
    mask       += (unsigned long)bitmapY * width + bitmapX;

    switch (DisplayBpp) {
        case 8: {
            unsigned char key = (unsigned char)sprite->transparentColor;
            for (y = 0; y < bitmapHeight; y++) {
                unsigned char FAR* dst = destBuffer;
                unsigned char FAR* src = spriteData;
                unsigned char FAR* m   = mask;
                for (x = 0; x < bitmapWidth; x++) {
                    if (m[x] && m[x] <= numTintColors)   { dst[x] = (unsigned char)tintColors[m[x] - 1]; continue; }
                    if (!transparent || src[x] != key)   { dst[x] = src[x]; }
                }
                destBuffer += DisplayPitch;
                spriteData += width;
                mask += width;
            }
            break;
        }
        case 16: {
            unsigned short key = (unsigned short)sprite->transparentColor;
            for (y = 0; y < bitmapHeight; y++) {
                unsigned short FAR* dst = (unsigned short FAR*)destBuffer;
                unsigned short FAR* src = (unsigned short FAR*)spriteData;
                unsigned char FAR*  m   = mask;
                for (x = 0; x < bitmapWidth; x++) {
                    if (m[x] && m[x] <= numTintColors)   { dst[x] = (unsigned short)tintColors[m[x] - 1]; continue; }
                    if (!transparent || src[x] != key)   { dst[x] = src[x]; }
                }
                destBuffer += DisplayPitch;
                spriteData += (unsigned long)width * 2;
                mask += width;
            }
            break;
        }
        default: {   // 32
            unsigned long key = sprite->transparentColor;
            for (y = 0; y < bitmapHeight; y++) {
                unsigned long FAR* dst = (unsigned long FAR*)destBuffer;
                unsigned long FAR* src = (unsigned long FAR*)spriteData;
                unsigned char FAR* m   = mask;
                for (x = 0; x < bitmapWidth; x++) {
                    if (m[x] && m[x] <= numTintColors)   { dst[x] = tintColors[m[x] - 1]; continue; }
                    if (!transparent || src[x] != key)   { dst[x] = src[x]; }
                }
                destBuffer += DisplayPitch;
                spriteData += (unsigned long)width * 4;
                mask += width;
            }
            break;
        }
    }

    sprite->xClip = sx;
    sprite->yClip = sy;
    sprite->widthClip = bitmapWidth;
    sprite->heightClip = bitmapHeight;
    sprite->visible = 1;
}
#endif

void pcxCopyToBuffer(PcxPicturePtr image, unsigned char FAR* buffer) {
#ifdef VBE_SUPPORT
    // this function is used to copy the data in the PCX buffer to another
    // buffer, usually the double buffer - image->buffer is tightly packed
    // at DisplayWidth (see pcxInit), but a VESA mode's DisplayPitch can pad
    // each scanline past DisplayWidth*bytespp, so a single contiguous copy
    // isn't safe in general - copy row by row instead, same shape as
    // pcxShowBuffer.
    //
    // Row count is DoubleBufferHeight, NOT DisplayHeight: the destination
    // buffer isn't always full-screen-tall (e.g. krk.c's createDoubleBuffer
    // (129) - a partial-height 3D viewport, with a fixed HUD below it) -
    // DoubleBufferHeight always tracks whatever createDoubleBuffer actually
    // allocated (and defaults to the full screen height if it was never
    // called, so a caller that copies straight to VideoBuffer still works).
    int row;
    unsigned long rowBytes = (unsigned long)DisplayWidth << DisplayBppShift;
    unsigned char FAR* src = image->buffer;
    unsigned char FAR* dst = buffer;

    for (row = 0; row < (int)DoubleBufferHeight; row++) {
        MEMCPY((void FAR*)dst, (void FAR*)src, rowBytes);
        dst += DisplayPitch;
        src += rowBytes;
    }
#else
    // mode-13h only: the book's original single contiguous copy - safe
    // because DoubleBufferSize already tracks whatever createDoubleBuffer
    // actually allocated, and there's no pitch padding at a fixed 320-wide
    // mode for it to disagree with
    fwordcpy((void FAR*)buffer, (void FAR*)image->buffer, DoubleBufferSize);
#endif
}

void fwordcpy(void FAR* destination, void FAR* source, int numWords) {
    // this function is similar to fmemcpy except that it moves data in words
    // it is about 25% faster than memcpy which uses bytes
#ifdef DOS_32_BIT
    _asm {
        mov edi, destination        ; load destination flat pointer
        mov esi, source             ; load source flat pointer
        mov ecx, numWords           ; number of words
        shr ecx, 1                  ; convert word count to dword count
        rep movsd                   ; copy dwords (4 bytes at a time - 2x faster!)

        mov ecx, numWords           ; handle odd word if numWords was odd
        and ecx, 1                  ; check if odd
        rep movsw                   ; copy remaining word if any
    }
#else
    _asm {
        push ds             ; need to save segment registers i.e. ds
        les di,destination  ; point es:di to destination of memory move
        lds si,source       ; point ds:si to source of memory move
        mov cx,numWords     ; move into cx the number of words to be moved
        rep movsw           ; let the processor do the memory move
        pop ds              ; restore the ds segment register
    }
#endif
}

int layerCreate(LayerPtr destLayer, int width, int height) {
    // this function can be used to allocate the memory needed for a layer
    // the width must be divisible by two.
#ifdef VBE_SUPPORT
    // layerBuild/layerDraw copy at the runtime DisplayBpp - the fixed *2
    // below only covers up to 16bpp, so this must scale the same way or a
    // 32bpp layer overflows the buffer they read/write into
    if ((destLayer->buffer = (unsigned char FAR*)MALLOC((unsigned long)width * height * (DisplayBpp / 8))) == NULL) {
        return 0;
    } else {
        destLayer->width = width;
        destLayer->height = height;
        return 1;
    }
#else
    if ((destLayer->buffer = (unsigned char FAR*)MALLOC(width * height * 2)) == NULL) {
        return 0;
    } else {
        destLayer->width = width;
        destLayer->height = height;
        return 1;
    }
#endif
}

void layerDelete(LayerPtr layer) {
    // this function deletes the memory used by a layer
    if (layer->buffer) {
        FREE(layer->buffer);
    }
}

void layerBuild(
    LayerPtr destLayer,
    int destX,
    int destY,
    unsigned char FAR* sourceBuffer,
    int sourceX,
    int sourceY,
    int width,
    int height) {

    // this function is used to build up the layer out of smaller pieces
    // this allows a layer to be very long, tall, etc.
#ifdef VBE_SUPPORT
    // also the source data buffer must use the current mode's pitch
    // (VideoBuffer/DoubleBuffer do)
    int y, layerWidth;
    unsigned char FAR* sourceData;
    unsigned char FAR* layerBuffer;

    layerWidth = destLayer->width;

    layerBuffer = destLayer->buffer + (((unsigned long)layerWidth * destY + destX) << DisplayBppShift);
    sourceData = sourceBuffer + PITCH_OFFSET(sourceY) + ((unsigned long)sourceX << DisplayBppShift);

    for (y = 0; y < height; y++) {
        MEMCPY((void FAR*) layerBuffer, (void FAR*)sourceData, (unsigned long)width << DisplayBppShift);
        sourceData += DisplayPitch;
        layerBuffer += (unsigned long)layerWidth << DisplayBppShift;
    }
#else
    // mode-13h only: also the source data buffer must be constructed such
    // that there are 320 bytes per row
    int y, layerWidth;
    unsigned char FAR* sourceData;
    unsigned char FAR* layerBuffer;

    layerWidth = destLayer->width;

    layerBuffer = destLayer->buffer + layerWidth * destY + destX;
    sourceData = sourceBuffer + (sourceY << 8) + (sourceY << 6) + sourceX;

    for (y = 0; y < height; y++) {
        MEMCPY((void FAR*) layerBuffer, (void FAR*)sourceData, width);
        sourceData += SCREEN_WIDTH;
        layerBuffer += layerWidth;
    }
#endif
}

void layerDraw(
    LayerPtr sourceLayer,
    int sourceX,
    int sourceY,
    unsigned char FAR* destBuffer,
    int destY,
    int destHeight,
    int transparent) {

    // this function will map down a section of the layer onto the destination
    // buffer at the desired location. Also, the function will always wrap
    // around the layer
#ifdef VBE_SUPPORT
    int x, y, layerWidth, screenWidth, rightWidth, leftWidth;
    unsigned char FAR* layerBufferL;
    unsigned char FAR* destBufferL;
    unsigned char FAR* layerBufferR;
    unsigned char FAR* destBufferR;

    layerWidth  = sourceLayer->width;
    screenWidth = DisplayWidth;
    destBufferL  = destBuffer + PITCH_OFFSET(destY);
    layerBufferL = sourceLayer->buffer + (((unsigned long)layerWidth * sourceY + sourceX) << DisplayBppShift);
    destBufferR  = 0;
    layerBufferR = 0;

    if ((layerWidth - sourceX) - screenWidth >= 0) {
        leftWidth = screenWidth;
        rightWidth = 0;
    } else {
        leftWidth = layerWidth - sourceX;
        rightWidth = screenWidth - leftWidth;
        destBufferR = destBufferL + ((unsigned long)leftWidth << DisplayBppShift);
        layerBufferR = layerBufferL - ((unsigned long)sourceX << DisplayBppShift);
    }

    if (!transparent) {
        for (y = 0; y < destHeight; y++) {
            MEMCPY((void FAR*)destBufferL, (void FAR*)layerBufferL, (unsigned long)leftWidth << DisplayBppShift);
            destBufferL += DisplayPitch;
            layerBufferL += (unsigned long)layerWidth << DisplayBppShift;
        }

        if (rightWidth) {
            for (y = 0; y < destHeight; y++) {
                MEMCPY((void FAR*)destBufferR, (void FAR*)layerBufferR, (unsigned long)rightWidth << DisplayBppShift);
                destBufferR += DisplayPitch;
                layerBufferR += (unsigned long)layerWidth << DisplayBppShift;
            }
        }
        return;
    }

    switch (DisplayBpp) {
        case 8:
            for (y = 0; y < destHeight; y++) {
                for (x = 0; x < leftWidth; x++) {
                    unsigned char pixel = layerBufferL[x];
                    if (pixel) { destBufferL[x] = pixel; }
                }
                destBufferL += DisplayPitch;
                layerBufferL += layerWidth;
            }
            if (rightWidth) {
                for (y = 0; y < destHeight; y++) {
                    for (x = 0; x < rightWidth; x++) {
                        unsigned char pixel = layerBufferR[x];
                        if (pixel) { destBufferR[x] = pixel; }
                    }
                    destBufferR += DisplayPitch;
                    layerBufferR += layerWidth;
                }
            }
            break;
        case 16:
            for (y = 0; y < destHeight; y++) {
                unsigned short FAR* src = (unsigned short FAR*)layerBufferL;
                unsigned short FAR* dst = (unsigned short FAR*)destBufferL;
                for (x = 0; x < leftWidth; x++) {
                    unsigned short pixel = src[x];
                    if (pixel) { dst[x] = pixel; }
                }
                destBufferL += DisplayPitch;
                layerBufferL += (unsigned long)layerWidth * 2;
            }
            if (rightWidth) {
                for (y = 0; y < destHeight; y++) {
                    unsigned short FAR* src = (unsigned short FAR*)layerBufferR;
                    unsigned short FAR* dst = (unsigned short FAR*)destBufferR;
                    for (x = 0; x < rightWidth; x++) {
                        unsigned short pixel = src[x];
                        if (pixel) { dst[x] = pixel; }
                    }
                    destBufferR += DisplayPitch;
                    layerBufferR += (unsigned long)layerWidth * 2;
                }
            }
            break;
        default:   // 32
            for (y = 0; y < destHeight; y++) {
                unsigned long FAR* src = (unsigned long FAR*)layerBufferL;
                unsigned long FAR* dst = (unsigned long FAR*)destBufferL;
                for (x = 0; x < leftWidth; x++) {
                    unsigned long pixel = src[x];
                    if (pixel) { dst[x] = pixel; }
                }
                destBufferL += DisplayPitch;
                layerBufferL += (unsigned long)layerWidth * 4;
            }
            if (rightWidth) {
                for (y = 0; y < destHeight; y++) {
                    unsigned long FAR* src = (unsigned long FAR*)layerBufferR;
                    unsigned long FAR* dst = (unsigned long FAR*)destBufferR;
                    for (x = 0; x < rightWidth; x++) {
                        unsigned long pixel = src[x];
                        if (pixel) { dst[x] = pixel; }
                    }
                    destBufferR += DisplayPitch;
                    layerBufferR += (unsigned long)layerWidth * 4;
                }
            }
            break;
    }
#else
    // mode-13h only: the book's original fixed 320-wide form - destination
    // buffer width is always assumed to be 320 bytes
    int x, y, layerWidth, rightWidth, leftWidth;
    unsigned char FAR* layerBufferL;
    unsigned char FAR* destBufferL;
    unsigned char FAR* layerBufferR;
    unsigned char FAR* destBufferR;
    unsigned char pixel;

    layerWidth = sourceLayer->width;
    destBufferL = destBuffer + (destY << 8) + (destY << 6);
    layerBufferL = sourceLayer->buffer + layerWidth * sourceY + sourceX;

    if (((layerWidth - sourceX) - (int)SCREEN_WIDTH) >= 0) {
        leftWidth = SCREEN_WIDTH;
        rightWidth = 0;
    } else {
        leftWidth = layerWidth - sourceX;
        rightWidth = SCREEN_WIDTH - leftWidth;
        destBufferR = destBufferL + leftWidth;
        layerBufferR = layerBufferL - sourceX;
    }

    if (transparent) {
        for (y = 0; y < destHeight; y++) {
            for (x = 0; x < leftWidth; x++) {
                if ((pixel = layerBufferL[x])) {
                    destBufferL[x] = pixel;
                }
            }
            destBufferL += SCREEN_WIDTH;
            layerBufferL += layerWidth;
        }

        if (rightWidth) {
            for (y = 0; y < destHeight; y++) {
                for (x = 0; x < rightWidth; x++) {
                    if ((pixel = layerBufferR[x])) {
                        destBufferR[x] = pixel;
                    }
                }
                destBufferR += SCREEN_WIDTH;
                layerBufferR += layerWidth;
            }
        }
    } else {
        for (y = 0; y < destHeight; y++) {
            MEMCPY((void FAR*)destBufferL, (void FAR*)layerBufferL, leftWidth);
            destBufferL += SCREEN_WIDTH;
            layerBufferL += layerWidth;
        }

        if (rightWidth) {
            for (y = 0; y < destHeight; y++) {
                MEMCPY((void FAR*)destBufferR, (void FAR*)layerBufferR, rightWidth);
                destBufferR += SCREEN_WIDTH;
                layerBufferR += layerWidth;
            }
        }
    }
#endif
}

void setWorkingPageModeZ(int page) {
    // this function sets the page that all mode Z functions will update when called
    if (page == PAGE_0) {
        VideoBuffer = Page0Buffer;
    } else {
        VideoBuffer = Page1Buffer;
    }
}

void setVisualPageModeZ(int page) {
    // this function sets the visual page that will be displayed by the VGA
    if (page == PAGE_0) {
        // re-program the start address registers in the CRT controller
        // to point at page 0 @ 0xA000:0000

        // first low byte of address
        outp(CRT_CONTROLLER, CRT_ADDR_LOW);
        outp(CRT_CONTROLLER + 1, 0x00);

        // now high byte
        outp(CRT_CONTROLLER, CRT_ADDR_HI);
        outp(CRT_CONTROLLER + 1, 0x00);
    } else if (page == PAGE_1) {
        // re-program the start address registers in the CRT controller
        // to point at page 1 @ 0xA000:8000

        // first low byte of address
        outp(CRT_CONTROLLER, CRT_ADDR_LOW);
        outp(CRT_CONTROLLER + 1, 0x00);

        // now high byte
        outp(CRT_CONTROLLER, CRT_ADDR_HI);
        outp(CRT_CONTROLLER + 1, 0x80);
    }
}
