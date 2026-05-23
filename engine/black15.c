#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <bios.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <search.h>             // this one is needed for qsort()

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black11.h"
#include "black15.h"

int FAR* ZBuffer;    // the current z buffer memory
int FAR* ZBank1;     // memory bank 1 of z buffer
int FAR* ZBank2;     // memory bank 2 of z buffer

unsigned int ZHeight = 200;         // the height of the z buffer
unsigned int ZHeight2 = 100;        // the height of half the z buffer
unsigned int ZBankSize = 64000L;    // size of a z buffer bank in bytes

FILE* FpOut; // general output file

void drawTbTri3DZ(
    int x1,
    int y1,
    int z1,
    int x2,
    int y2,
    int z2,
    int x3,
    int y3,
    int z3,
    int color) {

    // this function draws a triangle that has a flat top
    float dxRight,  // the dx/dy ratio of the right edge of line
          dxLeft,   // the dx/dy ratio of the left edge of line
          xs, xe,   // the starting and ending points of the edges
          height,   // the height of the triangle
          dx,       // general deltas
          dy,
          zLeft,    // the z value of the left edge of current line
          zRight,   // the z value for the right edge of current line
          ay,       // interpolator constant
          b1Y,      // the change of z with respect to y on the left edge
          b2Y;      // the change of z with respect to y on the right edge

    int tempX,      // used during sorting as temps
        tempY,
        tempZ,
        xsClip,     // used by clipping
        xeClip,
        x,
        xIndex,     // used as looping vars
        yIndex;

    // change these two back to float and remove al *32 and >>5
    // if you don't want to use fixed point during horizontal z interpolation
    int zMiddle,    // the z value of the middle between the left and right
        bx;         // the change of z with respect to x

    unsigned char FAR* destAddr;    // current image destination

    // test order of x1 and x2, note y1=y2

    // test if top or bottom is flat and set constants appropriately
    if (y1 == y2) {
        // perform computations for a triangle with a flat top
        if (x2 < x1) {
            tempX = x2;
            tempZ = z2;

            x2 = x1;
            z2 = z1;

            x1 = tempX;
            z1 = tempZ;
        }

        // compute deltas for scan conversion
        height = y3 - y1;

        dxLeft = (x3 - x1) / height;
        dxRight = (x3 - x2) / height;

        // compute deltas for z interpolation
        zLeft = z1;
        zRight = z2;

        // vertical interpolants
        ay = 1 / height;

        b1Y = ay * (z3 - z1);
        b2Y = ay * (z3 - z2);

        // set starting points

        xs = (float)x1;
        xe = (float)x2;
    } else {
        // bottom must be flat

        // test order of x3 and x2, note y2=y3
        if (x3 < x2) {
            tempX = x2;
            tempZ = z2;

            x2 = x3;
            z2 = z3;

            x3 = tempX;
            z3 = tempZ;
        }

        // compute deltas for scan conversion
        height = y3 - y1;

        dxLeft = (x2 - x1) / height;
        dxRight = (x3 - x1) / height;

        // compute deltas for z interpolation
        zLeft = z1;
        zRight = z1;

        // vertical interpolants
        ay = 1 / height;

        b1Y = ay * (z2 - z1);
        b2Y = ay * (z3 - z1);

        // set starting points
        xs = (float)x1;
        xe = (float)x1;
    }

    // perform y clipping

    // clip top
    if (y1 < PolyClipMinY) {
        // compute new xs and ys
        dy = (float)(-y1 + PolyClipMinY);

        xs = xs + dxLeft * dy;
        xe = xe + dxRight * dy;

        // re-compute zLeft and zRight to take into consideration
        // vertical shift down
        zLeft += b1Y * dy;
        zRight += b2Y * dy;

        // reset y1
        y1 = PolyClipMinY;
    }

    // clip bottom
    if (y3 > PolyClipMaxY) {
        y3 = PolyClipMaxY;
    }

    // compute starting address in video memory
    destAddr = DoubleBuffer + (y1 << 8) + (y1 << 6);

    // start z buffer at proper bank
    if (y1 < ZHeight2) {
        ZBuffer = ZBank1 + (y1 << 8) + (y1 << 6);
    } else {
        tempY = y1 - ZHeight2;

        ZBuffer = ZBank2 + (tempY << 8) + (tempY << 6);
    }

    // test if x clipping is needed
    if (x1 >= PolyClipMinX && x1 <= PolyClipMaxX &&
        x2 >= PolyClipMinX && x2 <= PolyClipMaxX &&
        x3 >= PolyClipMinX && x3 <= PolyClipMaxX) {

        // draw the triangle
        for (yIndex = y1; yIndex <= y3; yIndex++) {
            // test if we need to switch to z buffer bank 2
            if (yIndex == ZHeight2) {
                ZBuffer = ZBank2;
            }

            /////////////////////////////////////////////////////////////////////////////////
            // This section uses a bit of 16 bit fixed point for the horizontal
            // interpolation. The format is 11:5 i.e. 11 whole places and 5 decimal places.
            // Also, notice the use of >>5 and *32 to convert to fixed point and back to
            // simple integer, also the old version of each line is commented out under each
            // fixed point calculation, so you can see what was originally done...

            // compute horizontal z interpolant
            zMiddle = 32 * zLeft;
            // zMiddle = zLeft;

            bx = 32 * (zRight - zLeft) / (1 + xe - xs);
            // bx = (zRight - zLeft) / (1 + xe - xs);

            // draw the line
            for (xIndex = xs; xIndex <= xe; xIndex++) {
                // if current zMiddle is less than z-buffer then replace
                // and update image buffer
                if (zMiddle >> 5 < ZBuffer[xIndex]) { // if (zMiddle < ZBuffer[xIndex])
                    // update z buffer
                    ZBuffer[xIndex] = (int)zMiddle >> 5;
                    // zBuffer[xIndex] = (int)zMiddle;

                    // write to image buffer
                    destAddr[xIndex] = color;

                    // update video buffer
                }

                // update current z value
                zMiddle += bx;
            }

            // END FIXED POINT DEMO SECTION /////////////////////////////////////////////////

            // adjust starting point and ending point for scan conversion
            xs += dxLeft;
            xe += dxRight;

            // adjust vertical z interpolants
            zLeft += b1Y;
            zRight += b2Y;

            // adjust video and z buffer offsets
            destAddr += 320;
            ZBuffer += 320;
        }
    } else {
        // clip x axis with slower version

        // draw the triangle
        for (yIndex = y1; yIndex <= y3; yIndex++) {
            // test if we need to switch to z buffer bank 2
            if (yIndex == ZHeight2) {
                ZBuffer = ZBank2;
            }

            // do x clip
            xsClip = (int)xs;
            xeClip = (int)xe;

            ///////////////////////////////////////////////////////////////////////////////
            // This section uses a bit of 16 bit fixed point for the horizontal
            // interpolation. The format is 11:5 i.e. 11 whole places and 5 decimal places.
            // Also, notice the use of >>5 and *32 to convert to fixed point and back to
            // simple integer, also the old version of each line is commented out under each
            // fixed point calculation.

            // compute horizontal z interpolant
            zMiddle = 32 * zLeft;
            // zMiddle = zLeft;

            bx = 32 * (zRight - zLeft) / (1 + xe - xs);
            // bx = 32 * (zRight - zLeft) / (1 + xe - xs);

            // adjust starting point and ending point
            xs += dxLeft;
            xe += dxRight;

            // adjust vertical z interpolants
            zLeft += b1Y;
            zRight += b2Y;

            // clip line
            if (xsClip < PolyClipMinX) {
                dx = (-xsClip + PolyClipMinX);
                xsClip = PolyClipMinX;

                // re-compute zMiddle to take into consideration horizontal shift
                zMiddle += 32 * bx * dx;
                // zMiddle += bx * dx;
            }

            if (xeClip > PolyClipMaxX) {
                xeClip = PolyClipMaxX;
            }

            // draw the line
            for (xIndex = xsClip; xIndex <= xeClip; xIndex++) {
                // if current zMiddle is less than z-buffer then replace
                // and update image buffer
                if (zMiddle >> 5 < ZBuffer[xIndex]) { // if (zMiddle < ZBuffer[xIndex])
                    // update z buffer
                    ZBuffer[xIndex] = (int)zMiddle >> 5;
                    // ZBuffer[xIndex] = (int)zMiddle;

                    // write to image buffer
                    destAddr[xIndex] = color;

                    // update video buffer
                }

                // update current z value
                zMiddle += bx;
            }

            // END FIXED POINT DEMO SECTION ///////////////////////////////////////////////

            // adjust video and z buffer offsets
            destAddr += 320;
            ZBuffer += 320;
        }
    }
}

