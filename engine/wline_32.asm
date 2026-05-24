; this function draws a line from xs to xe using 16 bit data movement —
; 32-bit DOS/4GW flat-mode build

.MODEL FLAT, C      ; flat memory model, C calling convention
.386

.CODE

PUBLIC triangle16Line  ; export function name to linker

; USES edi — callee-saved register in the C calling convention (see qcpy_32.asm)
triangle16Line PROC C USES edi, dest:DWORD, xs:DWORD, xe:DWORD, color:DWORD

begin:

    mov edi, dest           ; edi = start of line (flat pointer)
    add edi, xs

    mov cl, BYTE PTR color  ; cx = color | color << 8
    mov ch, cl

processLeftEnd:

    mov eax, xs             ; eax = xs & 0x01
    and eax, 01h

testL1:

    cmp eax, 1              ; if (eax == 1)
    jne processRightEnd

    mov BYTE PTR [edi], cl  ; plot pixel
    inc DWORD PTR xs        ; xs++

processRightEnd:

    mov edi, dest           ; edi = start of line
    add edi, xe

    mov eax, xe             ; eax = xe & 0x01
    and eax, 01h

testR0:

    cmp eax, 0              ; if (eax == 0)
    jne processMiddle

    mov BYTE PTR [edi], cl  ; plot pixel
    dec DWORD PTR xe        ; xe -= 1

processMiddle:

    mov edi, dest           ; edi = start of line
    add edi, xs

    cld                     ; clear the direction of movement

    mov ax, cx              ; ax = color | color << 8 (for stosw)

    mov ecx, xe             ; ecx = number of words to move (xe-xs+1)/2
    sub ecx, xs
    inc ecx
    shr ecx, 1              ; divide by 2

    rep stosw               ; fill the region with data

    ret                     ; return to caller

triangle16Line ENDP

END
