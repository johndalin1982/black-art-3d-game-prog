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
#include "black9.h"

void (_interrupt _far *OldSerialIsr)(); // holds old com port interrupt handler

char SerialBuffer[SERIAL_BUFF_SIZE];    // the receive buffer

int SerialEnd = -1;                     // indexes into receive buffer
int SerialStart = -1;
int SerialCh;
int CharReady = 0;                      // current character and ready flag
int OldIntMask;                         // the old interrupt mask on the PIC
int OpenPort;                           // the currently open port
int SerialLock = 0;                     // serial ISR semaphore so the buffer
                                        // isn't altered will it is being written
                                        // to by the ISR

char* ModemStrings[] = {
    "OK",           // these are the standard Hayes
    "CONNECT",      // response strings
    "RING",
    "NO CARRIER",
    "ERROR",
    "CONNECT 1200",
    "NO DIALTONE",
    "BUSY",
    "NO ANSWER",
    "CONNECT 0600",
    "CONNECT 2400",
    "CARRIER 2400", // experimental response strings
    "CONNECT 9600",
    "CONNECT 4800"
};

void _interrupt _far serialIsr(void) {
    // this is the serial ISR that is installed. It is called whenever a character
    // is received. The received character is then placed into the next position
    // in the input ring buffer.
    _asm sti

    SerialCh = inp(OpenPort + SERIAL_RBF);

    // wrap buffer index around
    if (++SerialEnd > SERIAL_BUFF_SIZE - 1) {
        SerialEnd = 0;
    }

    // move character into buffer
    SerialBuffer[SerialEnd] = SerialCh;

    ++CharReady;

    // restore PIC
    outp(PIC_ICR, 0x20);
}

int serialOpen(int portBase, int baud, int configuration) {
    // this function will open up the serial port, set its configuration, turn
    // on all the little flags and bits to make interrupts happen and load the ISR
    unsigned char data;

    // save the port I/O address for other functions
    OpenPort = portBase;

    // first set the baud rate
    // turn on divisor latch registers
    outp(portBase + SERIAL_LCR, SERIAL_DIV_LATCH_ON);

    // send low and high bytes to divisor latches
    outp(portBase + SERIAL_DLL, baud);
    outp(portBase + SERIAL_DLH, 0);

    // set the configuration for the port
    outp(portBase + SERIAL_LCR, configuration);

    // enable the interrupts
    data = inp(portBase + SERIAL_MCR);
    data = SET_BITS(data, SERIAL_GPO2);
    outp(portBase + SERIAL_MCR, data);
    outp(portBase + SERIAL_IER, 1);

    // hold off an enabling PIC until we have the ISR installed
    if (portBase == COM_1) {
        OldSerialIsr = _dos_getvect(INT_SERIAL_PORT_0);
        _dos_setvect(INT_SERIAL_PORT_0, serialIsr);
    } else {
        OldSerialIsr = _dos_getvect(INT_SERIAL_PORT_1);
        _dos_setvect(INT_SERIAL_PORT_1, serialIsr);
    }

    // enable the receive character interrupt on PIC for selected comm port
    OldIntMask = inp(PIC_IMR);

    outp(PIC_IMR, portBase == COM_1 ? OldIntMask & 0xEF : OldIntMask & 0xF7);

    return 1;
}

int serialReady(void) {
    // this function returns true if there are any characters waiting and 0
    // if the buffer is empty
    return CharReady;
}

int serialRead(void) {
    // this function reads a character from the circulating buffer and returns ir
    // to the caller, if there is no character waiting then the function returns 0
    int ch;

    // test if there is a character(s) ready in buffer
    if (SerialEnd != SerialStart) {
        // wrap buffer index if needed
        if (++SerialStart > SERIAL_BUFF_SIZE - 1) {
            SerialStart = 0;
        }

        // get the character out of buffer
        ch = SerialBuffer[SerialStart];

        // one less character in buffer now
        if (CharReady > 0) {
            --CharReady;
        }

        // send data back to caller
        return ch;
    } else {
        // buffer was empty return a NULL i.e. 0
        return 0;
    }
}


int serialReadWait(void) {
    // this function waits for a character to be ready and then returns it
    while (!serialReady());

    // return the character
    return serialRead();
}

