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

    // set graphics mode to 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // point double buffer to video buffer since the triangle function
    // only writes to the double buffer and we want to see it doing the
    // writing
    DoubleBuffer = VideoBuffer;

    // main loop
    while (!kbhit()) {
        // draw a triangle somewhere on the screen
        drawTriangle2D(
            rand() % 320,
            rand() % 200,
            rand() % 320,
            rand() % 200,
            rand() % 320,
            rand() % 200,
            rand() % 256);
    }

    // restore text mode
    setGraphicsMode(TEXT_MODE);
}

