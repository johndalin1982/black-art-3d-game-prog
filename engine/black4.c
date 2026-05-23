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

// size of double buffer in WORDS
unsigned int DoubleBufferSize = SCREEN_WIDTH * SCREEN_HEIGHT / 2;

void screenTransition(int effect) {
    // this function can be called to perform a myriad of screen transitions
    // to the video buffer, note I have left one for you to create!
    int palReg;
    long index;
    RgbColor color;

    // test which screen effect is being selected
    switch (effect) {
        case SCREEN_DARKNESS:
        {
            // fade to black
            for (index = 0; index < 20; index++) {
                
                // loop thru all palette registers
                for (palReg = 1; palReg < 255; palReg++) {

                    // get the next color to fade
                    readColorReg(palReg, &color);

                    // test if this color register is already black
                    if (color.red > 4) {
                        color.red -= 3;
                    } else {
                        color.red = 0;
                    }

                    if (color.green > 4) {
                        color.green -= 3;
                    } else {
                        color.green = 0;
                    }

                    if (color.blue > 4) {
                        color.blue -= 3;
                    } else {
                        color.blue = 0;
                    }

                    // set the color to a diminished intensity
                    writeColorReg(palReg, &color);
                }

                // wait a bit
                timeDelay(1);
            }

            break;
        }

        case SCREEN_WHITENESS:
        {
            // fade to white
            for (index = 0; index < 20; index ++) {

                // loop thru all palette registers
                for (palReg = 0; palReg < 255; palReg++) {
                    
                    // get the color to fade
                    readColorReg(palReg, &color);
                    color.red += 4;

                    if (color.red > 63) {
                        color.red = 63;
                    }

                    color.green += 4;

                    if (color.green > 63) {
                        color.green = 63;
                    }

                    color.blue += 4;

                    if (color.blue > 63) {
                        color.blue = 63;
                    }

                    // set the color to a brighter intensity
                    writeColorReg(palReg, &color);
                }

                // wait a bit
                timeDelay(1);
            }

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
            for (index = 0; index < 160; index += 2) {

                // use this as a 1/70th of second time delay
                waitForVerticalRetrace();

                // draw two vertical lines at opposite ends of the screen
                lineV(0, 199, 319 - index, 0);
                lineV(0, 199, index, 0);
                lineV(0, 199, 319 - (index + 1), 0);
                lineV(0, 199, index + 1, 0);
            }

            break;
        }

        case SCREEN_SWIPE_Y:
        {
            // do a screen swipe from top to bottom, bottom to top
            for (index = 0; index < 100; index += 2) {

                // use this as a 1/70th of second time delay
                waitForVerticalRetrace();

                // draw two horizontal lines at opposite ends of the screen
                lineH(0, 319, 199 - index, 0);
                lineH(0, 319, index, 0);
                lineH(0, 319, 199 - (index + 1), 0);
                lineH(0, 319, index + 1, 0);
            }

            break;
        }

        case SCREEN_DISSOLVE:
        {
            // dissolve the screen by plotting zillions of little black dots
            for (index = 0; index <= 300000; index++) {
                writePixel(rand() % 320, rand() % 200, 0);
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
    
    while(inp(VGA_INPUT_STATUS_1) & VGA_VRETRACE_MASK) {
        // do nothing, vga is already in retrace
    }

    // now wait for start of vertical retrace and exit
    while(!(inp(VGA_INPUT_STATUS_1) & VGA_VRETRACE_MASK)) {
        // do nothing, wait for start of retrace
    }

    // at this point a vertical retrace is occuring, so return back to caller
}

void printCharDb(int xc, int yc, char c, int color, int transparent) {
    // this function is used to print a character on the double buffer. It uses the
    // internal 8x8 character set to do this. Note that each character is
    // 8 bytes where each byte represents the 8 pixels that make up the row
    // of pixels
    int offset,         // offset into video memory
        x,
        y;
    unsigned char FAR* workChar;    // pointer to character being printed
    unsigned char bitMask;          // bitmask used to extract proper bit

    // compute starting offset in rom character lookup table
    // multiply the character by 8 and add the result to the starting address
    // of the ROM character set
    workChar = RomCharSet + c * ROM_CHAR_HEIGHT;

    // compute offset of character in double buffer, use shifting to multiply
    offset = (yc << 8) + (yc << 6) + xc;

    // draw the character row by row
    for (y = 0; y < ROM_CHAR_HEIGHT; y++) {
        // reset bit mask
        bitMask = 0x80;

        // draw each pixel of this row
        for (x = 0; x < ROM_CHAR_WIDTH; x++) {
            // test for transparent pixel i.e. 0, if not transparent then draw
            if ((*workChar & bitMask)) {
                DoubleBuffer[offset + x] = (unsigned char)color;
            } else if (!transparent) {
                DoubleBuffer[offset + x] = 0;   // make black part opaque
            }

            // shift bit mask
            bitMask = bitMask >> 1;
        }

        // move to next line in double buffer and in rom character data area
        offset += MODE13_WIDTH;
        workChar++;
    }
}

void printStringDb(int x, int y, int color, char* string, int transparent) {
    // this function prints an entire string into the double buffer with fixed
    // spacing between each character by calling the printCharDb() function
    int currentX = x;
    char* currentChar = string;

    // print the string a character at a time
    while (*currentChar != '\0') {
        printCharDb(currentX, y, *currentChar, color, transparent);
        currentX += ROM_CHAR_WIDTH;
        currentChar++;
    }
}

void writePixelDb(int x, int y, int color) {
    // plots the pixel in the desired color to the double buffer
    // to accomplish the multiplications
    // use the fact that 320*y = 256*y + 64*y = y<<8 + y<<6
    DoubleBuffer[(y << 8) + (y << 6) + x] = (unsigned char)color;
}

int readPixelDb(int x, int y) {
    // this function reads a pixel from the screen buffer
    // use the fact that 320*y = 256*y + 64*y = y<<8 + y <<6
    return (int)DoubleBuffer[((y << 8) + (y << 6)) + x];
}

int createDoubleBuffer(int numLines) {
    // allocate enough memory to hold the double buffer
    if ((DoubleBuffer = (unsigned char FAR*)MALLOC(SCREEN_WIDTH * (numLines + 1))) == NULL) {
        printf("\nCouldn't allocate double buffer.");
        return 0;
    }

    // set the height of the buffer and compute its size
    DoubleBufferHeight = numLines;
    DoubleBufferSize = SCREEN_WIDTH * numLines / 2;

    // fill the buffer with black
    MEMSET(DoubleBuffer, 0, SCREEN_WIDTH * numLines);

    // everything was ok
    return 1;
}

void fillDoubleBuffer(int color) {
    // this function fills in the double buffer with the sent color a WORD at
    // a time
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
        
        ; handle odd word if size was odd
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
}

void displayDoubleBuffer(unsigned char FAR* buffer, int y) {
    // this function copies the double buffer into the video buffer at the
    // starting y location
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
    int x, y, width, height;
    unsigned char FAR* bitmapData;  // pointer to bitmap buffer
    unsigned char FAR* destBuffer;  // pointer to destination buffer
    unsigned char pixel;            // current pixel value being processed

    // compute offset of bitmap in destination buffer. Note: all video or
    // double buffers must be 320 bytes wide!
    destBuffer = destination + (image->y << 8) + (image->y << 6) + image->x;

    // create aliases to variables so the structure doesn't need to be
    // dereferenced continually
    height = image->height;
    width = image->width;
    bitmapData = image->buffer;

    // test if transparency is on or off
    if (transparent) {
        // use version that will draw a transparent bitmap (slightly slower)
        // draw each line of the bitmap
        for (y = 0; y < height; y++) {

            // copy the next row into the destination buffer
            for (x = 0; x < width; x++) {
                
                // test for transparent pixel i.e. 0, if not transparent then draw
                if (pixel = bitmapData[x]) {
                    destBuffer[x] = pixel;
                }
            }

            // move to next line in double buffer and in bitmap buffer
            destBuffer += SCREEN_WIDTH;
            bitmapData += width;
        }
    } else {
        // draw each line of the bitmap, note how each pixel doesn't need to be
        // tested for transparency hence a memcpy can be used (very fast!)
        for (y = 0; y < height; y++) {

            // copy the next row into the destination buffer using memcpy for speed
            MEMCPY((void FAR*)destBuffer, (void FAR*)bitmapData, width);

            // move to next line in destination buffer and in bitmap buffer
            destBuffer += SCREEN_WIDTH;
            bitmapData += width;
        }
    }
}

void bitmapGet(BitmapPtr image, unsigned char FAR* source) {
    // this function will scan a bitmap from the source buffer
    // could be a double buffer, video buffer or any buffer with a
    // logical row width of 320 bytes
    unsigned int sourceOff,             // offsets into destination and source buffers
                 bitmapOff;
    int y, width, height;
    unsigned char FAR* bitmapData;      // pointer to bitmap buffer

    // compute offset of bitmap in source buffer. Note: all video or double
    // buffers must be 320 bytes wide!
    sourceOff = (image->y << 8) + (image->y << 6) + image->x;
    bitmapOff = 0;

    // create aliases to variables so the structure doesn't need to be
    // dereferenced continually
    height = image->height;
    width = image->width;
    bitmapData = image->buffer;

    // draw each line of the bitmap, note how each pixel doesn't need to be
    // tested for transparency hence a memcpy can be used (very fast!)
    for (y = 0; y < height; y++) {

        // copy the next row into the bitmap buffer using memcpy for speed
        MEMCPY((void FAR*)&bitmapData[bitmapOff], (void FAR*)&source[sourceOff], width);

        // move to next line in source buffer and in bitmap buffer
        sourceOff += SCREEN_WIDTH;
        bitmapOff += width;
    }
}

int bitmapAllocate(BitmapPtr image, int width, int height) {
    // this function can be used to allocate the memory needed for a bitmap
    if ((image->buffer = (unsigned char FAR*)MALLOC(width * height + 1)) == NULL) {
        return 0;
    } else {
        return 1;
    }
}

void bitmapDelete(BitmapPtr image) {
    // this function deletes the memory used by a bitmap
    if (image->buffer) {
        FREE(image->buffer);
    }

    image->buffer = NULL;
}

int pcxInit(PcxPicturePtr image) {
    // this function allocates the buffer that the image data will be loaded into
    // when a PCX file is decompressed
    if (!(image->buffer = (unsigned char FAR*)MALLOC(SCREEN_WIDTH * SCREEN_HEIGHT + 1))) {
        printf("\nPCX SYSTEM - Couldn't allocate PCX image buffer");
        return 0;
    }

    return 1;
}

void pcxDelete(PcxPicturePtr image) {
    // this function de-allocates the buffer region used for the pcx file load
    FREE(image->buffer);
}

int pcxLoad(char* filename, PcxPicturePtr image, int loadPalette) {
    // this function loads a PCX file into the image structure. The function
    // has three main parts: 1. load the PCX header, 2. load the image data and
    // decompress it and 3. load the palette data and update the VGA palette
    // note: the palette will only be loaded if the loadPalette flag is 1
    FILE* fp;               // the file pointer used to open the PCX file
    int numBytes,           // number of bytes in current RLE run
        index;              // loop variable
    long count;             // the total number of bytes decompressed
    unsigned char data;     // the current pixel data
    char FAR* tempBuffer;   // working buffer

    // open the file, test if it exists
    if ((fp = fopen(filename, "rb")) == NULL) {
        printf("\nPCX SYSTEM - Couldn't find file: %s", filename);
        return 0;
    }

    // load the header
    tempBuffer = (char FAR*)image;

    for (index = 0; index < 128; index++) {
        tempBuffer[index] = (char)getc(fp);
    }

    // load the data and decompress into buffer, we need a total of 64,000 bytes
    count = 0;

    // loop while 64,000 bytes haven't been decompressed
    while (count < SCREEN_WIDTH * SCREEN_HEIGHT) {
        // get the first piece of data
        data = (unsigned char)getc(fp);

        // is this a RLE run?
        if (data >= 192) {
            // compute number of bytes in run
            numBytes = data - 192;

            // get the actual data for the run
            data = (unsigned char)getc(fp);

            // replicate data in buffer numBytes times
            while ((numBytes--) > 0) {
                image->buffer[count++] = data;
            }
        } else {
            // actual data, just copy it into buffer at next location
            image->buffer[count++] = data;
        }
    }

    // load color palette
    
    // move to end of file then back up 768 bytes i.e. to beginning of palette
    fseek(fp, -768L, SEEK_END);

    // load the PCX palette into the VGA color registers
    for (index = 0; index < 256; index++) {
        // get the red component
        image->palette[index].red = (unsigned char)(getc(fp) >> 2);

        // get the green component
        image->palette[index].green = (unsigned char)(getc(fp) >> 2);

        // get the blue component
        image->palette[index].blue = (unsigned char)(getc(fp) >> 2);
    }

    // time to close the file
    fclose(fp);

    // change the palette to newly loaded palette if commanded to do so
    if (loadPalette) {
        // for each palette register set to the new color values
        for (index = 0; index < 256; index++) {
            writeColorReg(index, &image->palette[index]);
        }
    }

    return 1;
}

void pcxShowBuffer(PcxPicturePtr image) {
    // copy the PCX buffer into the video buffer
    char FAR* data; // temp variable used for aliasing

    // alias image buffer
    data = image->buffer;

    // use inline assembly for speed
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
    sprite->background = (unsigned char FAR*)MALLOC(width * height + 1);

    if (!sprite->background) {
        printf("\nERROR: spriteInit() - Out of memory allocating background (%d bytes)\n", width * height + 1);
        return;  // Can't continue without memory
    }

    // set all bitmap pointers to null
    for (index = 0; index < MAX_SPRITE_FRAMES; index++) {
        sprite->frames[index] = NULL;
    }
}

void spriteDelete(SpritePtr sprite) {
    // this function deletes all the memory associated with a sprite
    int index;

    FREE(sprite->background);

    // now de-allocate all the animation frames
    for (index = 0; index < MAX_SPRITE_FRAMES; index++) {
        FREE(sprite->frames[index]);
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
    int xOff,   // position of sprite cell in PCX image buffer
        yOff,
        y,
        width,  // size of sprite
        height;
    unsigned char FAR* spriteData;

    // extract width and height of sprite
    width = sprite->width;
    height = sprite->height;

    // first allocate the memory for the sprite in the sprite structure
    sprite->frames[spriteFrame] = (unsigned char FAR*)MALLOC(width * height + 1);

    // create an alias to the sprite frame for ease of access
    spriteData = sprite->frames[spriteFrame];

    if (!sprite->frames[spriteFrame]) {
        printf("\nERROR: pcxGetSprite() - Out of memory allocating frame %d (%d bytes)\n", 
            spriteFrame, width * height + 1);
        return;  // Can't continue without memory
    }

    // now load the sprite data into the sprite frame away from the PCX picture
    xOff = (width + 1) * cellX + 1;
    yOff = (height + 1) * cellY + 1;

    // compute starting y address
    yOff = yOff * 320;  // 320 bytes per line

    // scan the data row by row
    for (y = 0; y < height; y++, yOff += 320) {
        // copy the row of pixels
        MEMCPY(
            (void FAR*)&spriteData[y * width],
            (void FAR*)&image->buffer[yOff + xOff],
            width);
    }

    // increment number of frames
    sprite->numFrames++;

    // done!, let's bail!
}

void spriteDraw(SpritePtr sprite, unsigned char FAR* buffer, int transparent) {
    // this function draws a sprite on the screen row by row very quickly
    // note the use of shifting to implement multiplication
    // if the transparent flag is true then pixels will be drawn one by one
    // else a memcpy will be used to draw each line
    unsigned char FAR* spriteData;  // pointer to sprite data
    unsigned char FAR* destBuffer;  // pointer to destination buffer
    int x, y,                       // looping variable
        width,                      // width of sprite
        height;                     // height of sprite
    unsigned char pixel;            // the current pixel being processed

    // alias a pointer to sprite for ease of access
    spriteData = sprite->frames[sprite->currFrame];

    // alias a variable to sprite size
    width = sprite->width;
    height = sprite->height;

    // compute number of bytes between adjacent video lines after a row of pixels has been drawn

    // compute offset of sprite in destination buffer
    destBuffer = buffer + (sprite->y << 8) + (sprite->y << 6) + sprite->x;

    // copy each line of the sprite data into destination buffer
    if (transparent) {
        for (y = 0; y < height; y++) {
            // copy the next row into the destination buffer
            for (x = 0; x < width; x++) {
                // test for transparent pixel i.e. 0, if not transparent then draw
                if ((pixel = spriteData[x])) {
                    destBuffer[x] = pixel;
                }
            }

            // move to next line in destination buffer and sprite image buffer
            destBuffer += SCREEN_WIDTH;
            spriteData += width;
        }
    } else {
        // draw sprite with transparency off
        for (y = 0; y < height; y++) {
            // copy the next row into the destination buffer
            MEMCPY((void FAR*)destBuffer, (void FAR*)spriteData, width);

            // move to next line in destination buffer and sprite image buffer
            destBuffer += SCREEN_WIDTH;
            spriteData += width;
        }
    }
}

void spriteUnder(SpritePtr sprite, unsigned char FAR* buffer) {
    // this function scans the background under a sprite so that when the sprite
    // is drawn the background isn't obliterated
    unsigned char FAR* backBuffer;  // background buffer for sprite
    int y,                          // current line being scanned
        width,                      // size of sprite
        height;

    // alias a pointer to sprite background for ease of access
    backBuffer = sprite->background;

    // alias width and height
    width = sprite->width;
    height = sprite->height;

    // compute offset of background in source buffer
    buffer = buffer + (sprite->y << 8) + (sprite->y << 6) + sprite->x;

    for (y = 0; y < height; y++) {
        // copy the next row out of image buffer into sprite background buffer
        MEMCPY((void FAR*) backBuffer, (void FAR*)buffer, width);

        // move to next line in source buffer and in sprite background buffer
        buffer += SCREEN_WIDTH;
        backBuffer += width;
    }
}

void spriteErase(SpritePtr sprite, unsigned char FAR* buffer) {
    // replace the background that was behind the sprite
    // this function replaces the background that was saved from where a sprite
    // was going to be placed
    unsigned char FAR* backBuffer;  // background buffer for sprite
    int y,                          // current line being scanned
        width,                      // size of sprite
        height;

    // alias a pointer to sprite background for ease of access
    backBuffer = sprite->background;

    // alias width and height
    width = sprite->width;
    height = sprite->height;

    // compute offset in destination buffer
    buffer = buffer + (sprite->y << 8) + (sprite->y << 6) + sprite->x;

    for (y = 0; y < height; y++) {
        // copy the next from sprite background buffer to destination buffer
        MEMCPY((void FAR*)buffer, (void FAR*)backBuffer, width);

        // move to next line in destination buffer and in sprite background buffer
        buffer += SCREEN_WIDTH;
        backBuffer += width;
    }
}

void spriteDrawClip(SpritePtr sprite, unsigned char FAR* buffer, int transparent) {
    // this function draws a sprite on the screen row by row very quickly
    // note the use of shifting to implement multiplication
    // if the transparent flag is true then pixels will be drawn one by one
    // else a memcpy will be used to draw each line
    // this function also performs clipping. It will test if the sprite
    // is totally visible/invisible and will only draw the portions that are visible
    unsigned char FAR* spriteData;  // pointer to sprite data
    unsigned char FAR* destBuffer;  // pointer to destination buffer
    int x, y,                       // looping variables
        sx, sy,                     // position of sprite
        width,                      // width of sprite
        bitmapX = 0,                // starting upper left corner of sub-bitmap
        bitmapY = 0,                // to be drawn after clipping
        bitmapWidth = 0,            // width and height of sub-bitmap
        bitmapHeight = 0;
    unsigned char pixel;            // the current pixel being processed

    // alias a variable to sprite size
    width = sprite->width;
    bitmapWidth = width;
    bitmapHeight = sprite->height;
    sx = sprite->x;
    sy = sprite->y;

    // perform trivial rejection tests
    if (sx >= (int)SCREEN_WIDTH ||
        sy >= (int)DoubleBufferHeight ||
        (sx + width) <= 0 ||
        (sy + bitmapHeight) <= 0 ||
        !sprite->visible) {

        // sprite is totally invisible therefore don't draw
        // set invisible flag in structure so that the erase sub-function
        // doesn't do anything
        sprite->visible = 0;
        return;
    }

    // the sprite needs some clipping or no clipping at all, so compute
    // visible portion of sprite rectangle

    // first compute upper left hand corner of clipped sprite
    if (sx < 0) {
        bitmapX = -sx;
        sx = 0;
        bitmapWidth -= bitmapX;
    } else if (sx + width >= (int)SCREEN_WIDTH) {
        bitmapX = 0;
        bitmapWidth = (int)SCREEN_WIDTH - sx;
    }

    // now process y
    if (sy < 0) {
        bitmapY = -sy;
        sy = 0;
        bitmapHeight -= bitmapY;
    } else if (sy + bitmapHeight >= (int)DoubleBufferHeight) {
        bitmapY = 0;
        bitmapHeight = (int)DoubleBufferHeight - sy;
    }

    // this point we know where to start drawing the bitmap i.e.
    // sx, sy
    // and we know where in the data to extract the bitmap i.e.
    // bitmapX, bitmapY,
    // and finally we know the size of the bitmap to be drawn i.e.
    // width, height, so plug it all into the rest of function

    // compute number of bytes between adjacent video lines after a row of pixels
    // has been drawn

    // compute offset of sprite in destination buffer
    destBuffer = buffer + (sy << 8) + (sy << 6) + sx;

    // alias a pointer to sprite for ease of access and locate starting sub
    // bitmap that will be drawn
    spriteData = sprite->frames[sprite->currFrame] + (bitmapY * width) + bitmapX;

    // copy each line of the sprite data into destination buffer
    if (transparent) {
        for (y = 0; y < bitmapHeight; y++) {
            // copy the next row into the destination buffer
            for (x = 0; x < bitmapWidth; x++) {
                // test for transparent pixel i.e. 0, if not transparent then draw
                if ((pixel = spriteData[x])) {
                    destBuffer[x] = pixel;
                }
            }

            // move to next line in destination buffer and sprite image buffer
            destBuffer += SCREEN_WIDTH;
            spriteData += width;    // note this width is the actual width of the
                                    // entire bitmap NOT the visible portion
        }
    } else {
        // draw sprite with transparency off
        for (y = 0; y < bitmapHeight; y++) {
            // copy the next row into the destination buffer
            MEMCPY((void FAR*)destBuffer, (void FAR*)spriteData, bitmapWidth);

            // move to next line in destination buffer and sprite image buffer
            destBuffer += SCREEN_WIDTH;
            spriteData += width;    // note this width is the actual width of the
                                    // entire bitmap NOT the visible portion
        }
    }

    // set variables in structure so that the erase sub-function can operate faster
    sprite->xClip = sx;
    sprite->yClip = sy;
    sprite->widthClip = bitmapWidth;
    sprite->heightClip = bitmapHeight;
    sprite->visible = 1;
}

void spriteUnderClip(SpritePtr sprite, unsigned char FAR* buffer) {
    // this function scans the background under a sprite, but only those
    // portions that are visible
    unsigned char FAR* backBuffer;      // pointer to sprite background buffer
    unsigned char FAR* sourceBuffer;    // pointer to source buffer
    int y,                       // looping variables
        sx, sy,                     // position of sprite
        width,                      // width of sprite
        bitmapWidth = 0,            // width and height of sub-bitmap
        bitmapHeight = 0;

    // alias a variable to sprite size
    width = sprite->width;
    bitmapWidth = width;
    bitmapHeight = sprite->height;
    sx = sprite->x;
    sy = sprite->y;

    // perform trivial rejection tests
    if (sx >= (int)SCREEN_WIDTH ||
        sy >= (int)DoubleBufferHeight ||
        (sx + width) <= 0 ||
        (sy + bitmapHeight) <= 0) {

        // sprite is totally invisible therefore don't draw
        // set invisible flag in structure so that the erase sub-function
        // doesn't do anything
        sprite->visible = 0;
        return;
    }

    // the sprite background region must be clipped before scanning
    // therefore compute visible portion

    // first compute upper left hand corner of clipped sprite background
    if (sx < 0) {
        bitmapWidth += sx;
        sx = 0;
    } else if (sx + width >= (int)SCREEN_WIDTH) {
        bitmapWidth = (int)SCREEN_WIDTH - sx;
    }

    // now process y
    if (sy < 0) {
        bitmapHeight += sy;
        sy = 0;
    } else if (sy + bitmapHeight >= (int)DoubleBufferHeight) {
        bitmapHeight = (int)DoubleBufferHeight - sy;
    }

    // this point we know where to start scanning the bitmap i.e.
    // sx, sy
    // and we know the size of the bitmap to be scanned i.e.
    // width, height, so plug it all into the rest of function

    // compute number of bytes between adjacent video lines after a row of pixels
    // has been drawn

    // compute offset of sprite in source buffer
    sourceBuffer = buffer + (sy << 8) + (sy << 6) + sx;

    // alias a pointer to sprite background
    backBuffer = sprite->background;

    for (y = 0; y < bitmapHeight; y++) {
        // copy the next row into the destination buffer
        MEMCPY((void FAR*)backBuffer, (void FAR*)sourceBuffer, bitmapWidth);

        // move to next line in destination buffer and sprite image buffer
        sourceBuffer += SCREEN_WIDTH;
        backBuffer += width;    // note this width is the actual width of the
                                // entire bitmap NOT the visible portion
    }

    // set variables in structure so that the erase sub-function can operate faster
    sprite->xClip = sx;
    sprite->yClip = sy;
    sprite->widthClip = bitmapWidth;
    sprite->heightClip = bitmapHeight;
    sprite->visible = 1;
}

void spriteEraseClip(SpritePtr sprite, unsigned char FAR* buffer) {
    // replace the background that was behind the sprite
    // this function replaces the background that was saved from where a sprite
    // was going to be placed
    unsigned char FAR* backBuffer;  // background buffer for sprite
    int y,                          // current line being scanned
        width,                      // size of sprite background buffer
        bitmapHeight,               // size of clipped bitmap
        bitmapWidth;

    // make sure sprite was visible
    if (!sprite->visible) {
        return;
    }

    // alias a pointer to sprite background for ease of access
    backBuffer = sprite->background;

    // alias width and height
    bitmapWidth = sprite->widthClip;
    bitmapHeight = sprite->heightClip;
    width = sprite->width;

    // compute offset in destination buffer
    buffer = buffer + (sprite->yClip << 8) + (sprite->yClip << 6) + sprite->xClip;

    for (y = 0; y < bitmapHeight; y++) {
        // copy the next row from sprite background buffer to destination buffer
        MEMCPY((void FAR*)buffer, (void FAR*)backBuffer, bitmapWidth);

        // move to next line in destination buffer and in sprite background buffer
        buffer += SCREEN_WIDTH;
        backBuffer += width;
    }
}

void pcxCopyToBuffer(PcxPicturePtr image, unsigned char FAR* buffer) {
    // this function is used to copy the data in the PCX buffer to another buffer
    // usually the double buffer

    // use the word copy function, note: doubleBufferSize is in WORDs
    fwordcpy((void FAR*)buffer, (void FAR*)image->buffer, DoubleBufferSize);
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
    if ((destLayer->buffer = (unsigned char FAR*)MALLOC(width * height * 2)) == NULL) {
        return 0;
    } else {
        // save the dimensions of layer
        destLayer->width = width;
        destLayer->height = height;
        return 1;
    }
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
    // this allows a layer to be very long, tall, etc. also the source data buffer
    // must be constructed such that there are 320 bytes per row
    int y,
        layerWidth;                 // the width of the layer
    unsigned char FAR* sourceData;  // pointer to start of source bitmap image
    unsigned char FAR* layerBuffer; // pointer to layer buffer

    // extract width of layer
    layerWidth = destLayer->width;

    // compute starting location in layer buffer
    layerBuffer = destLayer->buffer + layerWidth * destY + destX;

    // compute starting location in source image buffer
    sourceData = sourceBuffer + (sourceY << 8) + (sourceY << 6) + sourceX;

    // scan each line of source image into layer buffer
    for (y = 0; y < height; y++) {
        // copy the next row into the layer buffer using memcpy for speed
        MEMCPY((void FAR*) layerBuffer, (void FAR*)sourceData, width);

        // move to next line in source buffer and in layer buffer
        sourceData += SCREEN_WIDTH;
        layerBuffer += layerWidth;
    }
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
    // buffer at the desired location, note the width of the destination buffer
    // is always assumed to be 320 bytes width. Also, the function will always
    // wrap around the layer
    int x, y,
        layerWidth,         // the width of the layer
        rightWidth,         // the width of the right and left half of
        leftWidth;          // the layer to be drawn
    unsigned char FAR* layerBufferL;    // pointers to the left and right halves
    unsigned char FAR* destBufferL;     // of the layer buffer and destination
    unsigned char FAR* layerBufferR;    // buffer
    unsigned char FAR* destBufferR;
    unsigned char pixel;                // current pixel value being processed

    layerWidth = sourceLayer->width;
    destBufferL = destBuffer + (destY << 8) + (destY << 6);
    layerBufferL = sourceLayer->buffer + layerWidth * sourceY + sourceX;

    // test if wrapping is needed
    if (((layerWidth - sourceX) - (int)SCREEN_WIDTH) >= 0) {
        // there's enough data in layer to draw a complete line, no wrapping needed
        leftWidth = SCREEN_WIDTH;
        rightWidth = 0;  // no wrapping flag
    } else {
        // wrapping needed
        leftWidth = layerWidth - sourceX;
        rightWidth = SCREEN_WIDTH - leftWidth;
        destBufferR = destBufferL + leftWidth;
        layerBufferR = layerBufferL - sourceX;  // move to far left end of layer
    }

    // test if transparency is on or off
    if (transparent) {
        // use version that will draw a transparent bitmap (slightly slower)
        // first draw left half then right half
        // draw each line of the bitmap
        for (y = 0; y < destHeight; y++) {
            // copy the next row into the destination buffer
            for (x = 0; x < leftWidth; x++) {
                // test for transparent pixel i.e. 0, if not transparent then draw
                if ((pixel = layerBufferL[x])) {
                    destBufferL[x] = pixel;
                }
            }

            // move to next line in destination buffer and in layer buffer
            destBufferL += SCREEN_WIDTH;
            layerBufferL += layerWidth;
        }

        // now right half

        // draw each line of the bitmap
        if (rightWidth) {
            for (y = 0; y < destHeight; y++) {
                // copy the next row into the destination buffer
                for (x = 0; x < rightWidth; x++) {
                    // test for transparent pixrl i.e. 0, if not transparent then draw
                    if ((pixel = layerBufferR[x])) {
                        destBufferR[x] = pixel;
                    }
                }

                // move to next line in destination buffer and in layer buffer
                destBufferR += SCREEN_WIDTH;
                layerBufferR += layerWidth;
            }
        }
    } else {
        // draw each line of the bitmap, note how each pixel doesn't need to be
        // tested for transparency hence a memcpy can be used (very fast!)
        for (y = 0; y < destHeight; y++) {
            // copy the next row into the destination buffer using memcpy for speed
            MEMCPY((void FAR*)destBufferL, (void FAR*)layerBufferL, leftWidth);

            // move to next line in double buffer and in bitmap buffer
            destBufferL += SCREEN_WIDTH;
            layerBufferL += layerWidth;
        }

        // now right half if needed
        if (rightWidth) {
            for (y = 0; y < destHeight; y++) {
                // copy the next row into the destination buffer using memcpy for speed
                MEMCPY((void FAR*)destBufferR, (void FAR*)layerBufferR, rightWidth);

                // move to next line in double buffer and in bitmap buffer
                destBufferR += SCREEN_WIDTH;
                layerBufferR += layerWidth;
            }
        }
    }
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
        outp(CRT_CONTROLLER, CRT_ADDR_HI);
        outp(CRT_CONTROLLER + 1, 0x00);

        // now high byte
        outp(CRT_CONTROLLER, CRT_ADDR_HI);
        outp(CRT_CONTROLLER + 1, 0x80);
    }
}
