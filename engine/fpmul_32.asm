; this function uses 64 bit math to multiply two 32 bit fixed point numbers
; in 16:16 format — 32-bit DOS/4GW flat-mode build

.MODEL FLAT, C      ; flat memory model, C calling convention
.386

.CODE

PUBLIC fpMul       ; export function name to linker

fpMul PROC C multiplier:DWORD, multiplicand:DWORD

    mov eax, multiplicand   ; move multiplicand into eax
    imul multiplier         ; multiply by multiplier, result edx:eax
    shr eax, 16             ; shift the integer half of the 16:16 product
                            ; down into the low 32 bits of the return value
    ret                     ; return to caller (result in eax)

fpMul ENDP

END
