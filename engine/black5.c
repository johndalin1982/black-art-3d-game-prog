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
#include "black5.h"

// defines for the keyboard interface
#define KEYBOARD_INTERRUPT  0x09    // the keyboard interrupt number
#define KEY_BUFFER          0x60    // the port of the keyboard buffer
#define KEY_CONTROL         0x61    // the port of the keyboard controller
#define PIC_PORT            0x20    // the port of the peripheral interrupt controller (PIC)

// defines for the joystick interface
#define JOYPORT             0x201   // joyport is at 201 hex

// defines for mouse interface
#define MOUSE_INTERRUPT         0x33    // mouse interrupt number

void (_interrupt _FAR* OldKeyboardISR)();   // holds old keyboard interrupt handler

int RawScanCode = 0;  // the global keyboard scan code

// this holds the keyboard state table which tracks the state of every key
// on the keyboard
int KeyboardState[128];

// this tracks the number of keys that are currently pressed, helps
// with keyboard testing logic
int KeysActive = 0;

// these values hold the maximum, minimum and neutral joystick values for
// both joysticks
unsigned int Joystick1MaxX,     // maximum deflection of X axis joy 1
             Joystick1MaxY,     // maximum deflection of Y axis joy 1
             Joystick1MinX,     // minimum deflection of X axis joy 1
             Joystick1MinY,     // minimum deflection of Y axis joy 1
             Joystick1NeutralX, // neutral or center of X axis joy 1
             Joystick1NeutralY, // neutral or center of Y axis joy 1

             Joystick2MaxX,     // maximum deflection of X axis joy 2
             Joystick2MaxY,     // maximum deflection of Y axis joy 2
             Joystick2MinX,     // minimum deflection of X axis joy 2
             Joystick2MinY,     // minimum deflection of Y axis joy 2
             Joystick2NeutralX, // neutral or center of X axis joy 2
             Joystick2NeutralY; // neutral or center of Y axis joy 2

void _interrupt _far keyboardDriver() {
    // this function is used as the new keyboard driver. once it is installed
    // it will continuously update a keyboard state table that has an entry for
    // every key on the keyboard, when a key is down the appropriate entry will be
    // set to 1, when released the entry will be reset. any key can be queried by
    // accessing the KeyboardState[] table with the make code of the key as the
    // index

    // need to use assembly for speed since this is an interrupt
#ifdef DOS_32_BIT
    _asm {
        sti                         ; re-enable interrupts
        in al, KEY_BUFFER           ; get the key from keyboard buffer (port 0x60)
        movzx eax, al               ; zero-extend AL to EAX (32-bit value)
        mov RawScanCode, eax        ; store the scan code in 32-bit global variable
        in al, KEY_CONTROL          ; read keyboard control register (port 0x61)
        or al, 82h                  ; set bits to reset keyboard flip flop
        out KEY_CONTROL, al         ; write back to control register
        and al, 7fh                 ; mask off high bit
        out KEY_CONTROL, al         ; complete the reset
        mov al, 20h                 ; EOI (End of Interrupt) command
        out PIC_PORT, al            ; send EOI to PIC (port 0x20)
    }
#else
    _asm {
        sti                     ; re-enable interrupts
        in al,KEY_BUFFER        ; get the key that was pressed from the keyboard
        xor ah,ah               ; zero out upper 8 bits of AX
        mov RawScanCode,ax      ; store the key in global variable
        in al,KEY_CONTROL       ; set the control register to reflect key was read
        or al,82h               ; set the proper bits to reset the keyboard flip flop
        out KEY_CONTROL,al      ; send the new data back to the control register
        and al,7fh              ; mask off high bit
        out KEY_CONTROL,al      ; complete the reset
        mov al,20h              ; reset command
        out PIC_PORT,al         ; tell PIC to re-enable interrupts
    }
#endif

    // update the keyboard table

    // test if the scan code was a make code or a break code
    if (RawScanCode < 128) {
        // index into table and set this key to the "on" state if it already isn't on
        if (KeyboardState[RawScanCode] == KEY_UP) {
            // there is one more active key
            KeysActive++;

            // update the state table
            KeyboardState[RawScanCode] = KEY_DOWN;
        }
    } else {
        // must be a break code, therefore turn the key "off"
        // note that 128 must be subtracted from the raw key to access the correct
        // element of the state table
        if (KeyboardState[RawScanCode - 128] == KEY_DOWN) {
             // there is one less active key
             KeysActive--;

             // update the state table
             KeyboardState[RawScanCode - 128] = KEY_UP;
        }
    }
}

