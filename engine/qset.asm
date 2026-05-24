; this function fills a region of memory using 32 bit stores

.MODEL MEDIUM, C    ; medium memory model, C calling convention
.386

.CODE

PUBLIC fquadset    ; export function name to linker

fquadset PROC FAR C dest:DWORD, data:DWORD, count:DWORD

    cld                     ; clear the direction of movement

    les di, dest            ; point es:di at destination
    mov ecx, count          ; move into ecx number of quads
    mov eax, data           ; move the data into eax
    rep stosd               ; fill the region with data

    ret                     ; return to caller

fquadset ENDP

END
