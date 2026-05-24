; this function fills a region of memory using 32 bit stores — 32-bit DOS/4GW
; flat-mode build

.MODEL FLAT, C      ; flat memory model, C calling convention
.386

.CODE

PUBLIC fquadset    ; export function name to linker

; USES edi — callee-saved register in the C calling convention (see qcpy_32.asm)
fquadset PROC C USES edi, dest:DWORD, data:DWORD, count:DWORD

    cld                     ; clear the direction of movement

    mov edi, dest           ; edi = dest (flat 32-bit pointer)
    mov ecx, count          ; ecx = number of quads
    mov eax, data           ; eax = the data to write
    rep stosd               ; fill the region with data

    ret                     ; return to caller

fquadset ENDP

END
