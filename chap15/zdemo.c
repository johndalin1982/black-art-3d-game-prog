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
#include <search.h>             // this one is needed for qsort()

// include all of our stuff

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black11.h"
#include "black15.h"

void main() {
    int x = 160,    // local position of object
        y = 100,
        z = 100;

    // set graphics to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // point double buffer at video buffer, so we can see output without an
    // animation loop
    DoubleBuffer = VideoBuffer;

    // create a 200 line z buffer (128k)
    createZBuffer(200);

    // initialize the z buffer with an impossibly distant value
    fillZBuffer(16000);

    // draw two intersecting triangles
    drawTri3DZ(
        x, y, z,
        x - 30, y + 40, z,
        x + 40, y + 50, z,
        10);

    drawTri3DZ(
        x + 10, y + 10, z - 5,
        x - 50, y + 60, z + 5,
        x + 30, y + 20, z - 2,
        1);

    // wait for keyboard hit
    while (!kbhit());

    // restore graphics mode back to text
    setGraphicsMode(TEXT_MODE);

    // release the z buffer memory
    deleteZBuffer();
}
