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

void main(void) {
    int done = 0;
#ifdef VBE_SUPPORT
    int screenWidth = 640,
        screenHeight = 480;
#else
    int screenWidth = 320,
        screenHeight = 200;
#endif

    // set graphics mode
#ifdef VBE_SUPPORT
    setGraphicsModeVesa(640, 480, 8);
#else
    setGraphicsMode(GRAPHICS_MODE13);
#endif

    // point double buffer to video buffer since the triangle function
    // only writes to the double buffer and we want to see it doing the
    // writing
    DoubleBuffer = VideoBuffer;

    // main loop
    while (!kbhit()) {
        // draw a triangle somewhere on the screen
        drawTriangle2D(
            rand() % screenWidth,
            rand() % screenHeight,
            rand() % screenWidth,
            rand() % screenHeight,
            rand() % screenWidth,
            rand() % screenHeight,
            rand() % 256);
    }

    // restore text mode
    setGraphicsMode(TEXT_MODE);
}

