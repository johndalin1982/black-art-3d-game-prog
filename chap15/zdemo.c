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
    int z = 100,    // local position of object
#ifdef VBE_SUPPORT
        // fixed demo layout, doubled to (roughly) the same visual size and
        // position on the bigger screen: x by 640/320, y by 480/200 - see
        // chap12/gourdemo.c's port. z is a depth-comparison value, not a
        // screen coordinate, so it doesn't scale.
        x = 320, y = 240,
        xOff2 = -60, yOff2 = 96,
        xOff3 = 80, yOff3 = 120,
        xOff4 = 20, yOff4 = 24,
        xOff5 = -100, yOff5 = 144,
        xOff6 = 60, yOff6 = 48;
#else
        x = 160, y = 100,
        xOff2 = -30, yOff2 = 40,
        xOff3 = 40, yOff3 = 50,
        xOff4 = 10, yOff4 = 10,
        xOff5 = -50, yOff5 = 60,
        xOff6 = 30, yOff6 = 20;
#endif

    // set graphics mode
#ifdef VBE_SUPPORT
    setGraphicsModeVesa(640, 480, 8);
#else
    setGraphicsMode(GRAPHICS_MODE13);
#endif

    // point double buffer at video buffer, so we can see output without an
    // animation loop
    DoubleBuffer = VideoBuffer;

    // create a z buffer matching the active display's height
#ifdef VBE_SUPPORT
    createZBuffer(480);
#else
    createZBuffer(200);
#endif

    // initialize the z buffer with an impossibly distant value
    fillZBuffer(16000);

    // draw two intersecting triangles
    drawTri3DZ(
        x, y, z,
        x + xOff2, y + yOff2, z,
        x + xOff3, y + yOff3, z,
        10);

    drawTri3DZ(
        x + xOff4, y + yOff4, z - 5,
        x + xOff5, y + yOff5, z + 5,
        x + xOff6, y + yOff6, z - 2,
        1);

    // wait for keyboard hit
    while (!kbhit());

    // restore graphics mode back to text
    setGraphicsMode(TEXT_MODE);

    // release the z buffer memory
    deleteZBuffer();
}
