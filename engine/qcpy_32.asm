; this function performs a memory copy from source to destination using 32 bit
; moves — 32-bit DOS/4GW flat-mode build

.MODEL FLAT, C      ; flat memory model, C calling convention
.386

.CODE

PUBLIC fquadcpy    ; export function name to linker

; USES esi edi — these are callee-saved registers in the C calling convention.
; Without preserving them, the C compiler's cached array pointers/loop indices
; in those registers get clobbered across the call, causing silent corruption.
fquadcpy PROC C USES esi edi, dest:DWORD, source:DWORD, count:DWORD

    cld                     ; clear the direction of movement

    mov esi, source         ; esi = source (flat 32-bit pointer)
    mov edi, dest           ; edi = dest (flat 32-bit pointer)

    mov ecx, count          ; ecx = number of dwords
    rep movsd               ; move the data

    ret                     ; return to caller

fquadcpy ENDP

END
