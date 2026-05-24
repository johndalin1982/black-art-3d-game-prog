; this function draws a line from xs to xe using 16 bit data movement

.MODEL MEDIUM, C    ; medium memory model, C calling convention
.386

.CODE

PUBLIC triangle16Line  ; export function name to linker

triangle16Line PROC FAR C dest:DWORD, xs:WORD, xe:WORD, color:WORD

begin:

    les di, dest            ; point es:di to start of line
    add di, xs

    mov cx, color           ; cx = color | color << 8
    mov ch, cl

processLeftEnd:

    mov ax, xs              ; ax = xs & 0x01
    and ax, 01h

testL1:

    cmp ax, 1               ; if (ax == 1)
    jne processRightEnd

    mov es:[di], cl         ; plot pixel
    inc xs                  ; xs++

processRightEnd:

    les di, dest            ; point es:di to start of line
    add di, xe

    mov ax, xe              ; ax = xe & 0x01
    and ax, 01h

testR0:

    cmp ax, 0               ; if (ax == 0)
    jne processMiddle

    mov es:[di], cl         ; plot pixel
    dec xe                  ; xe -= 1

processMiddle:

    les di, dest            ; point es:di to start of line
    add di, xs

    cld                     ; clear the direction of movement

    mov ax, cx              ; move the color data into ax

    mov cx, xe              ; compute number of words to move (xe-xs+1)/2
    sub cx, xs
    inc cx
    shr cx, 1               ; divide by 2

    rep stosw               ; fill the region with data

    ret                     ; return to caller

triangle16Line ENDP

END
