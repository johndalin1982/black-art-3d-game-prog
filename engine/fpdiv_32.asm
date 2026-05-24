; this function uses 64 bit math to divide two numbers in 32 bit 16:16
; fixed point format — 32-bit DOS/4GW flat-mode build

.MODEL FLAT, C      ; flat memory model, C calling convention
.386

.CODE

PUBLIC fpDiv       ; export function name to linker

fpDiv PROC C dividend:DWORD, divisor:DWORD

    mov eax, dividend       ; move dividend into eax
    cdq                     ; convert eax into edx:eax using sign extension
    shld edx, eax, 16       ; shift edx:eax 16 bits to the left so the result
                            ; is in fixed point
    sal eax, 16             ; manually shift eax since shld doesn't change the
                            ; source register

    idiv divisor            ; perform division (eax = quotient)

    ret                     ; return to caller (result in eax)

fpDiv ENDP

END