void drawTri3DZ(
    int x1,
    int y1,
    int z1,
    int x2,
    int y2,
    int z2,
    int x3,
    int y3,
    int z3,
    int color) {

    int tempX,  // used for sorting
        tempY,
        tempZ,
        newX,   // used to compute new x and z at triangle splitting point
        newZ;

    // test for h lines and v lines
    if (x1 == x2 && x2 == x3 || y1 == y2 && y2 == y3) {
        return;
    }

    // sort p1,p2,p3 in ascending y order
    if (y2 < y1) {
        tempX = x2;
        tempY = y2;
        tempZ = z2;

        x2 = x1;
        y2 = y1;
        z2 = z1;

        x1 = tempX;
        y1 = tempY;
        z1 = tempZ;
    }

    // now we know that p1 and p2 are in order
    if (y3 < y1) {
        tempX = x3;
        tempY = y3;
        tempZ = z3;

        x3 = x1;
        y3 = y1;
        z3 = z1;

        x1 = tempX;
        y1 = tempY;
        z1 = tempZ;
    }

    // finally test y3 against y2
    if (y3 < y2) {
        tempX = x3;
        tempY = y3;
        tempZ = z3;

        x3 = x2;
        y3 = y2;
        z3 = z2;

        x2 = tempX;
        y2 = tempY;
        z2 = tempZ;
    }

    // do trivial rejection tests
    if (y3 < PolyClipMinY || y1 > PolyClipMaxY ||
        x1 < PolyClipMinX && x2 < PolyClipMinX && x3 < PolyClipMinX ||
        x1 > PolyClipMaxX && x2 > PolyClipMaxX && x3 > PolyClipMaxX) {

        return;
    }

    // test if top of triangle is flat
    if (y1 == y2 || y2 == y3) {
        drawTbTri3DZ(x1, y1, z1, x2, y2, z2, x3, y3, z3, color);
    } else {
        // general triangle that needs to be broken up along long edge

        // compute new x,z at split point
        newX = x1 + (int)((float)(y2 - y1) * (float)(x3 - x1) / (float)(y3 - y1));
        newZ = z1 + (int)((float)(y2 - y1) * (float)(z3 - z1) / (float)(y3 - y1));

        // draw each sub-triangle
        if (y2 >= PolyClipMinY && y1 < PolyClipMaxY) {
            drawTbTri3DZ(x1, y1, z1, newX, y2, newZ, x2, y2, z2, color);
        }

        if (y3 >= PolyClipMinY && y1 < PolyClipMaxY) {
            drawTbTri3DZ(x2, y2, z2, newX, y2, newZ, x3, y3, z3, color);
        }
    }
}

