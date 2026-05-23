// vblank.c - demo of the vertical blank interrupt supported by a few VGA cards

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

#define VERTICAL_BLANK_INTERRUPT    0x0A    // index of vblank in vector table
#define CRT_VERTICAL_END            0x11    // register with vblank enable bits

void (_interrupt _FAR* OldVertical)();

void _interrupt _far verticalBlank(void) {
    // this function is the vertical blank handler
    static int count = 0;   // used to count the vertical blanks
    char buffer[64];        // used to print a string
    unsigned char data;     // used to read data

    _asm sti;   // re-enable interrupts

    // reset latch on VGA
    outp(CRT_CONTROLLER, CRT_VERTICAL_END);
    data = inp(CRT_CONTROLLER + 1);
    data = RESET_BITS(data, 0x10);
    outp(CRT_CONTROLLER + 1, data);

    // do whatever you want here, but make it quick!
    sprintf(buffer, "Number of vertical blanks is %d", count++);
    printString(0, 0, 10, buffer, 1);

    // end process vertical blank

    // re-enable interrupts on PIC, send end of interrupt command EOI, 20h
    outp(PIC_ICR, PIC_EOI);
}

void main(void) {
    unsigned char data;

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // save old vertical blank interrupt
    OldVertical = _dos_getvect(VERTICAL_BLANK_INTERRUPT);

    // set new interrupt
    _dos_setvect(VERTICAL_BLANK_INTERRUPT, verticalBlank);

    // enable interrupt on VGA
    outp(CRT_CONTROLLER, CRT_VERTICAL_END);
    data = (unsigned char)inp(CRT_CONTROLLER + 1);

    data = RESET_BITS(data, 0x20);  // for your VGA you may need to SET this bit
    outp(CRT_CONTROLLER + 1, data);

    data = RESET_BITS(data, 0x10);
    outp(CRT_CONTROLLER + 1, data);

    // enable interrupt on PIC, i.e. enable IRQ 2
    data = (unsigned char)inp(PIC_IMR);
    data = RESET_BITS(data, 0x40);
    outp(PIC_IMR, data);

    while (!kbhit());

    // disable interrupts from VGA
    outp(CRT_CONTROLLER, CRT_VERTICAL_END);
    data = (unsigned char)inp(CRT_CONTROLLER + 1);

    data = SET_BITS(data, 0x20);    // for your VGA you may need to reset this bit
    outp(CRT_CONTROLLER + 1, data);

    // disable vertical blank interrupt on PIC
    data = (unsigned char)inp(PIC_IMR);
    data = SET_BITS(data, 0x04);
    outp(PIC_IMR, data);

    // restore old vector
    _dos_setvect(VERTICAL_BLANK_INTERRUPT, OldVertical);

    setGraphicsMode(TEXT_MODE);
}
