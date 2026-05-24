; this function draws a triangle from y1 to y2 using x1 and x2 as the starting
; x points along with deltas dxRight and dxLeft. The function uses 32 bit
; fixed point math, notice the use of floating point instructions to convert
; the floating point values into fixed point values

.MODEL MEDIUM, C    ; medium memory model, C calling convention
.386
.387                ; enable 387 FPU instructions (fld, fmul, fistp)

.CODE

PUBLIC triangleAsm ; export function name to linker

; float params declared as DWORD (4 bytes, same as REAL4) since Open Watcom 2.0
; wasm rejects REAL4 in PROC parameter lists; FPU loads use DWORD PTR to size
triangleAsm PROC FAR C dest:DWORD, y1:WORD, y2:WORD, x1:DWORD, x2:DWORD, dxLeft:DWORD, dxRight:DWORD, color:WORD

; local variables

LOCAL xsF:DWORD, xeF:DWORD, dxLeftF:DWORD, dxRightF:DWORD, xs:WORD, xe:WORD

    mov bx, 0               ; reset bx to 0, used for line increment

    mov dx, y1              ; dx = y1

    fld   DWORD PTR x1      ; xsF = (x1 * 65536)
    fmul  DWORD PTR FPSHIFT
    fistp xsF

    fld   DWORD PTR x2      ; xeF = (x2 * 65536)
    fmul  DWORD PTR FPSHIFT
    fistp xeF

    fld   DWORD PTR dxLeft  ; dxLeftF = (dxLeft * 65536)
    fmul  DWORD PTR FPSHIFT
    fistp dxLeftF

    fld   DWORD PTR dxRight ; dxRightF = (dxRight * 65536)
    fmul  DWORD PTR FPSHIFT
    fistp dxRightF

; for (dx = y1; dx <= y2; dx++)

beginLine:

    mov ax, WORD PTR xsF[2] ; get whole part of xsF into xs
    mov xs, ax

    mov ax, WORD PTR xeF[2] ; get whole part of xeF into xe
    mov xe, ax

    les di, dest            ; point es:di to start of line
    add di, xs
    add di, bx              ; add vertical offset

    mov cx, color           ; cx = color | color << 8
    mov ch, cl

processLeftEnd:

    mov ax, WORD PTR xsF[2] ; ax = xs & 0x01
    and ax, 01h

testL1:

    cmp ax, 1               ; if (ax == 1)
    jne processRightEnd

    mov es:[di], cl         ; plot pixel
    inc xs                  ; xs++

processRightEnd:

    les di, dest            ; point es:di to start of line
    add di, xe
    add di, bx              ; add vertical offset

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
    add di, bx              ; add vertical offset

    cld                     ; clear the direction of movement

    mov ax, cx              ; move the color data into ax

    mov cx, xe              ; compute number of words to move (xe-xs+1)/2
    sub cx, xs
    inc cx
    shr cx, 1               ; divide by 2

    rep stosw               ; fill the region with data

endLine:
                            ; perform fixed point addition of deltas
    mov eax, dxLeftF        ; xsF += dxLeftF
    add xsF, eax

    mov eax, dxRightF       ; xeF += dxRightF
    add xeF, eax

    add bx, 320             ; move to next line in destination buffer

    inc dx                  ; dx++
    cmp dx, y2

    jle beginLine           ; if (dx <= y2) process next line

    ret                     ; later!

; constants section, must be here so that processor doesn't try to execute
; data

FPSHIFT DD 65536.0          ; equivalent to FP_SCALE in the fixed point defines
                            ; (DD with float literal — REAL4 isn't accepted as
                            ; a top-level data type by Open Watcom 2.0 wasm)

triangleAsm ENDP

END
