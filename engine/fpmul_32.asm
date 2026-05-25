; this function uses 64 bit math to multiply two 32 bit fixed point numbers
; in 16:16 format — 32-bit DOS/4GW flat-mode build

.MODEL FLAT, C      ; flat memory model, C calling convention
.386

.CODE

PUBLIC fpMul       ; export function name to linker

fpMul PROC C multiplier:DWORD, multiplicand:DWORD

    mov eax, multiplicand   ; move multiplicand into eax
    imul multiplier         ; signed 32x32 -> 64-bit result in edx:eax
    shrd eax, edx, 16       ; the 16:16 fixed-point result lives in bits 16..47
                            ; of edx:eax — shift it down into eax (shrd brings
                            ; the low 16 bits of edx into the high 16 bits of eax)
    ret                     ; return to caller (result in eax)

fpMul ENDP

END
