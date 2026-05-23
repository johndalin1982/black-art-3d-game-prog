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

void timeDelay(int clicks) {
    // this function uses the internal timer to delay a number of clock ticks
#ifdef DOS_32_BIT
    long FAR* clock = (long FAR*)0x0000046C;
#else
    long FAR* clock = (long FAR*)0x0000046CL;
#endif

    unsigned long now;

    // get current time
    now = *clock;

    // wait for number of click to pass
    while (abs(*clock - now) < clicks) {}
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
}

void fillScreen(int color) {
    // this function will fill the entire screen with the sent color
    // use the inline assembler for speed
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
}

void writePixel(int x, int y, int color) {
    // plots the pixel in the desired color a little quicker using binary shifting
    // to accomplish the multiplications
    // use the fact that 320*y = 256*y + 64*y = y<<8 + y<<6
    VideoBuffer[((y << 8) + (y << 6)) + x] = (unsigned char)color;
}

int readPixel(int x, int y) {
    // this function reads a pixel from the screen buffer
    // use the fact that 320*y = 256*y + 64*y = y<<8 + y <<6
    return (int)VideoBuffer[((y << 8) + (y << 6)) + x];
}

void lineH(int x1, int x2, int y, int color) {
    // draw a horizontal line using the memset function
    // this function does not do clipping hence x1,x2 and y must all be within
    // the bounds of the screen
    int temp;   // used for temporary storage during endpoint swap

    // sort x1 and x2, so that x2 > x1
    if (x1 > x2) {
        temp = x1;
        x1 = x2;
        x2 = temp;
    }

    // draw the row of pixels
    MEMSET((char FAR*)(VideoBuffer + ((y << 8) + (y << 6)) + x1), (unsigned char) color, x2 - x1 + 1);
}

void lineV(int y1, int y2, int x, int color) {
    // draw a vertical line, note that a memset function can no longer be
    // used since the pixel addresses are no longer contiguous in memory
    // note that the end points of the line must be on the screen
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
}

void drawRectangle(int x1, int y1, int x2, int y2, int color) {
    // this function will draw a rectangle from (x1,y1) - (x2,y2)
    // it is assumed that each endpoint is within the screen boundaries
    // and (x1,y1) is the upper left hand corner and (x2,y2) is the lower
    // right hand corner
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
    // this function is used to print a character on the screen. It uses the
    // internal 8x8 character set to do this. Note that each character is
    // 8 bytes where each byte represents the 8 pixels that make up the row
    // of pixels
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
}

void printString(int x, int y, int color, char* string, int transparent) {
    // this function prints an entire string on the screen with fixed spacing
    // between each character by calling the printChar() function
    int currentX = x;
    char* currentChar = string;

    // print the string a character at a time
    while (*currentChar != '\0') {
        printChar(currentX, y, *currentChar, color, transparent);
        currentX += ROM_CHAR_WIDTH;
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

#ifdef DOS_32_BIT
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
