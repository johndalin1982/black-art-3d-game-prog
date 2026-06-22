# Project context

This is a port of "The Black Art of 3D Game Programming" (André LaMothe, 1995)
demos to SVGA (VESA VBE 2.0), built with Open Watcom 2.0, 32-bit DOS/4GW. The
engine (`engine/black3.c`/`black4.c`) supports mode-13h and VESA at any
resolution/bpp, switchable at runtime — there is no fixed target resolution.

## Ground truth

Before starting any work on the VESA engine or demo ports, read:

    vbe/PORTING.md

Treat it as the authoritative baseline for what exists — engine API, function
names, asset conventions, build commands, and project structure. If something is
not in PORTING.md, it does not exist. Do not reference it, do not document it,
and do not generate code that calls it.

The conversation history provides context for decisions. It is not a source of
truth about what the codebase contains.
