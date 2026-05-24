; this function draws a line from xs to xe using 32 bit data movement

.MODEL MEDIUM, C    ; medium memory model, C calling convention
.386

.CODE

PUBLIC triangle32Line  ; export function name to linker

triangle32Line PROC FAR C dest:DWORD, xs:WORD, xe:WORD, color:WORD

    les di, dest            ; point es:di to start of line
    add di, xs

; process special cases first, i.e. lines of length 1, 2, 3 or 4

begin:

    mov ax, xe              ; ax = xe - xs
    sub ax, xs

    mov cx, color           ; cx = color | color << 8
    mov ch, cl

test0:

    cmp ax, 0               ; if (ax == 0)
    jne test1               ; else goto test1

    mov es:[di], cl
    ret

test1:

    cmp ax, 1               ; if (ax == 1)
    jne test2               ; else goto test2

    mov es:[di], cx
    ret

test2:

    cmp ax, 2               ; if (ax == 2)
    jne test3               ; else goto test3

    mov es:[di], cx
    add di, 2
    mov es:[di], cl
    ret

test3:

    cmp ax, 3               ; if (ax == 3)
    jne processLeftEnd      ; else process left end

    mov es:[di], cx
    add di, 2
    mov es:[di], cx
    ret

processLeftEnd:

    mov ax, xs              ; ax = xs & 0x03
    and ax, 03h

testL1:

    cmp ax, 1               ; if (ax == 1)
    jne testL2

    mov es:[di], cl
    inc di
    mov es:[di], cx

    add xs, 3               ; xs += 3

    jmp processRightEnd

testL2:

    cmp ax, 2               ; if (ax == 2)
    jne testL3

    mov es:[di], cx

    add xs, 2               ; xs += 2

    jmp processRightEnd

testL3:

    cmp ax, 3               ; if (ax == 3)
    jne processRightEnd

    mov es:[di], cl

    inc xs                  ; xs += 1

processRightEnd:

    les di, dest            ; point es:di to start of line
    add di, xe

    mov ax, xe              ; ax = xe & 0x03
    and ax, 03h

testR0:

    cmp ax, 0               ; if (ax == 0)
    jne testR1

    mov es:[di], cl

    dec xe                  ; xe -= 1

    jmp processMiddle

testR1:

    cmp ax, 1               ; if (ax == 1)
    jne testR2

    dec di
    mov es:[di], cx

    sub xe, 2               ; xe -= 2

    jmp processMiddle

testR2:

    cmp ax, 2               ; if (ax == 2)
    jne processMiddle

    mov es:[di], cl
    sub di, 2
    mov es:[di], cx

    sub xe, 3               ; xe -= 3

processMiddle:

    les di, dest            ; point es:di to start of line
    add di, xs

    cld                     ; clear the direction of movement

    mov eax, ecx            ; move the color data into eax
    shl eax, 16
    or eax, ecx

    mov cx, xe              ; compute number of dwords to move (xe-xs+1)/4
    sub cx, xs
    inc cx
    shr cx, 2               ; divide by 4

    rep stosd               ; fill the region with data

    ret                     ; return to caller

triangle32Line ENDP

END