unsigned char getKey(void) {
    // test if a key press has been buffered by system and if so, return the ASCII
    // value of the key, otherwise return 0
    if (_bios_keybrd(_KEYBRD_READY)) {
        return (unsigned char)_bios_keybrd(_KEYBRD_READ);
    } else {
        return 0;
    }
}

unsigned char getScanCode(void) {
    // use BIOS functions to retrieve the scan code of the last pressed key
    // if a key was pressed at all, otherwise return 0
    _asm {
        mov ah,01h      ; function #1 which is "key ready"
        int 16h         ; call the BIOS keyboard interrupt
        jz BufferEmpty  ; if there was no key ready then exit
        mov ah,00h      ; function #0: retrieve raw scan code
        int 16h         ; call the BIOS keyboard interrupt
        mov al,ah       ; result is placed by BIOS in ah, copy it to al
        xor ah,ah       ; zero out ah
        jmp Done        ; jump to end so ax doesn't get reset
    BufferEmpty:
        xor ax,ax       ; a key was retrieved so write a 0 into ax to reflect this
    Done:
    }

    // 8 or 16 bit data is returned in AX, hence no need to explicitly say return()
}

unsigned int getShiftState(unsigned int mask) {
    // return the shift state of the keyboard logically ANDed with the sent mask
    return mask & _bios_keybrd(_KEYBRD_SHIFTSTATUS);
}

void keyboardInstallDriver(void) {
    // this function installs the new keyboard driver
    int index;

    // clear out keyboard state table
    for (index = 0; index < 128; index++) {
        KeyboardState[index] = 0;
    }

    // save the old keyboard driver
    OldKeyboardISR = _dos_getvect(KEYBOARD_INTERRUPT);

    // install the new keyboard driver
    _dos_setvect(KEYBOARD_INTERRUPT, keyboardDriver);
}

void keyboardRemoveDriver(void) {
    // this function restores the old keyboard driver (DOS version) with the
    // previously saved vector
    _dos_setvect(KEYBOARD_INTERRUPT, OldKeyboardISR);
}

unsigned char joystickButtons(unsigned char button) {
    // this function reads the state of the joystick buttons by retrieving the
    // appropriate bit in the joystick port
    outp(JOYPORT, 0);   // clear the joystick port and request a sample

    // invert buttons then mask with request so that a button that is pressed
    // returns a "1" instead of a "0"
    return (unsigned char)(~inp(JOYPORT) & button);
}

unsigned int joystick(unsigned char stick) {
    // this function will read a joystick by starting the timing circuits connected
    // to each joystick port, when the timing circuit has charged the joystick
    // bit will revert to 0, the time this process takes is proportional to
    // the joystick position and is returned as the result
#ifdef DOS_32_BIT
    _asm {
        cli                         ; disable interrupts for timing
        mov ah, byte ptr stick      ; select joystick with bitmask (8-bit)
        xor al, al                  ; zero out al
        xor ecx, ecx                ; clear ecx (32-bit loop counter)
        mov dx, JOYPORT             ; point dx to joystick port
        out dx, al                  ; start the 555 timers charging
    charged:
        in al, dx                   ; read the joystick port
        test al, ah                 ; test if bit has reverted to 0
        loopne charged              ; loopne uses ECX in 32-bit mode
        xor eax, eax                ; zero out eax
        sub eax, ecx                ; subtract ecx to get positive loop count
        sti                         ; re-enable interrupts
    }
#else
    _asm {
        cli                     ; disable interrupts for timing purposes
        mov ah,byte ptr stick   ; select joystick to read with bitmask
        xor al,al               ; zero out al
        xor cx,cx               ; clear cx i.e. set it to 0
        mov dx,JOYPORT          ; point dx to the joystick port
        out dx,al               ; start the 555 timers charging
    charged:
        in al,dx                ; read the joystick port and test if the bit
        test al,ah              ; has reverted back to 0
        loopne charged          ; if the joystick circuit isn't charged then
                                ; decrement cx and loop

        xor ax,ax               ; zero out ax
        sub ax,cx               ; subtract cx from ax to get a number that increases
                                ; as the joystick position is moved away from neutral

        sti                     ; re-enable interrupts
    }
#endif

    // ax has the 16 result, so no need for an explicit return
}

