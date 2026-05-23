// jelly.c - A demo of latching onto the timer interrupt 0x1C

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
#include "black8.h"

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

Sprite JellySprite;     // the jelly fish

int IsrJellyX = -20,    // global variables that track the position of the
    IsrJellyY = 100;    // jelly fish and are updated by the ISR

// this holds the old timer keeper ISR
void (_interrupt _FAR* OldTimerIsr)();

void _interrupt _far jellyIsr(void) {
    // this function will update the global jelly fish position variables and
    // test if the jelly fish has moved off the screen, this function will be
    // called once every timer tick
    _asm sti    ; re-enable interrupts

    // move the jelly fish
    if (++IsrJellyX > 320) {
        IsrJellyX = -20;
    }

    IsrJellyY = IsrJellyY - 1 + rand() % 3;

    // test y bounds
    if (IsrJellyY > 200) {
        IsrJellyY = -20;
    } else if (IsrJellyY < -20) {
        IsrJellyY = 200;
    }

    // re-enable pic
    outp(PIC_ICR, PIC_EOI);
}

void main(int argc, char** argv) {
    int index;

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // load the imagery for the jelly fish
    pcxInit(&ImagePcx);
    pcxLoad("jelly.pcx", &ImagePcx, 1);

    // initialize the jelly fish
    spriteInit(&JellySprite, -20, 100, 16, 16, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the jelly fish, there are 4 animation cells
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &JellySprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // install the jelly fish motion interrupt
    OldTimerIsr = _dos_getvect(TIME_KEEPER_INT);
    _dos_setvect(TIME_KEEPER_INT, jellyIsr);

    // scan background before entering event loop
    spriteUnderClip(&JellySprite, DoubleBuffer);

    // put up exit instructions
    printStringDb(80, 2, 9, "Hit any key to exit", 1);

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // do animation cycle

        // erase the jelly fish
        spriteEraseClip(&JellySprite, DoubleBuffer);

        // move the jelly fish, note that we only copy global variables here
        // the variables themselves are modified by the ISR
        JellySprite.x = IsrJellyX;
        JellySprite.y = IsrJellyY;

        // animate the jelly fish
        if (++JellySprite.currFrame == 4) {
            JellySprite.currFrame = 0;
        }

        // ready to draw jelly fish, but first scan background under it
        spriteUnderClip(&JellySprite, DoubleBuffer);
        spriteDrawClip(&JellySprite, DoubleBuffer, 1);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_DARKNESS);

    // free up all resources
    spriteDelete(&JellySprite);
    deleteDoubleBuffer();

    // restore the old isr
    _dos_setvect(TIME_KEEPER_INT, OldTimerIsr);
    setGraphicsMode(TEXT_MODE);
}
