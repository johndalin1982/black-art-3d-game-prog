; this function uses 64 bit math to divide two numbers in 32 bit 16:16
; fixed point format

.MODEL MEDIUM, C    ; medium memory model, C calling convention
.386

.CODE

PUBLIC fpDiv       ; export function name to linker

fpDiv PROC FAR C dividend:DWORD, divisor:DWORD

    mov eax, dividend       ; move dividend into eax
    cdq                     ; convert eax into edx:eax using sign extension
    shld edx, eax, 16       ; shift edx:eax 16 bits to the left so the result
                            ; is in fixed point
    sal eax, 16             ; manually shift eax since shld doesn't change the
                            ; source register

    idiv divisor            ; perform division

    shld edx, eax, 16       ; move result into dx:ax since it's in eax
    ret                     ; return to caller

fpDiv ENDP

END