void drawPolyListZ(void) {
    // this function draws the global polygon list generated by calls to
    // generatePolyList using the z buffer triangle system
    int currPoly,   // the current polygon
        isQuad = 0; // quadrilateral flag

    float x1, y1, z1,   // working variables
          x2, y2, z2,
          x3, y3, z3,
          x4, y4, z4;

    // draw each polygon in list
    for (currPoly = 0; currPoly < NumPolysFrame; currPoly++) {
        // do Z clipping first before projection
        z1 = WorldPolys[currPoly]->vertexList[0].z;
        z2 = WorldPolys[currPoly]->vertexList[1].z;
        z3 = WorldPolys[currPoly]->vertexList[2].z;

        // test if this is a quad
        if (WorldPolys[currPoly]->numPoints == 4) {
            // extract vertex number and z component for clipping and projection
            z4 = WorldPolys[currPoly]->vertexList[3].z;

            // set quad flag
            isQuad = 1;
        } else {
            z4 = z3;
        }

#if 0
        if (z1 < ClipNearZ && z2 < ClipNearZ && z3 < ClipNearZ && z4 < ClipNearZ ||
            z1 > ClipFarZ && z2 > ClipFarZ && z3 > ClipFarZ && z4 > ClipFarZ) {

            continue;
        }
#endif

        // extract points of polygon
        x1 = WorldPolys[currPoly]->vertexList[0].x;
        y1 = WorldPolys[currPoly]->vertexList[0].y;

        x2 = WorldPolys[currPoly]->vertexList[1].x;
        y2 = WorldPolys[currPoly]->vertexList[1].y;

        x3 = WorldPolys[currPoly]->vertexList[2].x;
        y3 = WorldPolys[currPoly]->vertexList[2].y;

        // compute screen position of points
        x1 = HALF_SCREEN_WIDTH + x1 * ViewingDistance / z1;
        y1 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y1 * ViewingDistance / z1;

        x2 = HALF_SCREEN_WIDTH + x2 * ViewingDistance / z2;
        y2 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y2 * ViewingDistance / z2;

        x3 = HALF_SCREEN_WIDTH + x3 * ViewingDistance / z3;
        y3 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y3 * ViewingDistance / z3;

        // draw triangle
        drawTri3DZ(
            (int)x1, (int)y1, (int)z1,
            (int)x2, (int)y2, (int)z2,
            (int)x3, (int)y3, (int)z3,
            WorldPolys[currPoly]->shade);

        // draw second poly if this is a quad
        if (isQuad) {
            // extract the point
            x4 = WorldPolys[currPoly]->vertexList[3].x;
            y4 = WorldPolys[currPoly]->vertexList[3].y;

            // project to screen
            x4 = HALF_SCREEN_WIDTH + x4 * ViewingDistance / z4;
            y4 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y4 * ViewingDistance / z4;

            // draw triangle
            drawTri3DZ(
                (int)x1, (int)y1, (int)z1,
                (int)x3, (int)y3, (int)z3,
                (int)x4, (int)y4, (int)z4,
                WorldPolys[currPoly]->shade);
        }
    }
}

