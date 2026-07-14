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
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black11.h"

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

void main(void) {
    int done = 0,
        index,
#ifdef VBE_SUPPORT
        // fixed demo layout, doubled to (roughly) the same visual size and
        // position on the bigger screen: x by 640/320, y by 480/200
        x = 280, y = 240,
        xOff2 = -100, yOff2 = 144,
        xOff3 = 60, yOff3 = 192,
        cellSize = 128,
#else
        x = 140, y = 100,
        xOff2 = -50, yOff2 = 60,
        xOff3 = 30, yOff3 = 80,
        cellSize = 64,
#endif
        currTexture = 0;

    // set graphics mode
#ifdef VBE_SUPPORT
    setGraphicsModeVesa(640, 480, 8);
#else
    setGraphicsMode(GRAPHICS_MODE13);
#endif

    // load in the text textures - textures.pcx is a resolution-scaled copy
    // under vbe/chap12/ (128x128 cells vs. the book's 64x64)
    pcxInit(&ImagePcx);
    pcxLoad("textures.pcx", &ImagePcx, 1);

    // initialize the texture sprite
    spriteInit(&Textures, 0, 0, cellSize, cellSize, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the textures (four of them: sone, wood, slime, lava)
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &Textures, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // draw textures on screen so user can see what is going on
    spriteDraw(&Textures, VideoBuffer, 1);
    Textures.x += cellSize;
    Textures.currFrame++;

    spriteDraw(&Textures, VideoBuffer, 1);
    Textures.x += cellSize;
    Textures.currFrame++;

    spriteDraw(&Textures, VideoBuffer, 1);
    Textures.x += cellSize;
    Textures.currFrame++;

    // draw textured triangle
    drawTriangle2DText(x, y, x + xOff2, y + yOff2, x + xOff3, y + yOff3, VideoBuffer, currTexture);

    // main loop
    while (!done) {
        // test for key
        if (kbhit()) {
            // get the key
            switch (getch()) {
                case ' ': { // space bar to select next texture
                    if (++currTexture > 3) {
                        currTexture = 0;
                    }
                } break;

                case 27: { // escape key
                    // exit system
                    done = 1;
                } break;

                default:
                    break;
            }

            // draw textured triangle
            drawTriangle2DText(x, y, x + xOff2, y + yOff2, x + xOff3, y + yOff3, VideoBuffer, currTexture);
        }
    }

    // restore text mode
    setGraphicsMode(TEXT_MODE);
}