#ifdef DOS_32_BIT
unsigned int joystickBios(unsigned char stick) {
    // read the joystick using bios interrupt 15h with the joystick function 84h
    union REGS inregs, outregs;     // used to hold CPU registers

    inregs.h.ah = 0x84;             // joystick function 84h
    inregs.x.edx = 0x01;            // read joysticks subfunction 01h

    // call the BIOS joystick interrupt
    int386(0x15, &inregs, &outregs);

    // return requested joystick
    switch (stick) {
        case JOYSTICK_1_X:  // eax has joystick 1's X axis
            return outregs.x.eax;
            break;

        case JOYSTICK_1_Y:  // ebx has joystick 1's Y axis
            return outregs.x.ebx;
            break;

        case JOYSTICK_2_X:  // ecx has joystick 2's X axis
            return outregs.x.ecx;
            break;

        case JOYSTICK_2_Y:  // edx has joystick 2's Y axis
            return outregs.x.edx;
            break;

        default:
            break;
    }

    return 0;
}
#else
unsigned int joystickBios(unsigned char stick) {
    // read the joystick using bios interrupt 15h with the joystick function 84h
    union _REGS inregs, outregs;    // used to hold CPU registers

    inregs.h.ah = 0x84;  // joystick function 84h
    inregs.x.dx = 0x01;  // read joysticks subfunction 01h

    // call the BIOS joystick interrupt
    int86(0x15, &inregs, &outregs);

    // return requested joystick
    switch (stick) {
        case JOYSTICK_1_X:  // ax has joystick 1's X axis
            return outregs.x.ax;
            break;

        case JOYSTICK_1_Y:  // bx has joystick 1's Y axis
            return outregs.x.bx;
            break;

        case JOYSTICK_2_X:  // cx has joystick 2's X axis
            return outregs.x.cx;
            break;

        case JOYSTICK_2_Y:  // dx has joystick 2's Y axis
            return outregs.x.dx;
            break;

        default:
            break;
    }

    return 0;
}
#endif

void joystickCalibrate(int stick, int method) {
    // this function is used to calibrate a joystick. the function will
    // query the user to move the joystick in circular motion and then release
    // the stick back to the neutral position and press the fire button. using
    // this information the function will compute the max,min and neutral
    // values for both the X and Y axis of the joystick. these values will
    // then be stored in global variables so they can be used by other
    // functions, note the function can use either the BIOS joystick call
    // or the low level one we made
    unsigned int xValue,    // used to read values of X and Y axis in real-time
                 yValue;

    // which stick does caller want to calibrate?
    if (stick == JOYSTICK_1) {
        printf("Calibrating Joystick #1: Move the joystick in a circle, then\n");
        printf("place the stick into its neutral position and press fire.\n");

        // set calibration values to extremes
        Joystick1MaxX = 0;
        Joystick1MaxY = 0;
        Joystick1MinX = 32000;
        Joystick1MinY = 32000;

        // process X and Y values in real time
        while (!joystickButtons(JOYSTICK_BUTTON_1_1 | JOYSTICK_BUTTON_1_2)) {
            // get the new values and try to update calibration

            // test if user wants to use bios or low level
            if (method == USE_BIOS) {
                xValue = joystickBios(JOYSTICK_1_X);
                yValue = joystickBios(JOYSTICK_1_Y);
            } else {
                xValue = joystick(JOYSTICK_1_X);
                yValue = joystick(JOYSTICK_1_Y);
            }

            // update globals with new extremes

            // process X - axis
            if (xValue >= Joystick1MaxX) {
                Joystick1MaxX = xValue;
            }

            if (xValue <= Joystick1MinX) {
                Joystick1MinX = xValue;
            }

            // process Y - axis
            if (yValue >= Joystick1MaxY) {
                Joystick1MaxY = yValue;
            }

            if (yValue <= Joystick1MinY) {
                Joystick1MinY = yValue;
            }

            //printf("x value = %d, y value = %d\n", xValue, yValue);
        }

        // stick is now in neutral position so record the values here also
        Joystick1NeutralX = xValue;
        Joystick1NeutralY = yValue;

        // notify user process is done
        printf("Joystick #1 Calibrated. Press the fire button to exit.\n");

        while (joystickButtons(JOYSTICK_BUTTON_1_1 | JOYSTICK_BUTTON_1_2));
        while (!joystickButtons(JOYSTICK_BUTTON_1_1 | JOYSTICK_BUTTON_1_2));
    } else if (stick == JOYSTICK_2) {
        printf("Calibrating Joystick #2: Move the joystick in a circle, then\n");
        printf("place the stick into its neutral position and press fire.\n");

        // set calibration values to extremes
        Joystick2MaxX = 0;
        Joystick2MaxY = 0;
        Joystick2MinX = 32000;
        Joystick2MinY = 32000;

        // process X and Y values in real time
        while (!joystickButtons(JOYSTICK_BUTTON_2_1 | JOYSTICK_BUTTON_2_2)) {
            // get the new values and try to update calibration

            // test if user wants to use bios or low level
            if (method == USE_BIOS) {
                xValue = joystickBios(JOYSTICK_2_X);
                yValue = joystickBios(JOYSTICK_2_Y);
            } else {
                xValue = joystick(JOYSTICK_2_X);
                yValue = joystick(JOYSTICK_2_Y);
            }

            // update globals with new extremes

            // process X - axis
            if (xValue >= Joystick2MaxX) {
                Joystick2MaxX = xValue;
            }

            if (xValue <= Joystick2MinX) {
                Joystick2MinX = xValue;
            }

            // process Y - axis
            if (yValue >= Joystick2MaxY) {
                Joystick2MaxY = yValue;
            }

            if (yValue <= Joystick2MinY) {
                Joystick2MinY = yValue;
            }

            //printf("x value = %d, y value = %d\n", xValue, yValue);
        }

        // stick is now in neutral position so record the values here also
        Joystick2NeutralX = xValue;
        Joystick2NeutralY = yValue;

        // notify user process is done
        printf("Joystick #2 Calibrated. Press the fire button to exit.\n");

        while (joystickButtons(JOYSTICK_BUTTON_2_1 | JOYSTICK_BUTTON_2_2));
        while (!joystickButtons(JOYSTICK_BUTTON_2_1 | JOYSTICK_BUTTON_2_2));
    }
}

