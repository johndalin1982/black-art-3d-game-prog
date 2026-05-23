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

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black11.h"
#include "black15.h"

Object TestObjects[4]; // objects in universe

void main(int argc, char** argv) {
    int done = 0,       // exit flag
        index;          // looping variable
    char buffer[80];    // output string buffer
    float x, y, z;      // working variables

    // build all look up tables
    buildLookUpTables();

    // load in the test object
    for (index = 0; index < 4; index++) {
        if (plgLoadObject(&TestObjects[index], argv[1], 1)) {
            printf("\nplg loaded.");
        } else {
            printf("\nCouldn't load file");
            return;
        }
    }

    // set position of the object
    for (index = 0; index < 4; index++) {
        TestObjects[index].worldPos.x = -200 + (index % 4) * 100;
        TestObjects[index].worldPos.y = 0;
        TestObjects[index].worldPos.z = 200 + 300 * (index >> 2);
    }

    // set graphics to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // read the 3D color palette off disk
    loadPaletteDisk("standard.pal", (RgbPalettePtr)&ColorPalette3D);
    writePalette((RgbPalettePtr)&ColorPalette3D);

    // allocate double buffer
    createDoubleBuffer(200);

    // install the isr keyboard driver
    keyboardInstallDriver();

#ifdef DOS_32_BIT
    // load the ROM font from font.bin
    if (!initRomCharSet()) {
        printf("\nFatal error: Could not initialize font system\n");
        return;
    }
#endif

    // main event loop
    while (!done) {
        // compute starting time of this frame
        StartingTime = timerQuery();

        // erase all objects

        // drawPolyList(1);

        fillDoubleBuffer(0);

        // test what user is doing

        // move the light source
        if (KeyboardState[MAKE_T]) {
            x = LightSource.x;
            y = cos(0.2) * LightSource.y - sin(0.2) * LightSource.z;
            z = sin(0.2) * LightSource.y + cos(0.2) * LightSource.z;

            LightSource.y = y;
            LightSource.z = z;
        }

        if (KeyboardState[MAKE_G]) {
            y = LightSource.y;
            x = cos(0.2) * LightSource.x + sin(0.2) * LightSource.z;
            z = -sin(0.2) * LightSource.x + cos(0.2) * LightSource.z;

            LightSource.x = x;
            LightSource.z = z;
        }

        if (KeyboardState[MAKE_B]) {
            z = LightSource.z;
            x = cos(0.2) * LightSource.x - sin(0.2) * LightSource.y;
            y = sin(0.2) * LightSource.x + cos(0.2) * LightSource.y;

            LightSource.x = x;
            LightSource.y = y;
        }

        if (KeyboardState[MAKE_Y]) {
            x = LightSource.x;
            y = cos(-0.2) * LightSource.y - sin(-0.2) * LightSource.z;
            z = sin(-0.2) * LightSource.y + cos(-0.2) * LightSource.z;

            LightSource.y = y;
            LightSource.z = z;
        }

        if (KeyboardState[MAKE_H]) {
            y = LightSource.y;
            x = cos(-0.2) * LightSource.x + sin(-0.2) * LightSource.z;
            z = -sin(-0.2) * LightSource.x + cos(-0.2) * LightSource.z;

            LightSource.x = x;
            LightSource.z = z;
        }

        if (KeyboardState[MAKE_N]) {
            z = LightSource.z;
            x = cos(-0.2) * LightSource.x - sin(-0.2) * LightSource.y;
            y = sin(-0.2) * LightSource.x + cos(-0.2) * LightSource.y;

            LightSource.x = x;
            LightSource.y = y;
        }

        // move the viewpoint
        if (KeyboardState[MAKE_UP]) {
            ViewPoint.y += 20;
        }

        if (KeyboardState[MAKE_DOWN]) {
            ViewPoint.y -= 20;
        }

        if (KeyboardState[MAKE_RIGHT]) {
            ViewPoint.x += 20;
        }

        if (KeyboardState[MAKE_LEFT]) {
            ViewPoint.x -= 20;
        }

        if (KeyboardState[MAKE_KEYPAD_PLUS]) {
            ViewPoint.z += 20;
        }

        if (KeyboardState[MAKE_KEYPAD_MINUS]) {
            ViewPoint.z -= 20;
        }

        // change the viewangles
        if (KeyboardState[MAKE_Z]) {
            if ((ViewAngle.angX += 10) > 36) {
                ViewAngle.angX = 0;
            }
        }

        if (KeyboardState[MAKE_A]) {
            if ((ViewAngle.angX -= 10) < 0) {
                ViewAngle.angX = 360;
            }
        }

        if (KeyboardState[MAKE_X]) {
            if ((ViewAngle.angY += 10) > 360) {
                ViewAngle.angY = 0;
            }
        }

        if (KeyboardState[MAKE_S]) {
            if ((ViewAngle.angY -= 10) < 0) {
                ViewAngle.angY = 360;
            }
        }

        if (KeyboardState[MAKE_C]) {
            if ((ViewAngle.angZ += 10) > 360) {
                ViewAngle.angZ = 0;
            }
        }

        if (KeyboardState[MAKE_D]) {
            if ((ViewAngle.angZ -= 10) < 0) {
                ViewAngle.angZ = 360;
            }
        }

        if (KeyboardState[MAKE_ESC]) {
            done = 1;
        }

        // rotate one of the objects
        rotateObject(&TestObjects[0], 3, 6, 9);

        // now that user has possible moved viewpoint, create the global
        // world to camera transformation matrix
        createWorldToCamera();

        // reset polygon list

        sprintf(buffer, "                           ");
        printStringDb(0, 0, 10, buffer, 0);

        generatePolyList(NULL, RESET_POLY_LIST);

        for (index = 0; index < 4; index++) {
            // test if object is visible
            if (!removeObject(&TestObjects[index], OBJECT_CULL_XYZ_MODE)) {
                // convert object local coordinates to world coordinate
                localToWorldObject(&TestObjects[index]);

                // remove the backfaces and shade object
                removeBackfacesAndShade(&TestObjects[index]);

                // convert world coordinates to camera coordinate
                worldToCameraObject(&TestObjects[index]);

                // clip the objects polygons against viewing volume
                clipObject3D(&TestObjects[index], CLIP_Z_MODE);

                // generate the final polygon list
                generatePolyList(&TestObjects[index], ADD_TO_POLY_LIST);
            } else {
                sprintf(buffer, "%d, ", index);
                printStringDb(index * 26, 0, 10, buffer, 0);
            }
        }

        // sort the polygons
        sortPolyList();

        // draw the polygon list
        drawPolyList();

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        while ((timerQuery() - StartingTime) < 1);
    }

#ifdef DOS_32_BIT
    freeRomCharSet();
#endif

    // restore graphics mode back to text
    setGraphicsMode(TEXT_MODE);

    // restore the old keyboard driver
    keyboardRemoveDriver();

    // print out some stats
    printf("\nSettings...\n");

    printf("\nview point x=%f, y=%f, z=%f", ViewPoint.x, ViewPoint.y, ViewPoint.z);
    printf("\nview angle x=%d, y=%d, z=%d", ViewAngle.angX, ViewAngle.angY, ViewAngle.angZ);
    printf("\nlight source x=%f, y=%f, z=%f", LightSource.x, LightSource.y, LightSource.z);

#ifdef DOS_32_BIT
    exitToDos(0);
#endif
}
