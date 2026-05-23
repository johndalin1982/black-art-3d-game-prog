// timer.c - A demo of reprogramming the PC's internal timer

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

#include "black8.h"

void main(void) {
    int done = 0,   // exit flag
        selection;  // user input variable

    // main event loop
    while (!done) {
        // display menu
        printf("PC Timer Re-Programming Utility\n");
        printf("1 - Program timer to 120 Hz.\n");
        printf("2 - Program timer to 100 Hz.\n");
        printf("3 - Program timer to 60 Hz.\n");
        printf("4 - Program timer to 50 Hz.\n");
        printf("5 - Program timer to 40 Hz.\n");
        printf("6 - Program timer to 30 Hz.\n");
        printf("7 - Program timer to 20 Hz.\n");
        printf("8 - Program timer to 18 Hz.\n");
        printf("9 - Exit program.\n");
        printf("\nSelect one?");

        // get input
        scanf("%d", &selection);

        // what rate did user select?
        switch (selection) {
            case 1: // set timer to 120 Hz
            {
                timerProgram(TIMER_COUNTER_0, TIMER_120HZ);
            } break;

            case 2: // set timer to 100 Hz
            {
                timerProgram(TIMER_COUNTER_0, TIMER_100HZ);
            } break;

            case 3: // set timer to 60 Hz
            {
                timerProgram(TIMER_COUNTER_0, TIMER_60HZ);
            } break;

            case 4: // set timer to 50 Hz
            {
                timerProgram(TIMER_COUNTER_0, TIMER_50HZ);
            } break;

            case 5: // set timer to 40 Hz
            {
                timerProgram(TIMER_COUNTER_0, TIMER_40HZ);
            } break;

            case 6: // set timer to 30 Hz
            {
                timerProgram(TIMER_COUNTER_0, TIMER_30HZ);
            } break;

            case 7: // set timer to 20 Hz
            {
                timerProgram(TIMER_COUNTER_0, TIMER_20HZ);
            } break;

            case 8: // set timer to 18 Hz
            {
                timerProgram(TIMER_COUNTER_0, TIMER_18HZ);
            } break;

            case 9: // exit program
            {
                done = 1;
            } break;

            default:
            {
                printf("\nInvalid Selection!\n");
            } break;
        }
    }
}
