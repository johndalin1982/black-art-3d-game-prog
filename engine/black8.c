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
#include "black8.h"

long StartingTime,  // these are used to compute the length of some event
     EndingTime;

void timerProgram(int timer, unsigned int rate) {
    // this function re-programs the internal timer

    // first program the timer to mode 2 - binary and data loading sequence of
    // low byte then high byte
    outp(TIMER_CONTROL, TIMER_SET_BITS);

    // write least significant byte of the new rate to the counter register
    outp(timer, LOW_BYTE(rate));

    // and now the most significant byte
    outp(timer, HI_BYTE(rate));
}

long timerQuery(void) {
    // this function is used to record the current time

    // address of timer
#ifdef DOS_32_BIT
    long FAR* clock = (long FAR*)0x0000046C;
#else
    long FAR* clock = (long FAR*)0x0000046CL;
#endif

    return (*clock);
}
