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
    // set graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // draw randomly positioned lines until user hits a key
    while (!kbhit()) {
        // draw the line with random (x0,y0) - (x1,y1) with a random color
        drawLine(
            rand() % 320,
            rand() % 200,
            rand() % 320,
            rand() % 200,
            rand() % 256,
            VideoBuffer);
    }

    // restore graphics mode back to text
    setGraphicsMode(TEXT_MODE);
}
