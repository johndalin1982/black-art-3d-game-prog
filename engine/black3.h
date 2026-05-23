#ifndef BLACK3_H
#define BLACK3_H

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

extern unsigned char FAR* VideoBuffer;

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

#ifdef DOS_32_BIT
int initRomCharSet(void);
void freeRomCharSet(void);
#endif

// Exit directly to DOS, bypassing C runtime cleanup
// Use this for clean game exit in both 16-bit and 32-bit modes
void exitToDos(int returnCode);

#endif

