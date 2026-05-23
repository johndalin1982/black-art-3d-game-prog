// alien.c - A demo of parallax scrolling

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

#define MOUNTAIN_Y      106 // the starting vertical position of the mountains

#define GRASS_1_Y       150 // the starting vertical position of the grass layers
#define GRASS_2_Y       155
#define GRASS_3_Y       163
#define GRASS_4_Y       172
#define GRASS_5_Y       188

#define MOUNTAIN_HEIGHT (1 + 149 - 106) // the height of each layer

#define GRASS_1_HEIGHT  (1 + 154 - 150)
#define GRASS_2_HEIGHT  (1 + 162 - 155)
#define GRASS_3_HEIGHT  (1 + 171 - 163)
#define GRASS_4_HEIGHT  (1 + 187 - 172)
#define GRASS_5_HEIGHT  (1 + 199 - 188)

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

Sprite AlienSprite;     // our rocket sledding alien

Layer MountainsLayer,   // the layers for the mountains and grass
      Grass1Layer,
      Grass2Layer,
      Grass3Layer,
      Grass4Layer,
      Grass5Layer;

int MountainX = 0,      // positions of scan window in each layer
    Grass1X = 0,
    Grass2X = 0,
    Grass3X = 0,
    Grass4X = 0,
    Grass5X = 0;

RgbColor FireColor = { 63, 0, 0 };  // used for engines

void main(int argc, char** argv) {
    srand(time(NULL));

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // load the imagery
    pcxInit(&ImagePcx);
    pcxLoad("alienimg.pcx", &ImagePcx, 1);

    // initialize the alien sprite
    spriteInit(&AlienSprite, 160, 160, 32, 18, 0, 0, 0, 0, 0, 0);

    // extract the bitmap for the alien
    pcxGetSprite(&ImagePcx, &AlienSprite, 0, 0, 0);

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // now load the background that will be scrolled
    pcxInit(&ImagePcx);
    pcxLoad("alienwld.pcx", &ImagePcx, 1);
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);
    pcxDelete(&ImagePcx);

    // create the layers
    layerCreate(&MountainsLayer, SCREEN_WIDTH, MOUNTAIN_HEIGHT);
    layerCreate(&Grass1Layer, SCREEN_WIDTH, GRASS_1_HEIGHT);
    layerCreate(&Grass2Layer, SCREEN_WIDTH, GRASS_2_HEIGHT);
    layerCreate(&Grass3Layer, SCREEN_WIDTH, GRASS_3_HEIGHT);
    layerCreate(&Grass4Layer, SCREEN_WIDTH, GRASS_4_HEIGHT);
    layerCreate(&Grass5Layer, SCREEN_WIDTH, GRASS_5_HEIGHT);

    // scan layers out of double buffer, could have easily scanned from PCX file... just personal taste
    layerBuild(&MountainsLayer, 0, 0, DoubleBuffer, 0, MOUNTAIN_Y, SCREEN_WIDTH, MOUNTAIN_HEIGHT);

    layerBuild(&Grass1Layer, 0, 0, DoubleBuffer, 0, GRASS_1_Y, SCREEN_WIDTH, GRASS_1_HEIGHT);

    layerBuild(&Grass2Layer, 0, 0, DoubleBuffer, 0, GRASS_2_Y, SCREEN_WIDTH, GRASS_2_HEIGHT);

    layerBuild(&Grass3Layer, 0, 0, DoubleBuffer, 0, GRASS_3_Y, SCREEN_WIDTH, GRASS_3_HEIGHT);

    layerBuild(&Grass4Layer, 0, 0, DoubleBuffer, 0, GRASS_4_Y, SCREEN_WIDTH, GRASS_4_HEIGHT);

    layerBuild(&Grass5Layer, 0, 0, DoubleBuffer, 0, GRASS_5_Y, SCREEN_WIDTH, GRASS_5_HEIGHT);

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // move the alien
        if ((AlienSprite.x += 2) > 320) {
            AlienSprite.x = -32;
        }

        // move each layer
        if ((MountainX += 1) >= 319) {
            MountainX -= 320;
        }

        if ((Grass1X += 2) > 319) {
            Grass1X -= 320;
        }

        if ((Grass2X += 4) > 319) {
            Grass2X -= 320;
        }

        if ((Grass3X += 8) > 319) {
            Grass3X -= 320;
        }

        if ((Grass4X += 14) > 319) {
            Grass4X -= 320;
        }

        if ((Grass5X += 24) > 319) {
            Grass5X -= 320;
        }

        // draw layers
        layerDraw(&MountainsLayer, MountainX, 0, DoubleBuffer, MOUNTAIN_Y, MOUNTAIN_HEIGHT, 0);

        // update background layer positions
        layerDraw(&Grass1Layer, Grass1X, 0, DoubleBuffer, GRASS_1_Y, GRASS_1_HEIGHT, 0);
        layerDraw(&Grass2Layer, Grass2X, 0, DoubleBuffer, GRASS_2_Y, GRASS_2_HEIGHT, 0);
        layerDraw(&Grass3Layer, Grass3X, 0, DoubleBuffer, GRASS_3_Y, GRASS_3_HEIGHT, 0);
        layerDraw(&Grass4Layer, Grass4X, 0, DoubleBuffer, GRASS_4_Y, GRASS_4_HEIGHT, 0);
        layerDraw(&Grass5Layer, Grass5X, 0, DoubleBuffer, GRASS_5_Y, GRASS_5_HEIGHT, 0);

        // draw the sprite on top of layers
        AlienSprite.visible = 1;
        spriteDrawClip(&AlienSprite, DoubleBuffer, 1);

        // change color of fire
        FireColor.red = 20 + rand() % 44;
        writeColorReg(32, &FireColor);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_SWIPE_Y);

    // free up all resources
    spriteDelete(&AlienSprite);
    deleteDoubleBuffer();
    setGraphicsMode(TEXT_MODE);
}
