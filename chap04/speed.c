// speed.c - A demo of 3-D palette animation

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

// the color register ranges used by the road and side markers
#define START_ROAD_REG      16
#define END_ROAD_REG        23

#define START_MARKER_REG    32
#define END_MARKER_REG      34

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

void main(int argc, char** argv) {
    RgbColor color1, color2;    // used to perform the color rotation
    int index;

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // load the screen image
    pcxInit(&ImagePcx);

    // load a PCX file (make sure it's there)
    if (pcxLoad("speed.pcx", &ImagePcx, 1)) {
        // copy the image to the display buffer
        pcxShowBuffer(&ImagePcx);

        // delete the PCX buffer
        pcxDelete(&ImagePcx);

        // remap color registers to more appropriate values

        // remap the side marker colors to red, red, white
        color1.red = 63;
        color1.green = 0;
        color1.blue = 0;

        writeColorReg(START_MARKER_REG, &color1);
        writeColorReg(START_MARKER_REG + 1, &color1);

        color1.green = 63;
        color1.blue = 63;

        writeColorReg(START_MARKER_REG + 2, &color1);

        // now color the road all grey except for three slices
        color1.red = 20;
        color1.green = 20;
        color1.blue = 20;

        for (index = START_ROAD_REG; index <= END_ROAD_REG; index++) {
            writeColorReg(index, &color1);
        }

        // now color three of the slices a slightly brighter grey
        color1.red = 30;
        color1.green = 30;
        color1.blue = 30;

        for (index = START_ROAD_REG; index <= END_ROAD_REG; index += 4) {
            writeColorReg(index, &color1);
        }

        // wait for a keyboard press
        while (!kbhit()) {
            // rotate road colors

            // temp = r1
            // r1 <--- r2 <---- r3 <---- ... ri - 1 <---- ri
            // ri = temp
            readColorReg(END_ROAD_REG, &color1);

            for (index = END_ROAD_REG; index > START_ROAD_REG; index--) {
                // read the (i-1)th register
                readColorReg(index - 1, &color2);

                // assign it to the ith
                writeColorReg(index, &color2);
            }

            // place the value of the first color register into the last to complete the rotation
            writeColorReg(START_ROAD_REG, &color1);

            // rotate side marker colors
            readColorReg(END_MARKER_REG, &color1);

            for (index = END_MARKER_REG; index > START_MARKER_REG; index--) {
                // read the (i-1)the register
                readColorReg(index - 1, &color2);

                // assign it to the ith
                writeColorReg(index, &color2);
            }

            // place the value of the first color register into the last to complete the rotation
            writeColorReg(START_MARKER_REG, &color1);

            // synchronize to 2/18th of second or 9 FPS
            timeDelay(2);
        }

        // use a screen transition to exit
        screenTransition(SCREEN_WHITENESS);
    }

    setGraphicsMode(TEXT_MODE);
}
