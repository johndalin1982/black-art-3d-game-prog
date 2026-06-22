#include <stdlib.h>

#define SCREEN_WIDTH      (unsigned int)320 // mode 13h screen dimensions
#define SCREEN_HEIGHT     (unsigned int)200

// screen transition commands
#define SCREEN_DARKNESS     0   // fade to black
#define SCREEN_WHITENESS    1   // fade to white
#define SCREEN_WARP         2   // warp the screen image
#define SCREEN_SWIPE_X      3   // do a horizontal swipe
#define SCREEN_SWIPE_Y      4   // do a vertical swipe
#define SCREEN_DISSOLVE      5   // a pixel dissolve

#define MAX_SPRITE_FRAMES 32

// sprite states
#define SPRITE_DEAD       0
#define SPRITE_ALIVE      1
#define SPRITE_DYING      2

// mode Z page stuff
#define PAGE_0  0
#define PAGE_1  1

extern unsigned char FAR* RomCharSet;
extern unsigned char FAR* DoubleBuffer;
extern unsigned int DoubleBufferSize;   // total size of buffer in bytes
extern unsigned int DoubleBufferHeight; // row count of the buffer createDoubleBuffer last sized

// this is the typedef for a bitmap
typedef struct BitmapType {
    int x, y;                   // position of bitmap
    int width, height;          // size of bitmap
    unsigned char FAR* buffer;  // buffer holding image
} Bitmap, *BitmapPtr;

#ifdef DOS_32_BIT
#define INT16 short
#else
#define INT16 int
#endif

// the PCX file structure
#pragma pack(push, 1)  // Force byte-aligned packing - no padding
typedef struct PcxHeaderType {
    char manufacturer;          // the manufacturer of the file
    char version;               // the file format version
    char encoding;              // type of compression
    char bitsPerPixel;          // number of bits per pixel
    INT16 x, y;                   // starting location of image
    INT16 width, height;          // size of image
    INT16 horzRes;                // resolution in DPI (dots per inch)
    INT16 vertRes;
    char egaPalette[48];        // the old EGA palette (usually ignored)
    char reserved;              // don't care
    char numColorPlanes;        // number of color planes
    INT16 bytesPerLine;           // number of bytes per line of the image
    INT16 paletteType;            // 1 for color, 2 for grey scale palette
    char padding[58];           // extra bytes
} PcxHeader, *PcxHeaderPtr;

// this holds the PCX header and the actual image
typedef struct PcxPictureType {
    PcxHeader header;           // the header of the PCX file
    RgbColor palette[256];      // the palette data
    unsigned char FAR* buffer;  // holding the decompressed image
#ifdef VBE_SUPPORT
    int bpp;                    // this file's own depth: 8 (indexed) or 32
                                // (true colour, RGB widened on load) - checked
                                // against the runtime DisplayBpp before use.
                                // 24-bit true-colour PCX support is VESA-only
                                // (mode-13h is always 8bpp indexed, matching
                                // the book), hence this field only exists
                                // here too.
#endif
} PcxPicture, *PcxPicturePtr;
#pragma pack(pop)  // Restore default packing

// this is a sprite structure
typedef struct SpriteType {
    int x, y;           // position of sprite
    int width, height;  // dimensions of sprite in pixels
    int counter1;       // some counters for timing and animation
    int counter2;
    int counter3;
    int threshold1;     // thresholds for the counters (if needed)
    int threshold2;
    int threshold3;
    unsigned char FAR* frames[MAX_SPRITE_FRAMES];   // array of pointers to the images
#ifdef VBE_SUPPORT
    // tinted sprites (spriteDrawTinted/pcxGetSpriteTinted) have no
    // book-original equivalent - VESA-only, same reasoning as PcxPicture.bpp
    unsigned char FAR* tintMask[MAX_SPRITE_FRAMES]; // optional, one byte/pixel: 0 means
                                    // the frame's own pixel; N (1-based) means
                                    // substitute tint region N's live color at
                                    // draw time (0 = unused, the default)
    unsigned long transparentColor; // native-format value a transparent draw
                                    // skips (0 by default - "index 0 is
                                    // transparent", matching the book's own
                                    // hardcoded convention)
#endif
    int currFrame;                  // current frame being displayed
    int numFrames;                  // total number of frames
    int state;                      // state of sprite, alive, dead...
    unsigned char FAR* background;  // image under the sprite
    int xClip, yClip;               // clipped position of sprite
    int widthClip, heightClip;      // clipped size of sprite used
    int visible;                    // by sprite engine to flag
                                    // if a sprite was invisible last
                                    // time it was drawn hence the background
                                    // need not be replaced
} Sprite, *SpritePtr;

