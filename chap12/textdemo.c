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
        x = 140,
        y = 100,
        currTexture = 0;

    // set graphics mode to 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // load in the text textures 64x64
    pcxInit(&ImagePcx);
    pcxLoad("textures.pcx", &ImagePcx, 1);

    // initialize the texture sprite
    spriteInit(&Textures, 0, 0, 64, 64, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the textures (four of them: sone, wood, slime, lava)
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &Textures, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // draw textures on screen so user can see what is going on
    spriteDraw(&Textures, VideoBuffer, 1);
    Textures.x += 64;
    Textures.currFrame++;

    spriteDraw(&Textures, VideoBuffer, 1);
    Textures.x += 64;
    Textures.currFrame++;

    spriteDraw(&Textures, VideoBuffer, 1);
    Textures.x += 64;
    Textures.currFrame++;

    // draw textured triangle
    drawTriangle2DText(x, y, x - 50, y + 60, x + 30, y + 80, VideoBuffer, currTexture);

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
            drawTriangle2DText(x, y, x - 50, y + 60, x + 30, y + 80, VideoBuffer, currTexture);
        }
    }

    // restore text mode
    setGraphicsMode(TEXT_MODE);
}
