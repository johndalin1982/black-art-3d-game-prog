; this function uses 64 bit math to multiply two 32 bit fixed point numbers
; in 16:16 format

.MODEL MEDIUM, C    ; medium memory model, C calling convention
.386

.CODE

PUBLIC fpMul       ; export function name to linker

fpMul PROC FAR C multiplier:DWORD, multiplicand:DWORD

    mov eax, multiplicand   ; move multiplicand into eax
    imul multiplier         ; multiply by multiplier, result edx:eax
    shr eax, 16             ; shift upper 16 bits of eax to the right so the
                            ; result is in dx:ax instead of edx:eax
    ret                     ; return to caller

fpMul ENDP

END