// this is a typedef used for the layers in parallax scrolling
// note it is identical to a bitmap, but we'll make a separate typedef
// in the event we later need to add fields to it
typedef struct LayerType {
    int x, y;                   // used to hold position information
                                // no specific function
    int width, height;          // size of layer, note: width must
                                // be divisible by 2
    unsigned char FAR* buffer;  // the layer buffer
} Layer, *LayerPtr;

void screenTransition(int effect);
void waitForVerticalRetrace(void);
void printCharDb(int xc, int yc, char c, int color, int transparent);
void printStringDb(int x, int y, int color, char* string, int transparent);
void writePixelDb(int x, int y, int color);
int readPixelDb(int x, int y);
int createDoubleBuffer(int numLines);
void fillDoubleBuffer(int color);
void displayDoubleBuffer(unsigned char FAR* buffer, int y);
void deleteDoubleBuffer(void);
void bitmapPut(BitmapPtr image, unsigned char FAR* destination, int transparent);
void bitmapGet(BitmapPtr image, PcxPicturePtr source);
int bitmapAllocate(BitmapPtr image, int width, int height);
void bitmapDelete(BitmapPtr image);
int pcxInit(PcxPicturePtr image);
void pcxDelete(PcxPicturePtr image);
int pcxLoad(char* filename, PcxPicturePtr image, int loadPalette);
void pcxShowBuffer(PcxPicturePtr image);
void spriteInit(
    SpritePtr sprite,
    int x, int y,
    int width, int height,
    int c1, int c2, int c3,
    int t1, int t2, int t3);
void spriteDelete(SpritePtr sprite);
void pcxGetSprite(
    PcxPicturePtr image,
    SpritePtr sprite,
    int spriteFrame,
    int cellX, int cellY);
void spriteDraw(SpritePtr sprite, unsigned char FAR* buffer, int transparent);
void spriteUnder(SpritePtr sprite, unsigned char FAR* buffer);
void spriteErase(SpritePtr sprite, unsigned char FAR* buffer);
void spriteDrawClip(SpritePtr sprite, unsigned char FAR* buffer, int transparent);
void spriteUnderClip(SpritePtr sprite, unsigned char FAR* buffer);
void spriteEraseClip(SpritePtr sprite, unsigned char FAR* buffer);
#ifdef VBE_SUPPORT
// Like spriteDrawClip, but a pixel marked region N (1-based) in
// sprite->tintMask[currFrame] (see pcxGetSpriteTinted) draws as
// tintColors[N-1] instead of its own stored pixel. Always clips - no
// unclamped fast-path sibling, unlike spriteDraw/spriteDrawClip. No
// book-original equivalent - VESA-only, see Sprite.tintMask above.
void spriteDrawTinted(SpritePtr sprite, unsigned char FAR* buffer, int transparent,
                     const unsigned long* tintColors, int numTintColors);
// Like pcxGetSprite, but also marks tinted pixels: a source pixel equal to
// tintKeys[i] is recorded in sprite->tintMask[spriteFrame] (allocated here)
// as region (i+1) instead of being copied into the frame verbatim, so
// spriteDrawTinted can substitute a live color for it at draw time.
void pcxGetSpriteTinted(
    PcxPicturePtr image,
    SpritePtr sprite,
    int spriteFrame,
    int cellX, int cellY,
    const unsigned long* tintKeys, int numTintKeys);
#endif
void pcxCopyToBuffer(PcxPicturePtr image, unsigned char FAR* buffer);
void fwordcpy(void FAR* destination, void FAR* source, int numWords);
int layerCreate(LayerPtr destLayer, int width, int height);
void layerDelete(LayerPtr layer);
void layerBuild(
    LayerPtr destLayer,
    int destX,
    int destY,
    unsigned char FAR* sourceBuffer,
    int sourceX,
    int sourceY,
    int width,
    int height);
void layerDraw(
    LayerPtr sourceLayer,
    int sourceX,
    int sourceY,
    unsigned char FAR* destBuffer,
    int destY,
    int destHeight,
    int transparent);
void setWorkingPageModeZ(int page);
void setVisualPageModeZ(int page);
