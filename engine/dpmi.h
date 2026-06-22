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

// Call a real-mode FAR procedure from protected mode (DPMI INT 31h func 0301h).
// regs->cs:ip must point to the real-mode procedure to call; the other register
// fields are passed in and updated on return. DPMI supplies a real-mode stack
// when regs->ss:sp are zero. Returns 0 on success, nonzero on DPMI host error.
int dpmiCallRealFar(DpmiRealModeRegsPtr regs);

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

// Allocate a real-mode callback (DPMI INT 31h func 0303h).
//
// pmCallback : address of a protected-mode procedure that DPMI will invoke when
//              real-mode code calls the returned (cbSegment:cbOffset) entry.
//              The procedure receives ES:EDI = pointer to `rmRegs` (already
//              populated with the real-mode register state at time of upcall);
//              it must preserve all registers and return via IRETD. Typically
//              authored as a Watcom #pragma aux asm trampoline that saves
//              registers, sets DS to our data selector, calls a real C handler,
//              restores registers, IRETs.
// rmRegs     : storage for the real-mode register frame DPMI populates on each
//              upcall. Must remain valid for the lifetime of the callback.
// cbSegment, cbOffset : on success, receive the real-mode entry point that the
//              TSR (packet driver, etc.) can call to reach our PM handler.
//
// Returns 1 on success, 0 on DPMI host error.
int dpmiAllocRealCallback(
    void (*pmCallback)(void),
    DpmiRealModeRegsPtr rmRegs,
    unsigned short* cbSegment,
    unsigned short* cbOffset
);

// Free a real-mode callback previously returned by dpmiAllocRealCallback
// (DPMI INT 31h func 0304h).
void dpmiFreeRealCallback(unsigned short cbSegment, unsigned short cbOffset);

// Map a physical address range into the flat address space (DPMI INT 31h func
// 0800h). Used to reach a VBE 2.0 linear framebuffer, whose physical address
// sits above the 1 MB line and so isn't otherwise addressable. On success
// *linear receives a linear address usable as a flat pointer under DOS/4GW.
// Returns 1 on success, 0 on DPMI host error (e.g. host without 0800h support).
int dpmiMapPhysical(unsigned long phys, unsigned long size, unsigned long* linear);

#endif