void serialWrite(char ch) {
    // this function writes a character to the transmit buffer, but first it
    // waits for the transmit buffer to be empty. note: it is not interrupt
    // driven and it turns off interrupts while it's working

    // wait for transmit buffer to be empty
    while (!(inp(OpenPort + SERIAL_LSR) & 0x20)) {}

    // turn off interrupts for a bit
    _asm cli

    // send the character
    outp(OpenPort + SERIAL_THR, ch);

    // turn interrupts back on
    _asm sti
}

void serialPrint(char* string, int cr) {
    // this function is used to print a string to the serial port
    int index,
        length; // used for length of string

    length = strlen(string);

    // write each character
    for (index = 0; index < length; index++) {
        serialWrite(string[index]);
    }

    // send a carriage return if requested
    if (cr) {
        serialWrite(13);
    }
}

void serialFlush(void) {
    // this function flushes out the serial buffer
    int index;

    // read up to 32 characters
    for (index = 0; index < 32; index++) {
        serialRead();
        timeDelay(1);
    }
}

int serialClose(void) {
    // this function closes the port which entails turning off interrupts and
    // restoring the old interrupt vector
    unsigned char data;

    // disable the comm port interrupts
    data = inp(OpenPort + SERIAL_MCR);
    data = RESET_BITS(data, SERIAL_GPO2);

    outp(OpenPort + SERIAL_MCR, data);

    outp(OpenPort + SERIAL_IER, 0);
    outp(PIC_IMR, OldIntMask);

    // replace old comm port isr
    if (OpenPort == COM_1) {
        _dos_setvect(INT_SERIAL_PORT_0, OldSerialIsr);
    } else {
        _dos_setvect(INT_SERIAL_PORT_1, OldSerialIsr);
    }

    return 1;
}

void modemControl(int command) {
    // this function is used to control specific aspects of the modem hardware
    unsigned char data;

    // which command is being issued?
    switch (command) {
        case MODEM_DTR_ON:
        {
            // read modem control register
            data = inp(OpenPort + SERIAL_MCR);
            data = SET_BITS(data, 1);
            outp(OpenPort + SERIAL_MCR, data);
        } break;

        case MODEM_DTR_OFF:
        {
            // read modem control register
            data = inp(OpenPort + SERIAL_MCR);
            data = RESET_BITS(data, 1);
            outp(OpenPort + SERIAL_MCR, data);
        } break;

        default:
            break;
    }

    // wait a sec for it to take effect
    timeDelay(DELAY_1_SECOND);
}

int hangUp(void) {
    // this function hangs up the phone and places the modem back into its command state

    // drop dtr line
    modemControl(MODEM_DTR_OFF);

    return MODEM_OK;
}

void modemSendCommand(char* buffer) {
    // this function sends a command string to the modem
    int index,
        length; // length of command

    // write the string out
    length = strlen(buffer);

    for (index = 0; index < length; index++) {
        serialWrite(buffer[index]);
    }

    // uncomment the next line if you want to see what's being sent
    // printf("Sending:%s\n", buffer);

    // send a carriage return
    serialWrite(13);

    timeDelay(DELAY_1_SECOND);
}

