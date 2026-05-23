// term1.c - A simple serial communications program.

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

#include "black9.h"
#include "black3.h"

void main(void) {
    char ch;        // input character
    int done = 0,   // global exit flag
    comPort;        // the com port to open

    printf("Serial Communications Program Version 1.0\n\n\n");

    // ask the user for the com port
    printf("Which COM port is your modem attached to 1 or 2?");
    scanf("%d", &comPort);

    // depending on user's selection open COM 1 or 2
    if (comPort == 1) {
        serialOpen(
            COM_1,
            SERIAL_BAUD_2400,
            SERIAL_PARITY_NONE | SERIAL_BITS_8 | SERIAL_STOP_1);
    } else {
        serialOpen(
            COM_2,
            SERIAL_BAUD_2400,
            SERIAL_PARITY_NONE | SERIAL_BITS_8 | SERIAL_STOP_1);
    }

    printf("COM Port %d was selected\n", comPort);

    // main loop
    // flush serial port
    serialFlush();

    // enter event loop
    while (!done) {
        // try and get a character from local machine
        if (kbhit()) {
            // get the character from keyboard
            ch = getch();
            printf("%c", ch);
            fflush(stdout);

            // send the character to other machine
            serialWrite(ch);

            // has user pressed ESC? if so, bail.
            if (ch == 27) {
                serialFlush();
                done = 1;
            }

            // test for CR, if so add a line feed
            if (ch == 13) {
                printf("\n");
            }
        }

        // try and get a character from remote
        if (ch = serialRead()) {
            // print the character to the screen
            printf("%c", ch);

            // if it's a carriage return add a line feed
            if (ch == 13) {
                printf("\n");
            }

            // if an ESC character is sent from remote then close down
            if (ch == 27) {
                printf("Remote Machine Closing Connection.\n");
                serialFlush();
                done = 1;
            }
        }
    }

    // close the connection and blaze
    serialClose();
}
