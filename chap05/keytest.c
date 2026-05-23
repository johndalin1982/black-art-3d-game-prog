// keytest.c - A demo of the keyboard driver

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

// include our graphics library

#include "black3.h"
#include "black4.h"
#include "black5.h"

#define START_NUMERIC_COLOR (6 * 16)    // start of color register bank that keys are drawn with

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

void main(int argc, char** argv) {
    RgbColor onColor = { 0, 63, 0 }, // the color values for the on and off buttons
             offColor = { 0, 20, 0 };

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // load the screen image
    pcxInit(&ImagePcx);

    //load a PCX file (make sure it's there)
    if (pcxLoad("keypad.pcx", &ImagePcx, 1)) {
        // copy the image to the display buffer
        pcxShowBuffer(&ImagePcx);

        // delete the PCX buffer
        pcxDelete(&ImagePcx);

        // install the keyboard driver
        keyboardInstallDriver();

        // enter main even loop
        while (!KeyboardState[MAKE_ESC]) {
            // to avoid video snow wait for vertical retrace
            waitForVerticalRetrace();

            // now test all the keys to see if they are pressed or released
            // based on this turn the virtual light that illuminates each
            // button on or off
            if (KeyboardState[MAKE_1]) {
                writeColorReg(START_NUMERIC_COLOR + 0, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 0, &offColor);
            }

            if (KeyboardState[MAKE_2]) {
                writeColorReg(START_NUMERIC_COLOR + 1, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 1, &offColor);
            }

            if (KeyboardState[MAKE_3]) {
                writeColorReg(START_NUMERIC_COLOR + 2, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 2, &offColor);
            }

            if (KeyboardState[MAKE_4]) {
                writeColorReg(START_NUMERIC_COLOR + 3, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 3, &offColor);
            }

            if (KeyboardState[MAKE_5]) {
                writeColorReg(START_NUMERIC_COLOR + 4, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 4, &offColor);
            }

            if (KeyboardState[MAKE_6]) {
                writeColorReg(START_NUMERIC_COLOR + 5, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 5, &offColor);
            }

            if (KeyboardState[MAKE_7]) {
                writeColorReg(START_NUMERIC_COLOR + 6, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 6, &offColor);
            }

            if (KeyboardState[MAKE_8]) {
                writeColorReg(START_NUMERIC_COLOR + 7, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 7, &offColor);
            }

            if (KeyboardState[MAKE_9]) {
                writeColorReg(START_NUMERIC_COLOR + 8, &onColor);
            } else {
                writeColorReg(START_NUMERIC_COLOR + 8, &offColor);
            }
        }

        // remove the keyboard driver
        keyboardRemoveDriver();

        // use a screen transition to exit
        screenTransition(SCREEN_WHITENESS);
    }

    // reset graphics to text mode
    setGraphicsMode(TEXT_MODE);
}
