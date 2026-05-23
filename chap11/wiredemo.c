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
#include <search.h> // this one is needed for qsort()

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black11.h"

Object TestObject;  // the test object

void main(int argc, char** argv) {
    int index,
        done = 0;
    char buffer[80];

    // load in the object from the command line
    if (!plgLoadObject(&TestObject, argv[1], 1)) {
        printf("Couldn't find file %s", argv[1]);
        return;
    }

    // position the object
    TestObject.worldPos.x = 0;
    TestObject.worldPos.y = 0;
    TestObject.worldPos.z = 300;

    // create the sin/cos lookup tables used for the rotation function
    buildLookUpTables();

    // set graphics to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // allocate double buffer
    createDoubleBuffer(200);

    // install the isr keyboard driver
    keyboardInstallDriver();

    // set viewing distance
    ViewingDistance = 250;

    // main event loop
    while (!done) {
        // compute starting time of this frame
        StartingTime = timerQuery();

        // erase screen
        fillDoubleBuffer(0);

        // test what key(s) user is pressing

        // test if user is moving object to right
        if (KeyboardState[MAKE_RIGHT]) {
            TestObject.worldPos.x += 5;
        }

        // test if user is moving object to left
        if (KeyboardState[MAKE_LEFT]) {
            TestObject.worldPos.x -= 5;
        }

        // test if user is moving object up
        if (KeyboardState[MAKE_UP]) {
            TestObject.worldPos.y -= 5;
        }

        // test if user is moving object down
        if (KeyboardState[MAKE_DOWN]) {
            TestObject.worldPos.y += 5;
        }

        // test if user is moving object farther
        if (KeyboardState[MAKE_PGUP]) {
            TestObject.worldPos.z += 15;
        }

        // test if user is moving object nearer
        if (KeyboardState[MAKE_PGDWN]) {
            TestObject.worldPos.z -= 15;
        }

        // test for exit key
        if (KeyboardState[MAKE_ESC]) {
            done = 1;
        }

        // rotate the object on all three axes
        rotateObject(&TestObject, 2, 4, 6);

        // convert the local coordinates into camera coordinates for projection
        // note the viewer is at (0,0,0) with angles 0,0,0 so the transformation
        // is simply to add the world position to each local vertex
        for (index = 0; index < TestObject.numVertices; index++) {
            TestObject.verticesCamera[index].x =
                TestObject.verticesLocal[index].x + TestObject.worldPos.x;
             TestObject.verticesCamera[index].y =
                TestObject.verticesLocal[index].y + TestObject.worldPos.y;
             TestObject.verticesCamera[index].z =
                TestObject.verticesLocal[index].z + TestObject.worldPos.z;
        }

        // draw the object
        drawObjectWire(&TestObject);

        // print out position of object
        sprintf(
            buffer,
            "Object is at (%d,%d,%d)    ",
            TestObject.worldPos.x,
            TestObject.worldPos.y,
            TestObject.worldPos.z);

        printStringDb(0, 0, 9, buffer, 0);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // local onto 18 frames per second max
        while ((timerQuery() - StartingTime) < 1);
    }

    // restore graphics mode back to text
    setGraphicsMode(TEXT_MODE);

    // restore the old keyboard driver
    keyboardRemoveDriver();
}