int joystickAvailable(int stickNum) {
    // test if the joystick that the user is requesting is plugged in
    // note the use of the BIOS joystick function, it is very reliable
    if (stickNum == JOYSTICK_1) {
        // test if joystick 1 is plugged in by testing the port values
        // they will be 0,0 if there is no stick
        return joystickBios(JOYSTICK_1_X) + joystickBios(JOYSTICK_1_Y);
    } else {
        // test if joystick 2 is plugged in by testing the port values
        // they will be 0x0 if there is no stick
        return joystickBios(JOYSTICK_2_X) + joystickBios(JOYSTICK_2_Y);
    }
}

#ifdef DOS_32_BIT
int mouseControl(int command, int* x, int* y, int* buttons) {
    union REGS inregs,      // CPU register unions to be used by interrupts
               outregs;

    // what is caller asking function to do?
    switch (command) {
        case MOUSE_RESET: // this resets the mouse
        {
            // mouse subfunction 0: reset
            inregs.x.eax = 0x00;

            // call the mouse interrupt
            int386(MOUSE_INTERRUPT, &inregs, &outregs);

            // return number of buttons on this mouse
            // (see MOUSE_POSITION_BUTTONS for the 16-bit mask rationale)
            *buttons = outregs.x.ebx & 0xFFFF;

            // return success/failure of function
            return outregs.x.eax & 0xFFFF;
        }
        break;

        case MOUSE_SHOW: // this shows the mouse
        {
            // this function increments the internal mouse visibility counter.
            // when it is equal to 0 then the mouse will be displayed.

            // mouse subfunction 1: increment show flag
            inregs.x.eax = 0x01;

            // call the mouse interrupt
            int386(MOUSE_INTERRUPT, &inregs, &outregs);

            // return success always
            return 1;
        }
        break;

        case MOUSE_HIDE: // this hides the mouse
        {
            // this function decrements the internal mouse visibility counter.
            // when it is equal to -1 then the mouse will be hidden.

            // mouse subfunction 2: decrement show flag
            inregs.x.eax = 0x02;

            // call the interrupt
            int386(MOUSE_INTERRUPT, &inregs, &outregs);

            // return success
            return 1;
        }
        break;

        case MOUSE_POSITION_BUTTONS: // this gets both the position and
                                     // state of buttons
        {
            // this function computes the absolute position of the mouse
            // and the state of the mouse buttons

            // mouse subfunction 3: get position and buttons
            inregs.x.eax = 0x03;

            // call the mouse interrupt
            int386(MOUSE_INTERRUPT, &inregs, &outregs);

            // extract the info and send back to caller via pointers
            // mask to 16 bits — real-mode int 33h only sets the low half of
            // each 32-bit register and the upper half leaks uninitialized
            // stack garbage from inregs through the int386 translation
            *x = outregs.x.ecx & 0xFFFF;
            *y = outregs.x.edx & 0xFFFF;
            *buttons = outregs.x.ebx & 0xFFFF;

            // return success always
            return 1;
        }
        break;

        case MOUSE_MOTION_REL: // this gets the relative motion of mouse
        {
            // this function gets the relative mouse motions from the last
            // call, these values will range from -32768 to +32767 and
            // be in mickeys which are 1/200 of inch or 1/400 of inch
            // depending on the resolution of your mouse

            // subfunction 11: get relative motion
            inregs.x.eax = 0x0B;

            // call the interrupt
            int386(MOUSE_INTERRUPT, &inregs, &outregs);

            // extract the info and send back to caller via pointers
            // (see MOUSE_POSITION_BUTTONS for the 16-bit mask rationale)
            *x = outregs.x.ecx & 0xFFFF;
            *y = outregs.x.edx & 0xFFFF;

            // return success
            return 1;
        }
        break;

        case MOUSE_SET_SENSITIVITY:
        {
            // subfunction 26: set sensitivity
            inregs.x.eax = 0x1A;

            // place the desired sensitivity and double speed values in place
            inregs.x.ebx = *x;
            inregs.x.ecx = *y;
            inregs.x.edx = *buttons;

            // call the interrupt
            int386(MOUSE_INTERRUPT, &inregs, &outregs);

            // always return success
            return 1;
        }
        break;

        default:
            break;
    }
}
#else
int mouseControl(int command, int* x, int* y, int* buttons) {
    union _REGS inregs,     // CPU register unions to be used by interrupts
                outregs;

    // what is caller asking function to do?
    switch (command) {
        case MOUSE_RESET: // this resets the mouse
        {
            // mouse subfunction 0: reset
            inregs.x.ax = 0x00;

            // call the mouse interrupt
            int86(MOUSE_INTERRUPT, &inregs, &outregs);

            // return number of buttons on this mouse
            *buttons = outregs.x.bx;

            // return success/failure of function
            return outregs.x.ax;
        }
        break;

        case MOUSE_SHOW: // this shows the mouse
        {
            // this function increments the internal mouse visibility counter.
            // when it is equal to 0 then the mouse will be displayed.
            // mouse subfunction 1: increment show flag
            inregs.x.ax = 0x01;

            // call the mouse interrupt
            int86(MOUSE_INTERRUPT, &inregs, &outregs);

            // return success always
            return 1;
        }
        break;

        case MOUSE_HIDE: // this hides the mouse
        {
            // this function decrements the internal mouse visibility counter.
            // when it is equal to -1 then the mouse will be hidden.

            // mouse subfunction 2: decrement show flag
            inregs.x.ax = 0x02;

            // call the interrupt
            int86(MOUSE_INTERRUPT, &inregs, &outregs);

            // return success
            return 1;
        }
        break;

        case MOUSE_POSITION_BUTTONS: // this gets both the position and
                                     // state of buttons
        {
            // this function computes the absolute position of the mouse
            // and the state of the mouse buttons

            // mouse subfunction 3: get position and buttons
            inregs.x.ax = 0x03;

            // call the mouse interrupt
            int86(MOUSE_INTERRUPT, &inregs, &outregs);

            // extract the info and send back to caller via pointers
            *x = outregs.x.cx;
            *y = outregs.x.dx;
            *buttons = outregs.x.bx;

            // return success always
            return 1;
        }
        break;

        case MOUSE_MOTION_REL: // this gets the relative motion of mouse
        {
            // this function gets the relative mouse motions from the last
            // call, these values will range from -32768 to +32767 and
            // be in mickeys which are 1/200 of inch or 1/400 of inch
            // depending on the resolution of your mouse

            // subfunction 11: get relative motion
            inregs.x.ax = 0x0B;

            // call the interrupt
            int86(MOUSE_INTERRUPT, &inregs, &outregs);

            // extract the info and send back to caller via pointers
            *x = outregs.x.cx;
            *y = outregs.x.dx;

            // return success
            return 1;
        }
        break;

        case MOUSE_SET_SENSITIVITY:
        {
            // subfunction 26: set sensitivity
            inregs.x.ax = 0x1A;

            // place the desired sensitivity and double speed values in place
            inregs.x.bx = *x;
            inregs.x.cx = *y;
            inregs.x.dx = *buttons;

            // call the interrupt
            int86(MOUSE_INTERRUPT, &inregs, &outregs);

            // always return success
            return 1;
        }
        break;

        default:
            break;
    }
}
#endif