int createZBuffer(unsigned int height) {
    // this function allocates the z buffer in two banks

    // set global z buffer values
    ZHeight = height;
    ZHeight2 = height / 2;
#ifdef DOS_32_BIT
    ZBankSize = ZHeight2 * (unsigned int)1280;  // 32-bit ints: 320 * 4 bytes
#else
    ZBankSize = ZHeight2 * (unsigned int)640;   // 16-bit ints: 320 * 2 bytes
#endif

    // allocate the memory
    ZBank1 = (int FAR*)MALLOC(ZBankSize);
    ZBank2 = (int FAR*)MALLOC(ZBankSize);

    // return success or failure
    if (ZBank1 && ZBank2) {
        return 1;
    } else {
        return 0;
    }
}

void deleteZBuffer(void) {
    // this function frees up the memory used by the z buffer memory banks
    FREE(ZBank1);
    FREE(ZBank2);
}

void fillZBuffer(int value) {
    // this function fills the entire z buffer (both banks) with the sent value
#ifdef DOS_32_BIT
    _asm {
        // bank 1
        mov edi, ZBank1     
        mov eax, value      // use 32-bit eax for dword value
        mov ecx, ZBankSize  
        shr ecx, 2          // divide by 4 (dwords instead of words)
        rep stosd           // store dwords (faster!)

        // bank 2
        mov edi, ZBank2     
        mov eax, value      
        mov ecx, ZBankSize  
        shr ecx, 2          
        rep stosd           
    }
#else
    _asm {
        // bank 1
        les di, ZBank1      // point es:di to z buffer bank 1
        mov ax, value       // move the value into ax
        mov cx, ZBankSize   // number of bytes to fill
        shr cx, 1           // convert to number of words
        rep stosw           // move the value into z buffer

        // bank 2
        les di, ZBank2      // point es:di to z buffer bank 2
        mov ax, value       // move the value into ax (redundant)
        mov cx, ZBankSize   // number of bytes to fill (so is this)
        shr cx, 1           // convert to number of words
        rep stosw           // move the value into z buffer
    }
#endif
}

void bspWorldToCamera(WallPtr root) {
}

void bspTranslate(WallPtr root, int xTrans, int yTrans, int zTrans) {
}

void bspShade(WallPtr root) {
}

void bspTraverse(WallPtr root) {
}

void bspDelete(WallPtr root) {
}

void bspPrint(WallPtr root) {
}

void bspView(WallPtr root) {
}

void buildBspTree(WallPtr root) {
}

void intersectLines(
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    float* xi,
    float* yi) {
}

