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
#include <search.h> // this one is needed for qsort

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black11.h"

void main(void) {
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

    // draw randomly positioned lines until user hits a key
    while (!kbhit()) {
        // draw the line with random (x0,y0) - (x1,y1) with a random color
        drawLine(
            rand() % screenWidth,
            rand() % screenHeight,
            rand() % screenWidth,
            rand() % screenHeight,
            rand() % 256,
            VideoBuffer);
    }

    // restore graphics mode back to text
    setGraphicsMode(TEXT_MODE);
}
