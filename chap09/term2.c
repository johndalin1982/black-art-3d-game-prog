// term2.c - A menu driven modem terminal program.

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
    char ch,            // input character
         number[64];    // the phone number that is dialed
    int done = 0,       // global exit flag
        sel,            // user menu input
        linked = 0,     // has a connection been made
        result,         // result of modem commands
        comPort;        // the com port to open

    printf("Modem Terminal Communications Program Version 2.0\n\n");

    // ask the user for the com port
    printf("Which COM port is your modem attached to 1 or 2?");
    scanf("%d", &comPort);

    // depending on users selection open COM 1 or 2
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

    // flush modem and serial port
    serialFlush();

    initializeModem("AT&D2");   // make this NULL if you have a problem with an external modem
    printf("\n");

    // enter event loop
    while (!done) {
        // if a connection hasn't been made then enter into menu
        if (!linked) {
            printf("Main Menu\n");
            printf("1 - Make a Call\n");
            printf("2 - Wait for Call\n");
            printf("3 - Hand Up\n");
            printf("4 - Exit\n");
            printf("\nSelect?");

            // get input from user
            scanf("%d", &sel);

            // what is user trying to do
            switch (sel) {
                case 1: // make a call
                {
                    printf("\nNumber to dial?");
                    scanf("%s", number);

                    // try and make the connection
                    result = makeConnection(number);

                    // was the response code a connect message
                    if (result == MODEM_CONNECT_1200 ||
                        result == MODEM_CONNECT_2400 ||
                        result == MODEM_CONNECT ||
                        result == MODEM_CARRIER_2400) {

                        linked = 1;
                        printf("\nEntering Terminal Mode...\n");
                    } else if (result == MODEM_USER_ABORT) {
                        printf("\nUser Aborted!\n");
                        hangUp();
                        serialFlush();
                    }
                } break;

                case 2: // wait for call
                {
                    printf("\nWaiting...\n");

                    // wait for a call
                    result = waitForConnection();

                    // was the response code a connection
                    if (result == MODEM_CONNECT_1200 ||
                        result == MODEM_CONNECT_2400 ||
                        result == MODEM_CONNECT ||
                        result == MODEM_CARRIER_2400) {

                        linked = 1;
                        printf("\nEntering Terminal Mode...\n");
                    } else if (result == MODEM_USER_ABORT) {
                        printf("\nUser Aborted!\n");
                        hangUp();
                        serialFlush();
                    }
                } break;

                case 3: // hang up
                {
                    // drop DTR and flush serial buffer
                    hangUp();
                    serialFlush();
                } break;

                case 4: // exit
                {
                    // set global exit flag
                    done = 1;
                } break;

                default:
                    break;
            }
        }

        // once machines are linked, allow bi-directional communication
        while (linked == 1) {
            // try and get a character from local machine
            if (kbhit()) {
                // get the character from keyboard
                ch = getch();

                printf("%c", ch);

                // send the character to other machine
                serialWrite(ch);

                // has user pressed ESC ? if so, bail
                if (ch == 27) {
                    linked = 0;
                    hangUp();
                    serialFlush();
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

                // if an esc character is sent from remote then close down
                if (ch == 27) {
                    printf("\nRemote Machine Closing Connection.\n");
                    linked = 0;
                    hangUp();
                    serialFlush();
                }
            }
        }        
    }

    // close the connection and blaze

    // break connection just in case
    hangUp();
    serialClose();
}
