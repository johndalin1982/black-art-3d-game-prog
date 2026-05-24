; this function performs a memory copy from source to destination using 32 bit
; moves

.MODEL MEDIUM, C    ; medium memory model, C calling convention
.386

.CODE

PUBLIC fquadcpy    ; export function name to linker

fquadcpy PROC FAR C USES ds, dest:DWORD, source:DWORD, count:DWORD

    cld                     ; clear the direction of movement

    lds si, source          ; point ds:si at source
    les di, dest            ; point es:di at destination

    mov ecx, count          ; move into ecx number of dwords
    rep movsd               ; move the data

    ret                     ; return to caller

fquadcpy ENDP

END
