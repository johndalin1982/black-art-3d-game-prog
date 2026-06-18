#include "dpmi.h"

#include <i86.h>
#include <string.h>

int dpmiRealModeInt(int intNumber, DpmiRealModeRegsPtr regs) {
    union REGS  r;
    struct SREGS s;

    memset(&s, 0, sizeof(s));
    segread(&s);

    r.x.eax = 0x0300;                              // simulate real-mode interrupt
    r.x.ebx = (unsigned int)(intNumber & 0xFF);    // BL = INT number, BH = flags (0 = normal)
    r.x.ecx = 0;                                   // no PM->RM stack words to copy
    r.x.edi = (unsigned int)regs;                  // ES:EDI -> real-mode register frame
    s.es    = s.ds;                                // flat model: ES = data selector

    int386x(0x31, &r, &r, &s);

    return (r.x.cflag != 0);
}

int dpmiCallRealFar(DpmiRealModeRegsPtr regs) {
    union REGS   r;
    struct SREGS s;

    memset(&s, 0, sizeof(s));
    segread(&s);

    r.x.eax = 0x0301;                              // call real-mode procedure (far return frame)
    r.x.ebx = 0;                                   // BH = flags (0)
    r.x.ecx = 0;                                   // no PM->RM stack words to copy
    r.x.edi = (unsigned int)regs;                  // ES:EDI -> real-mode register frame
    s.es    = s.ds;                                // flat model: ES = data selector

    int386x(0x31, &r, &r, &s);

    return (r.x.cflag != 0);
}

int dpmiAllocDos(unsigned int paragraphs, unsigned short* segment, unsigned short* selector) {
    union REGS r;

    r.x.eax = 0x0100;                              // allocate DOS memory block
    r.x.ebx = paragraphs;

    int386(0x31, &r, &r);

    if (r.x.cflag) {
        return 0;
    }

    *segment  = r.w.ax;                            // real-mode segment
    *selector = r.w.dx;                            // PM selector covering the same memory

    return 1;
}

void dpmiFreeDos(unsigned short selector) {
    union REGS r;

    r.x.eax = 0x0101;                              // free DOS memory block
    r.x.edx = selector;

    int386(0x31, &r, &r);
}

int dpmiVectorInstalled(int intNumber) {
    unsigned short seg;
    unsigned short off;

    if (!dpmiGetVector(intNumber, &seg, &off)) {
        return 0;
    }

    return !(seg == 0 && off == 0);
}

int dpmiGetVector(int intNumber, unsigned short* segment, unsigned short* offset) {
    union REGS r;

    r.x.eax = 0x0200;                              // get real-mode interrupt vector
    r.x.ebx = (unsigned int)(intNumber & 0xFF);

    int386(0x31, &r, &r);

    if (r.x.cflag) {
        return 0;
    }

    *segment = r.w.cx;                             // CX = real-mode segment
    *offset  = r.w.dx;                             // DX = real-mode offset
    return 1;
}

int dpmiAllocRealCallback(
    void (*pmCallback)(void),
    DpmiRealModeRegsPtr rmRegs,
    unsigned short* cbSegment,
    unsigned short* cbOffset
) {
    union REGS   r;
    struct SREGS s;

    memset(&s, 0, sizeof(s));
    segread(&s);

    r.x.eax = 0x0303;                              // allocate real-mode callback
    r.x.esi = (unsigned int)pmCallback;            // DS:ESI -> PM callback entry
    r.x.edi = (unsigned int)rmRegs;                // ES:EDI -> real-mode register frame
    // DS:ESI must use the CODE selector: the DPMI host loads it into CS:EIP to
    // invoke the callback, so a (non-executable) DATA selector here faults with
    // "JMP illegal descriptor" the first time the driver calls back. segread()
    // leaves ES = the data selector, which is correct for the ES:EDI frame.
    s.ds    = s.cs;                                // procedure lives in the code segment

    int386x(0x31, &r, &r, &s);

    if (r.x.cflag) {
        return 0;
    }

    *cbSegment = r.w.cx;                           // CX:DX = real-mode entry point
    *cbOffset  = r.w.dx;
    return 1;
}

void dpmiFreeRealCallback(unsigned short cbSegment, unsigned short cbOffset) {
    union REGS r;

    r.x.eax = 0x0304;                              // free real-mode callback
    r.x.ecx = cbSegment;
    r.x.edx = cbOffset;

    int386(0x31, &r, &r);
}
