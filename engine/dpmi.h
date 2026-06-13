#ifndef DPMI_H
#define DPMI_H

// DPMI real-mode register frame used by INT 31h function 0300h
// (simulate real-mode interrupt). Field order and packing fixed by the DPMI spec.
#pragma pack(push, 1)
typedef struct DpmiRealModeRegsType {
    unsigned int   edi;
    unsigned int   esi;
    unsigned int   ebp;
    unsigned int   reserved;
    unsigned int   ebx;
    unsigned int   edx;
    unsigned int   ecx;
    unsigned int   eax;
    unsigned short flags;
    unsigned short es;
    unsigned short ds;
    unsigned short fs;
    unsigned short gs;
    unsigned short ip;
    unsigned short cs;
    unsigned short sp;
    unsigned short ss;
} DpmiRealModeRegs, *DpmiRealModeRegsPtr;
#pragma pack(pop)

// Issue a real-mode interrupt from protected mode (DPMI INT 31h func 0300h).
// Returns 0 on success, nonzero on DPMI host error.
int dpmiRealModeInt(int intNumber, DpmiRealModeRegsPtr regs);

// Allocate a block of DOS conventional memory (< 1 MB), accessible from both
// real mode (via *segment) and protected mode (via *selector). The flat linear
// address of the block equals (*segment << 4) under DOS/4GW.
// paragraphs = size in 16-byte paragraphs.
// Returns 1 on success, 0 on failure.
int dpmiAllocDos(unsigned int paragraphs, unsigned short* segment, unsigned short* selector);

// Free a DOS memory block previously returned by dpmiAllocDos.
void dpmiFreeDos(unsigned short selector);

// Returns 1 if the real-mode interrupt vector for intNumber is hooked
// (segment:offset is non-null), 0 otherwise. Coarse check — for TSR detection
// you usually want dpmiGetVector + a signature probe at the handler entry.
int dpmiVectorInstalled(int intNumber);

// Read the current real-mode interrupt vector for intNumber.
// On success: *segment and *offset receive the vector (CX:DX from DPMI 0200h),
// returns 1. Returns 0 on DPMI host error.
int dpmiGetVector(int intNumber, unsigned short* segment, unsigned short* offset);

#endif
