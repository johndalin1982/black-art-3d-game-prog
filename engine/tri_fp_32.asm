; this function draws a triangle from y1 to y2 using x1 and x2 as the starting
; x points along with deltas dxRight and dxLeft. The function uses 32 bit
; fixed point math, notice the use of floating point instructions to convert
; the floating point values into fixed point values — 32-bit DOS/4GW flat-mode
; build

.MODEL FLAT, C      ; flat memory model, C calling convention
.386
.387                ; enable 387 FPU instructions (fld, fmul, fistp)

.CODE

PUBLIC triangleAsm ; export function name to linker

; float params declared as DWORD (4 bytes, same as REAL4) since Open Watcom 2.0
; wasm rejects REAL4 in PROC parameter lists; FPU loads use DWORD PTR to size
; USES ebx edi — callee-saved registers in the C calling convention
; (see qcpy_32.asm); ebx holds the line vertical offset, edi the dest pointer
triangleAsm PROC C USES ebx edi, dest:DWORD, y1:DWORD, y2:DWORD, x1:DWORD, x2:DWORD, dxLeft:DWORD, dxRight:DWORD, color:DWORD

; local variables — all DWORD in flat mode for clean 32-bit access

LOCAL xsF:DWORD, xeF:DWORD, dxLeftF:DWORD, dxRightF:DWORD, xs:DWORD, xe:DWORD

    xor ebx, ebx            ; reset ebx to 0, used for line increment

    mov edx, y1             ; edx = y1 (line counter)

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

; for (edx = y1; edx <= y2; edx++)

beginLine:

    ; arithmetic shift (sar) — fixed-point dxLeftF can round 1 LSB more negative
    ; than the mathematical slope, so xsF can accumulate to a tiny negative at
    ; the last row of a triangle whose left edge ends at x=0.  logical shr would
    ; convert e.g. xsF=-2 to xs=65535, and the rep stosw count below would
    ; underflow to ~2 billion and write 4 GB of garbage.  sar keeps the sign so
    ; xs becomes -1 and the asm produces at worst a single stray pixel one row up.
    mov eax, xsF            ; get whole part of xsF into xs
    sar eax, 16
    mov xs, eax

    mov eax, xeF            ; get whole part of xeF into xe
    sar eax, 16
    mov xe, eax

    mov edi, dest           ; edi = start of line (flat pointer)
    add edi, xs
    add edi, ebx            ; add vertical offset

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
    add edi, ebx            ; add vertical offset

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
    add edi, ebx            ; add vertical offset

    cld                     ; clear the direction of movement

    mov ax, cx              ; ax = color word (for stosw)

    mov ecx, xe             ; ecx = number of words to move (xe-xs+1)/2
    sub ecx, xs
    inc ecx
    shr ecx, 1              ; divide by 2

    rep stosw               ; fill the region with data

endLine:
                            ; perform fixed point addition of deltas
    mov eax, dxLeftF        ; xsF += dxLeftF
    add xsF, eax

    mov eax, dxRightF       ; xeF += dxRightF
    add xeF, eax

    add ebx, 320            ; move to next line in destination buffer

    inc edx                 ; edx++
    cmp edx, y2

    jle beginLine           ; if (edx <= y2) process next line

    ret                     ; later!

; constants section, must be here so that processor doesn't try to execute
; data

FPSHIFT DD 65536.0          ; equivalent to FP_SCALE in the fixed point defines
                            ; (DD with float literal — REAL4 isn't accepted as
                            ; a top-level data type by Open Watcom 2.0 wasm)

triangleAsm ENDP

END
