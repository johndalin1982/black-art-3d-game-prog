// pcxdemo.c - A PCX file demo that loads a PCX file and displays it

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

PcxPicture ImagePcx;     // general PCX image used to load background and imagery

void main(int argc, char** argv) {
#ifdef VBE_SUPPORT
    // set the graphics mode to SVGA 640x480x256
    setGraphicsModeVesa(640, 480, 8);
#else
    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);
#endif

    // load the screen image
    pcxInit(&ImagePcx);

    // load a PCX file (make sure it's there)
    if (pcxLoad("andre.pcx", &ImagePcx, 1)) {
        // copy the image to the display buffer
        pcxShowBuffer(&ImagePcx);

        // delete the PCX buffer
        pcxDelete(&ImagePcx);

        // wait for a keyboard press
        while (!kbhit()) {}

        // use a screen transition to exit
        screenTransition(SCREEN_DARKNESS);
    }

    // reset graphics to text mode
    setGraphicsMode(TEXT_MODE);
}