int modemResult(char* output, int exitEnable) {
    // this function is a bit messy (as are all parsing functions), it is used
    // to retrieve a response from the modem, however, it will disregard any
    // echoed commands, also, the last parameter exitEnable is used as a flag
    // to allow the keyboard to force an exit, this is useful in a situation
    // such as waiting for an answer or waiting to be called, in any case, this
    // gives the function an exit avenue, the exit is enabled by pressing a key
    // on the keyboard, however, the keyboard handler must be installed for
    // this to work!
    int index = 0;
    char ch,
         buffer[64];

    // hunt for start of response
    while (1) {
        // is a character ready?
        if (serialReady()) {
            if (serialRead() == 10) {
                break;
            }
        }

        // test if user is trying to abort
        if (exitEnable && (kbhit() || KeysActive)) {
            return MODEM_USER_ABORT;
        }
    }

    // read the response
    while (1) {
        // is a character ready?
        if (serialReady()) {
            ch = serialRead();

            if (ch == 10) {
                break;
            }

            buffer[index++] = ch;
        }

        // test if user is trying to abort
        if (exitEnable && (kbhit() || KeysActive)) {
            return MODEM_USER_ABORT;
        }
    }

    // terminate response
    buffer[index - 1] = 0;

    // uncomment the next line if you want to see what's being received
    // printf("Received:%s\n", buffer);

    // copy the result into the output string
    if (output != NULL) {
        strcpy(output, buffer);
    }

    // Any "CONNECT..." line means we are online. Modems tack the negotiated line
    // speed onto the result code, and it varies wildly across hardware/emulators
    // (bare CONNECT, CONNECT 2400, CONNECT 9600, CONNECT 57600, CONNECT 115200,
    // ...). DOSBox-X always reports "CONNECT 57600". The speed is informational
    // only -- we drive the UART at our own DTE rate -- so collapse every CONNECT
    // variant to MODEM_CONNECT instead of exact-matching a fixed list of speeds.
    if (strncmp(buffer, "CONNECT", 7) == 0) {
        return MODEM_CONNECT;
    }

    // compute which response has been given
    for (index = 0; index < NUM_MODEM_RESPONSES; index++) {
        // test the response to next response string
        if (strcmp(ModemStrings[index], buffer) == 0) {
            return index;
        }
    }

    // there must be some kind of error
    return MODEM_ERROR;
}

int initializeModem(char* extraInit) {
    // this function will initialize the modem and prepare it for use
    // if your modem has some specific sequence that must be sent to reset
    // it then send it in the extra string, otherwise set extra equal to NULL
    int result;

    // send reset command
    modemSendCommand("ATZ");
    result = modemResult(NULL, 0);

    if (result != MODEM_OK) {
        return result;
    }

    // allow dtr line to be controlled, uncomment this if you want
    // the function to perform the DTR set, otherwise, leave it
    // commented and send AT&D2 in the extra string. This
    // is commented out because it causes problems with some external modems
    // modemSendCommand("AT&D2");
    // result = modemResult(NULL, 0);

    // if (result != MODEM_OK) {
    //     return result;
    // }

    // place modem in direct asynchronous mode
    modemSendCommand("AT&Q0");
    result = modemResult(NULL, 0);

    if (result != MODEM_OK) {
        return result;
    }

    // send hardware specific modem command
    if (extraInit && strlen(extraInit) >= 2) {
        // this is where the modem will be given most of its
        // correct initialization information
        modemSendCommand(extraInit);
        result = modemResult(NULL, 0);
    }

    return result;
}

int makeConnection(char* number) {
    // this function calls up the send phone number and returns true if the
    // connection was made or false if it wasn't
    int result;
    char command[64];

    // flush serial buffers
    serialFlush();

    // enable the DTR line
    modemControl(MODEM_DTR_ON);

    timeDelay(DELAY_1_SECOND);

    // dial number
    sprintf(command, "ATDT%s", number);

    // make the call
    modemSendCommand(command);
    result = modemResult(NULL, 1);

    // test the result
    if (result == MODEM_CONNECT ||
        result == MODEM_CONNECT_1200 ||
        result == MODEM_CONNECT_2400) {

        // a successful connection has been made
        return result;
    } else {
        // there must be a problem, hang up the phone
        hangUp();

        // return the error
        return result;
    }
}

int waitForConnection(void) {
    // this function will wait for a connection to be made and return true
    // when this occurs. the function will return false if a connection isn't
    // made or in a specific amount of time or if a key is pressed
    int result;

    // flush serial buffers
    serialFlush();

    // make sure modem is hung up
    hangUp();

    modemControl(MODEM_DTR_ON);

    // wait for phone to ring...
    result = modemResult(NULL, 1);

    // was that a ring?
    if (result == MODEM_RING) {
        // tell modem to answer
        modemSendCommand("ATA");
        result = modemResult(NULL, 1);

        // test the result
        if (result == MODEM_CONNECT ||
            result == MODEM_CONNECT_1200 ||
            result == MODEM_CONNECT_2400) {

            // a successful connection has been made
            return result;
        } else {
            // there must be a problem, hang up the phone
            hangUp();

            // return the error
            return result;
        }
    }

    // never saw a RING (timeout, stray response, or user abort) -- report it
    // rather than falling off the end with an undefined return value.
    return result;
}
