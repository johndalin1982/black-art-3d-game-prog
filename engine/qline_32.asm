; this function draws a line from xs to xe using 32 bit data movement —
; 32-bit DOS/4GW flat-mode build

.MODEL FLAT, C      ; flat memory model, C calling convention
.386

.CODE

PUBLIC triangle32Line  ; export function name to linker

; USES edi — callee-saved register in the C calling convention (see qcpy_32.asm)
triangle32Line PROC C USES edi, dest:DWORD, xs:DWORD, xe:DWORD, color:DWORD

    mov edi, dest           ; edi = start of line (flat pointer)
    add edi, xs

; process special cases first, i.e. lines of length 1, 2, 3 or 4

begin:

    mov eax, xe             ; eax = xe - xs
    sub eax, xs

    mov cl, BYTE PTR color  ; cx = color | color << 8
    mov ch, cl

test0:

    cmp eax, 0              ; if (eax == 0)
    jne test1               ; else goto test1

    mov BYTE PTR [edi], cl
    ret

test1:

    cmp eax, 1              ; if (eax == 1)
    jne test2               ; else goto test2

    mov WORD PTR [edi], cx
    ret

test2:

    cmp eax, 2              ; if (eax == 2)
    jne test3               ; else goto test3

    mov WORD PTR [edi], cx
    add edi, 2
    mov BYTE PTR [edi], cl
    ret

test3:

    cmp eax, 3              ; if (eax == 3)
    jne processLeftEnd      ; else process left end

    mov WORD PTR [edi], cx
    add edi, 2
    mov WORD PTR [edi], cx
    ret

processLeftEnd:

    mov eax, xs             ; eax = xs & 0x03
    and eax, 03h

testL1:

    cmp eax, 1              ; if (eax == 1)
    jne testL2

    mov BYTE PTR [edi], cl
    inc edi
    mov WORD PTR [edi], cx

    add DWORD PTR xs, 3     ; xs += 3

    jmp processRightEnd

testL2:

    cmp eax, 2              ; if (eax == 2)
    jne testL3

    mov WORD PTR [edi], cx

    add DWORD PTR xs, 2     ; xs += 2

    jmp processRightEnd

testL3:

    cmp eax, 3              ; if (eax == 3)
    jne processRightEnd

    mov BYTE PTR [edi], cl

    inc DWORD PTR xs        ; xs += 1

processRightEnd:

    mov edi, dest           ; edi = start of line
    add edi, xe

    mov eax, xe             ; eax = xe & 0x03
    and eax, 03h

testR0:

    cmp eax, 0              ; if (eax == 0)
    jne testR1

    mov BYTE PTR [edi], cl

    dec DWORD PTR xe        ; xe -= 1

    jmp processMiddle

testR1:

    cmp eax, 1              ; if (eax == 1)
    jne testR2

    dec edi
    mov WORD PTR [edi], cx

    sub DWORD PTR xe, 2     ; xe -= 2

    jmp processMiddle

testR2:

    cmp eax, 2              ; if (eax == 2)
    jne processMiddle

    mov BYTE PTR [edi], cl
    sub edi, 2
    mov WORD PTR [edi], cx

    sub DWORD PTR xe, 3     ; xe -= 3

processMiddle:

    mov edi, dest           ; edi = start of line
    add edi, xs

    cld                     ; clear the direction of movement

    movzx eax, cx           ; eax = color word, zero-extended
    shl eax, 16
    or ax, cx               ; eax = (color << 16) | color (replicated dword)

    mov ecx, xe             ; ecx = number of dwords to move (xe-xs+1)/4
    sub ecx, xs
    inc ecx
    shr ecx, 2              ; divide by 4

    rep stosd               ; fill the region with data

    ret                     ; return to caller

triangle32Line ENDP

END
