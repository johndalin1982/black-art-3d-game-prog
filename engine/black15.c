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

// ROW_OFFSET(y)/ROW_PITCH (the color-buffer addressing macros) come from
// black11.h - shared with black11.c, the file that first needed them.

// resolves to the current row's INT-element offset/pitch for the Z-buffer.
// The Z-buffer is always one int per pixel column, regardless of
// DisplayBpp, so it can't reuse ROW_OFFSET/ROW_PITCH above (those are
// byte-pitch-based, tied to the color buffer's bits-per-pixel).
#ifdef VBE_SUPPORT
#define ZROW_OFFSET(y) ((unsigned long)(y) * DisplayWidth)
#define ZROW_PITCH     DisplayWidth
#else
#define ZROW_OFFSET(y) (((y) << 6) + ((y) << 8))
#define ZROW_PITCH     320
#endif

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
    destAddr = DoubleBuffer + ROW_OFFSET(y1);

    // start z buffer at proper bank
    if (y1 < ZHeight2) {
        ZBuffer = ZBank1 + ZROW_OFFSET(y1);
    } else {
        tempY = y1 - ZHeight2;

        ZBuffer = ZBank2 + ZROW_OFFSET(tempY);
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
            destAddr += ROW_PITCH;
            ZBuffer += ZROW_PITCH;
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
            destAddr += ROW_PITCH;
            ZBuffer += ZROW_PITCH;
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

#ifdef VBE_SUPPORT
    resyncCachedSettings();
#endif

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

#ifdef VBE_SUPPORT
    resyncCachedSettings();
#endif

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
        x1 = HalfScreenWidth + x1 * ViewingDistance / z1;
        y1 = HalfScreenHeight - AspectRatio * y1 * ViewingDistance / z1;

        x2 = HalfScreenWidth + x2 * ViewingDistance / z2;
        y2 = HalfScreenHeight - AspectRatio * y2 * ViewingDistance / z2;

        x3 = HalfScreenWidth + x3 * ViewingDistance / z3;
        y3 = HalfScreenHeight - AspectRatio * y3 * ViewingDistance / z3;

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
            x4 = HalfScreenWidth + x4 * ViewingDistance / z4;
            y4 = HalfScreenHeight - AspectRatio * y4 * ViewingDistance / z4;

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
#ifdef VBE_SUPPORT
    // one int per pixel column, at the active display's width - not the
    // book's fixed 320
    ZBankSize = (unsigned int)(ZHeight2 * (unsigned long)DisplayWidth * sizeof(int));
#elif defined(DOS_32_BIT)
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
    // this function traverses the bsp tree and converts the world coordinates
    // to camera coordinates using the global transformation matrix note the
    // function is recursive and uses and inorder traversal, other traversals
    // such as preorder and postorder will would just as well...

    static int index; // looping variable

    // test if we have hit a dead end
    if (root == NULL) {
        return;
    }

    // transform back most sub-tree
    bspWorldToCamera(root->back);

    // iterate thru all vertices of current wall and transform them into
    // camera coordinates
    for (index = 0; index < 4; index++) {
        // multiply the point by the viewing transformation matrix

        // x component
        root->wallCamera[index].x =
            root->wallWorld[index].x * GlobalView[0][0] +
            root->wallWorld[index].y * GlobalView[1][0] +
            root->wallWorld[index].z * GlobalView[2][0] +
                                       GlobalView[3][0];

        // y component
        root->wallCamera[index].y =
            root->wallWorld[index].x * GlobalView[0][1] +
            root->wallWorld[index].y * GlobalView[1][1] +
            root->wallWorld[index].z * GlobalView[2][1] +
                                       GlobalView[3][1];

        // z component
        root->wallCamera[index].z =
            root->wallWorld[index].x * GlobalView[0][2] +
            root->wallWorld[index].y * GlobalView[1][2] +
            root->wallWorld[index].z * GlobalView[2][2] +
                                       GlobalView[3][2];
    }

    // transform front most sub-tree
    bspWorldToCamera(root->front);
}

void bspTranslate(WallPtr root, int xTrans, int yTrans, int zTrans) {
    // this function translates all the walls that make up the bsp world
    // note function is recursive, we don't really need this function, but
    // it's a good example of how we might perform transformations on the BSP
    // tree and similar tree like structures using recursion

    static int index; // looping variable

    // test if we have hit a dead end
    if (root == NULL) {
        return;
    }

    // translate back most sub-tree
    bspTranslate(root->back, xTrans, yTrans, zTrans);

    // iterate thru all vertices of current wall and translate them
    for (index = 0; index < 4; index++) {
        // perform translation
        root->wallWorld[index].x += xTrans;
        root->wallWorld[index].y += yTrans;
        root->wallWorld[index].z += zTrans;
    }

    // translate front most sub-tree
    bspTranslate(root->front, xTrans, yTrans, zTrans);
}

void bspShade(WallPtr root) {
    // this function shades the bsp tree and need only be called if the global
    // lightsource changes position

    static int index;   // looping variable

    static float normalLength,  // length of surface normal
                 intensity,     // intensity of light falling on surface being processed
                 dp;            // result of dot product

    // test if we have hit a dead end
    if (root == NULL) {
        return;
    }

    // shade the back most sub-tree
    bspShade(root->back);

    // compute the dot product between line of sight vector and normal to surface
    dp = dotProduct3D((Vector3DPtr)&root->normal, (Vector3DPtr)&LightSource);

    // compute length of normal of surface normal, remember this function
    // doesn't need to be time critical since it is only called once at startup
    // or whenever the light source moves
    normalLength = vectorMag3D((Vector3DPtr)&root->normal);

    // cos 0 = (u.v)/|u||v| or
    intensity = AmbientLight + (15 * dp / normalLength);

    // test if intensity has overflowed
    if (intensity > 15) {
        intensity = 15;
    }

    // intensity now varies from 0-1, 0 being black or grazing and 1 being
    // totally illuminated. use the value to index into color table
    root->color = BSP_WALL_SHADE - (int)(fabs(intensity));

    // shade the front most sub-tree
    bspShade(root->front);
}

void bspTraverse(WallPtr root) {
    // this function traverses the BSP tree and generates the polygon list used
    // by drawPolys() for the current global viewpoint (note the view angle is
    // irrelevant), also as the polygon list is being generated, only polygons
    // that are within the z extents are added to the polygon, in essence, the
    // function is performing Z clipping also, this is to minimize the amount
    // of polygons in the graphics pipeline that will have to be processed during
    // rendering

    // this function works by testing the viewpoint against the current wall
    // in the bsp, then depending on the side the viewpoint is the algorithm
    // proceeds. the search takes place as the rest in an "inorder" method
    // with hooks to process and add each node into the polygon list at the
    // right time

    static Vector3D testVector;

    static float dotWall,
                 z1, z2, z3, z4;

    // is this a dead end?
    if (root == NULL) {
        return;
    }

    // test which side viewpoint is on relative to the current wall
    makeVector3D((Point3DPtr)&root->wallWorld[0],
                 (Point3DPtr)&ViewPoint,
                 (Vector3DPtr)&testVector);

    // now dot test vector with the surface normal and analyze signs
    dotWall = dotProduct3D((Vector3DPtr)&testVector,
                           (Vector3DPtr)&root->normal);

    // if the sign of the dot product is positive then the viewer on on the
    // front side of current wall, so recursively process the walls behind then
    // in front of this wall, else do the opposite
    if (dotWall > 0) {
        // viewer is in front of this wall

        // process the back wall sub tree
        bspTraverse(root->back);

        // try to add this wall to the polygon list if it's within the Z extents
        z1 = root->wallCamera[0].z;
        z2 = root->wallCamera[1].z;
        z3 = root->wallCamera[2].z;
        z4 = root->wallCamera[3].z;

        // perform the z extents clipping test
        if ((z1 > ClipNearZ && z1 < ClipFarZ) || (z2 > ClipNearZ && z2 < ClipFarZ) ||
            (z3 > ClipNearZ && z3 < ClipFarZ) || (z4 > ClipNearZ && z4 < ClipFarZ)) {

            // first copy data and vertices into an open slot in storage area
            WorldPolyStorage[NumPolysFrame].numPoints = 4;
            WorldPolyStorage[NumPolysFrame].color     = BSP_WALL_COLOR;
            WorldPolyStorage[NumPolysFrame].shade     = root->color;
            WorldPolyStorage[NumPolysFrame].shading   = 0;
            WorldPolyStorage[NumPolysFrame].twoSided  = 1;
            WorldPolyStorage[NumPolysFrame].visible   = 1;
            WorldPolyStorage[NumPolysFrame].clipped   = 0;
            WorldPolyStorage[NumPolysFrame].active    = 1;

            // now copy vertices
            WorldPolyStorage[NumPolysFrame].vertexList[0].x = root->wallCamera[0].x;
            WorldPolyStorage[NumPolysFrame].vertexList[0].y = root->wallCamera[0].y;
            WorldPolyStorage[NumPolysFrame].vertexList[0].z = root->wallCamera[0].z;

            WorldPolyStorage[NumPolysFrame].vertexList[1].x = root->wallCamera[1].x;
            WorldPolyStorage[NumPolysFrame].vertexList[1].y = root->wallCamera[1].y;
            WorldPolyStorage[NumPolysFrame].vertexList[1].z = root->wallCamera[1].z;

            WorldPolyStorage[NumPolysFrame].vertexList[2].x = root->wallCamera[2].x;
            WorldPolyStorage[NumPolysFrame].vertexList[2].y = root->wallCamera[2].y;
            WorldPolyStorage[NumPolysFrame].vertexList[2].z = root->wallCamera[2].z;

            WorldPolyStorage[NumPolysFrame].vertexList[3].x = root->wallCamera[3].x;
            WorldPolyStorage[NumPolysFrame].vertexList[3].y = root->wallCamera[3].y;
            WorldPolyStorage[NumPolysFrame].vertexList[3].z = root->wallCamera[3].z;

            // assign poly list pointer to it
            WorldPolys[NumPolysFrame] = &WorldPolyStorage[NumPolysFrame];

            // increment number of polys in this frame
            NumPolysFrame++;
        }

        // now process the front walls sub tree
        bspTraverse(root->front);
    } else {
        // viewer is behind this wall

        // process the front wall sub tree
        bspTraverse(root->front);

        // try to add this wall to the polygon list if it's within the Z extents
        z1 = root->wallCamera[0].z;
        z2 = root->wallCamera[1].z;
        z3 = root->wallCamera[2].z;
        z4 = root->wallCamera[3].z;

        // perform the z extents clipping test
        if ((z1 > ClipNearZ && z1 < ClipFarZ) || (z2 > ClipNearZ && z2 < ClipFarZ) ||
            (z3 > ClipNearZ && z3 < ClipFarZ) || (z4 > ClipNearZ && z4 < ClipFarZ)) {

            // first copy data and vertices into an open slot in storage area
            WorldPolyStorage[NumPolysFrame].numPoints = 4;
            WorldPolyStorage[NumPolysFrame].color     = BSP_WALL_COLOR;
            WorldPolyStorage[NumPolysFrame].shade     = root->color;
            WorldPolyStorage[NumPolysFrame].shading   = 0;
            WorldPolyStorage[NumPolysFrame].twoSided  = 1;
            WorldPolyStorage[NumPolysFrame].visible   = 1;
            WorldPolyStorage[NumPolysFrame].clipped   = 0;
            WorldPolyStorage[NumPolysFrame].active    = 1;

            // now copy vertices, note that we don't use a structure copy, it's
            // not dependable
            WorldPolyStorage[NumPolysFrame].vertexList[0].x = root->wallCamera[0].x;
            WorldPolyStorage[NumPolysFrame].vertexList[0].y = root->wallCamera[0].y;
            WorldPolyStorage[NumPolysFrame].vertexList[0].z = root->wallCamera[0].z;

            WorldPolyStorage[NumPolysFrame].vertexList[1].x = root->wallCamera[1].x;
            WorldPolyStorage[NumPolysFrame].vertexList[1].y = root->wallCamera[1].y;
            WorldPolyStorage[NumPolysFrame].vertexList[1].z = root->wallCamera[1].z;

            WorldPolyStorage[NumPolysFrame].vertexList[2].x = root->wallCamera[2].x;
            WorldPolyStorage[NumPolysFrame].vertexList[2].y = root->wallCamera[2].y;
            WorldPolyStorage[NumPolysFrame].vertexList[2].z = root->wallCamera[2].z;

            WorldPolyStorage[NumPolysFrame].vertexList[3].x = root->wallCamera[3].x;
            WorldPolyStorage[NumPolysFrame].vertexList[3].y = root->wallCamera[3].y;
            WorldPolyStorage[NumPolysFrame].vertexList[3].z = root->wallCamera[3].z;

            // assign poly list pointer to it
            WorldPolys[NumPolysFrame] = &WorldPolyStorage[NumPolysFrame];

            // increment number of polys in this frame
            NumPolysFrame++;
        }

        // now process the back walls sub tree
        bspTraverse(root->back);
    }
}

void bspDelete(WallPtr root) {
    // this function recursively deletes all the nodes in the bsp tree and frees
    // the memory back to the OS.

    WallPtr tempWall;   // a temporary wall

    // test if we have hit a dead end
    if (root == NULL) {
        return;
    }

    // delete back sub tree
    bspDelete(root->back);

    // delete this node, but first save the front sub-tree
    tempWall = root->front;

    // delete the memory
    free(root);

    // assign the root to the saved front most sub-tree
    root = tempWall;

    // delete front sub tree
    bspDelete(root);
}

void bspPrint(WallPtr root) {
    // this function performs a recursive in-order traversal of the BSP tree and
    // prints the results out to the file opened with FpOut as the handle

    // test if this child is null
    if (root == NULL) {
        fprintf(FpOut, "\nReached NULL node returning...");
        return;
    }

    // search left tree (back walls)
    fprintf(FpOut, "\nTraversing back sub-tree...");

    bspPrint(root->back);

    // visit node
    fprintf(FpOut, "\n\n\nWall ID #%d", root->id);
    fprintf(FpOut, "\nVertices...");
    fprintf(FpOut, "\nVertex 0: (%f,%f,%f)", root->wallWorld[0].x,
                                             root->wallWorld[0].y,
                                             root->wallWorld[0].z);

    fprintf(FpOut, "\nVertex 1: (%f,%f,%f)", root->wallWorld[1].x,
                                             root->wallWorld[1].y,
                                             root->wallWorld[1].z);

    fprintf(FpOut, "\nVertex 2: (%f,%f,%f)", root->wallWorld[2].x,
                                             root->wallWorld[2].y,
                                             root->wallWorld[2].z);

    fprintf(FpOut, "\nVertex 3: (%f,%f,%f)", root->wallWorld[3].x,
                                             root->wallWorld[3].y,
                                             root->wallWorld[3].z);

    fprintf(FpOut, "\nNormal (%f,%f,%f)", root->normal.x,
                                          root->normal.y,
                                          root->normal.z);

    fprintf(FpOut, "\nEnd wall data\n");

    // search right tree (front walls)
    fprintf(FpOut, "\nTraversing front sub-tree..");

    bspPrint(root->front);
}

void bspView(WallPtr root) {
    // this function is a self contained viewing processor that has it's own event
    // loop, the display will continue to be generated until the ESC key is pressed

    int done = 0;

    // install the isr keyboard driver
    keyboardInstallDriver();

    // change the light source direction
    LightSource.x = (float) 0.398636;
    LightSource.y = (float)-0.374248;
    LightSource.z = (float) 0.8372275;

    // reset viewpoint to (0,0,0)
    ViewPoint.x = 0;
    ViewPoint.y = 0;
    ViewPoint.z = 0;

    // main event loop
    while (!done) {
        // compute starting time of this frame
        StartingTime = timerQuery();

        // erase all objects
        fillDoubleBuffer(0);

        // move viewpoint
        if (KeyboardState[MAKE_UP]) {
            ViewPoint.y += 20;
        }

        if (KeyboardState[MAKE_DOWN]) {
            ViewPoint.y -= 20;
        }

        if (KeyboardState[MAKE_RIGHT]) {
            ViewPoint.x += 20;
        }

        if (KeyboardState[MAKE_LEFT]) {
            ViewPoint.x -= 20;
        }

        if (KeyboardState[MAKE_KEYPAD_PLUS]) {
            ViewPoint.z += 20;
        }

        if (KeyboardState[MAKE_KEYPAD_MINUS]) {
            ViewPoint.z -= 20;
        }

        if (KeyboardState[MAKE_Z]) {
            if ((ViewAngle.angX += 10) > 360) {
                ViewAngle.angX = 0;
            }
        }

        if (KeyboardState[MAKE_A]) {
            if ((ViewAngle.angX -= 10) < 0) {
                ViewAngle.angX = 360;
            }
        }

        if (KeyboardState[MAKE_X]) {
            if ((ViewAngle.angY += 10) > 360) {
                ViewAngle.angY = 0;
            }
        }

        if (KeyboardState[MAKE_S]) {
            if ((ViewAngle.angY -= 10) < 0) {
                ViewAngle.angY = 360;
            }
        }

        if (KeyboardState[MAKE_C]) {
            if ((ViewAngle.angZ += 10) > 360) {
                ViewAngle.angZ = 0;
            }
        }

        if (KeyboardState[MAKE_D]) {
            if ((ViewAngle.angZ -= 10) < 0) {
                ViewAngle.angZ = 360;
            }
        }

        if (KeyboardState[MAKE_ESC]) {
            done = 1;
        }

        // now that user has possible moved viewpoint, create the global
        // world to camera transformation matrix
        createWorldToCamera();

        // now convert the bsp tree world coordinates into camera coordinates
        bspWorldToCamera(root);

        // reset number of polygons in polygon list
        NumPolysFrame = 0;

        // traverse the BSP tree and generate the polygon list
        bspTraverse(root);

        // draw the polygon list generated by traversing the BSP tree
        drawPolyList();

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        while ((timerQuery() - StartingTime) < 1);
    }

    // restore the old keyboard driver
    keyboardRemoveDriver();
}

void buildBspTree(WallPtr root) {
    // this function recursively builds the bsp tree from the sent wall list
    // note the function has some calls to drawLine() and a timeDelay() at
    // the end, these are for illustrative purposes only for the demo interface
    // and should be removed if you wish to use this function in a real
    // application

    static WallPtr nextWall,    // pointer to next wall to be processed
                   frontWall,   // the front wall
                   backWall,    // the back wall
                   tempWall;    // a temporary wall

    static float dotWall1,                  // dot products for test wall
                 dotWall2,
                 wallX0, wallY0, wallZ0,    // working vars for test wall
                 wallX1, wallY1, wallZ1,
                 ppX0, ppY0, ppZ0,          // working vars for partitioning plane
                 ppX1, ppY1, ppZ1,
                 xi, zi;                    // points of intersection when the partitioning
                                            // plane cuts a wall in two

    static Vector3D testVector1,    // test vectors from the partitioning plane
                    testVector2;    // to the test wall to test the side
                                    // of the partitioning plane the test wall
                                    // lies on

    static int frontFlag = 0,   // flags if a wall is on the front or back
               backFlag = 0,    // of the partitioning plane
               index;           // looping index

    // test if this tree is complete
    if (root == NULL) {
        return;
    }

    // the root is the partitioning plane, partition the polygons using it
    nextWall   = root->link;
    root->link = NULL;

    // extract top two vertices of partitioning plane wall for ease of calculations
    ppX0 = root->wallWorld[0].x;
    ppY0 = root->wallWorld[0].y;
    ppZ0 = root->wallWorld[0].z;

    ppX1 = root->wallWorld[1].x;
    ppY1 = root->wallWorld[1].y;
    ppZ1 = root->wallWorld[1].z;

    // highlight space partition green
    drawLine(ppX0 / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
             ppZ0 / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
             ppX1 / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
             ppZ1 / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
             10,
             VideoBuffer);

    // test if all walls have been partitioned
    while (nextWall) {
        // test which side test wall is relative to partitioning plane
        // defined by root

        // first compute vectors from point on partitioning plane to point on
        // test wall
        makeVector3D((Point3DPtr)&root->wallWorld[0],
                     (Point3DPtr)&nextWall->wallWorld[0],
                     (Vector3DPtr)&testVector1);

        makeVector3D((Point3DPtr)&root->wallWorld[0],
                     (Point3DPtr)&nextWall->wallWorld[1],
                     (Vector3DPtr)&testVector2);

        // now dot each test vector with the surface normal and analyze signs
        dotWall1 = dotProduct3D((Vector3DPtr)&testVector1,
                                (Vector3DPtr)&root->normal);

        dotWall2 = dotProduct3D((Vector3DPtr)&testVector2,
                                (Vector3DPtr)&root->normal);

        // perform the tests

        // case 0, the partitioning plane and the test wall have a point in common
        // this is a special case and must be accounted for, shorten the code
        // we will set a pair of flags and then the next case will handle
        // the actual insertion of the wall into BSP

        // reset flags
        frontFlag = backFlag = 0;

        // determine of wall is tangent to endpoints of partitioning wall
        if (POINTS_EQUAL_3D(root->wallWorld[0], nextWall->wallWorld[0])) {
            // p0 of partitioning plane is the same at p0 of test wall
            // we only need to see what side p1 of test wall in on
            if (dotWall2 > 0) {
                frontFlag = 1;
            } else {
                backFlag = 1;
            }
        } else if (POINTS_EQUAL_3D(root->wallWorld[0], nextWall->wallWorld[1])) {
            // p0 of partitioning plane is the same at p1 of test wall
            // we only need to see what side p0 of test wall in on
            if (dotWall1 > 0) {
                frontFlag = 1;
            } else {
                backFlag = 1;
            }
        } else if (POINTS_EQUAL_3D(root->wallWorld[1], nextWall->wallWorld[0])) {
            // p1 of partitioning plane is the same at p0 of test wall
            // we only need to see what side p1 of test wall in on
            if (dotWall2 > 0) {
                frontFlag = 1;
            } else {
                backFlag = 1;
            }
        } else if (POINTS_EQUAL_3D(root->wallWorld[1], nextWall->wallWorld[1])) {
            // p1 of partitioning plane is the same at p1 of test wall
            // we only need to see what side p0 of test wall in on
            if (dotWall1 > 0) {
                frontFlag = 1;
            } else {
                backFlag = 1;
            }
        }

        // case 1 both signs are the same or the front or back flag has been set
        if ((dotWall1 >= 0 && dotWall2 >= 0) || frontFlag) {
            // highlight the wall blue
            drawLine(nextWall->wallWorld[0].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                     nextWall->wallWorld[0].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                     nextWall->wallWorld[1].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                     nextWall->wallWorld[1].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                     9,
                     VideoBuffer);

            // place this wall on the front list
            if (root->front == NULL) {
                // this is the first node
                root->front     = nextWall;
                nextWall        = nextWall->link;
                frontWall       = root->front;
                frontWall->link = NULL;
            } else {
                // this is the nth node
                frontWall->link = nextWall;
                nextWall        = nextWall->link;
                frontWall       = frontWall->link;
                frontWall->link = NULL;
            }
        } else if ((dotWall1 < 0 && dotWall2 < 0) || backFlag) {
            // highlight the wall red
            drawLine(nextWall->wallWorld[0].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                     nextWall->wallWorld[0].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                     nextWall->wallWorld[1].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                     nextWall->wallWorld[1].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                     12,
                     VideoBuffer);

            // place this wall on the back list
            if (root->back == NULL) {
                // this is the first node
                root->back     = nextWall;
                nextWall       = nextWall->link;
                backWall       = root->back;
                backWall->link = NULL;
            } else {
                // this is the nth node
                backWall->link = nextWall;
                nextWall       = nextWall->link;
                backWall       = backWall->link;
                backWall->link = NULL;
            }
        }

        // case 2 both signs are different
        else if ((dotWall1 < 0 && dotWall2 >= 0) ||
                 (dotWall1 >= 0 && dotWall2 < 0)) {
            // the partitioning plane cuts the wall in half, the wall
            // must be split into two walls

            // extract top two vertices of test wall for ease of calculations
            wallX0 = nextWall->wallWorld[0].x;
            wallY0 = nextWall->wallWorld[0].y;
            wallZ0 = nextWall->wallWorld[0].z;

            wallX1 = nextWall->wallWorld[1].x;
            wallY1 = nextWall->wallWorld[1].y;
            wallZ1 = nextWall->wallWorld[1].z;

            // compute the point of intersection between the walls
            // note that x and z are the plane that the intersection takes place in
            intersectLines(wallX0, wallZ0, wallX1, wallZ1,
                           ppX0, ppZ0, ppX1, ppZ1,
                           &xi, &zi);

            // here comes the tricky part, we need to split the wall in half and
            // create two walls. We'll do this by creating two new walls,
            // placing them on the appropriate front and back lists and
            // then deleting the original wall

            // process first wall

            // allocate the memory for the wall
            tempWall = (WallPtr)malloc(sizeof(Wall));

            tempWall->front = NULL;
            tempWall->back  = NULL;
            tempWall->link  = NULL;

            tempWall->normal = nextWall->normal;
            tempWall->id     = nextWall->id + 1000; // add 1000 to denote a split

            // compute wall vertices
            for (index = 0; index < 4; index++) {
                tempWall->wallWorld[index].x = nextWall->wallWorld[index].x;
                tempWall->wallWorld[index].y = nextWall->wallWorld[index].y;
                tempWall->wallWorld[index].z = nextWall->wallWorld[index].z;
            }

            // now modify vertices 1 and 2 to reflect intersection point
            // but leave y alone since it's invariant for the wall splitting
            tempWall->wallWorld[1].x = xi;
            tempWall->wallWorld[1].z = zi;

            tempWall->wallWorld[2].x = xi;
            tempWall->wallWorld[2].z = zi;

            // insert new wall into front or back of root
            if (dotWall1 >= 0) {
                // highlight the wall blue
                drawLine(tempWall->wallWorld[0].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                         tempWall->wallWorld[0].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                         tempWall->wallWorld[1].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                         tempWall->wallWorld[1].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                         9,
                         VideoBuffer);

                // place this wall on the front list
                if (root->front == NULL) {
                    // this is the first node
                    root->front     = tempWall;
                    frontWall       = root->front;
                    frontWall->link = NULL;
                } else {
                    // this is the nth node
                    frontWall->link = tempWall;
                    frontWall       = frontWall->link;
                    frontWall->link = NULL;
                }
            } else if (dotWall1 < 0) {
                // highlight the wall red
                drawLine(tempWall->wallWorld[0].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                         tempWall->wallWorld[0].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                         tempWall->wallWorld[1].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                         tempWall->wallWorld[1].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                         12,
                         VideoBuffer);

                // place this wall on the back list
                if (root->back == NULL) {
                    // this is the first node
                    root->back     = tempWall;
                    backWall       = root->back;
                    backWall->link = NULL;
                } else {
                    // this is the nth node
                    backWall->link = tempWall;
                    backWall       = backWall->link;
                    backWall->link = NULL;
                }
            }

            // process second wall

            // allocate the memory for the wall
            tempWall = (WallPtr)malloc(sizeof(Wall));

            tempWall->front = NULL;
            tempWall->back  = NULL;
            tempWall->link  = NULL;

            tempWall->normal = nextWall->normal;
            tempWall->id     = nextWall->id + 1000;

            // compute wall vertices
            for (index = 0; index < 4; index++) {
                tempWall->wallWorld[index].x = nextWall->wallWorld[index].x;
                tempWall->wallWorld[index].y = nextWall->wallWorld[index].y;
                tempWall->wallWorld[index].z = nextWall->wallWorld[index].z;
            }

            // now modify vertices 0 and 3 to reflect intersection point
            // but leave y alone since it's invariant for the wall splitting
            tempWall->wallWorld[0].x = xi;
            tempWall->wallWorld[0].z = zi;

            tempWall->wallWorld[3].x = xi;
            tempWall->wallWorld[3].z = zi;

            // insert new wall into front or back of root
            if (dotWall2 >= 0) {
                // highlight the wall blue
                drawLine(tempWall->wallWorld[0].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                         tempWall->wallWorld[0].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                         tempWall->wallWorld[1].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                         tempWall->wallWorld[1].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                         9,
                         VideoBuffer);

                // place this wall on the front list
                if (root->front == NULL) {
                    // this is the first node
                    root->front     = tempWall;
                    frontWall       = root->front;
                    frontWall->link = NULL;
                } else {
                    // this is the nth node
                    frontWall->link = tempWall;
                    frontWall       = frontWall->link;
                    frontWall->link = NULL;
                }
            } else if (dotWall2 < 0) {
                // highlight the wall red
                drawLine(tempWall->wallWorld[0].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                         tempWall->wallWorld[0].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                         tempWall->wallWorld[1].x / WORLD_SCALE_X - SCREEN_TO_WORLD_X,
                         tempWall->wallWorld[1].z / WORLD_SCALE_Z - SCREEN_TO_WORLD_Z,
                         12,
                         VideoBuffer);

                // place this wall on the back list
                if (root->back == NULL) {
                    // this is the first node
                    root->back     = tempWall;
                    backWall       = root->back;
                    backWall->link = NULL;
                } else {
                    // this is the nth node
                    backWall->link = tempWall;
                    backWall       = backWall->link;
                    backWall->link = NULL;
                }
            }

            // we are now done splitting the wall, so we can delete it
            tempWall = nextWall;
            nextWall = nextWall->link;
            free(tempWall);
        }
    }

    // delay a bit so user can see BSP being created
    timeDelay(5);

    // recursively process front and back walls
    buildBspTree(root->front);

    buildBspTree(root->back);
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

    // this function computes the intersection of the sent lines
    // and returns the intersection point, note that the function assumes
    // the lines intersect. the function can handle vertical as well
    // as horizontal lines. note the function isn't very clever, it simply applies
    // the math, but we don't need speed since this is a pre-processing step

    float a1, b1, c1,   // constants of linear equations
          a2, b2, c2,
          detInv,       // the inverse of the determinant of the coefficient matrix
          m1, m2;       // the slopes of each line

    // compute slopes, note the kludge for infinity, however, this will
    // be close enough
    if ((x1 - x0) != 0) {
        m1 = (y1 - y0) / (x1 - x0);
    } else {
        m1 = (float)1e+10;  // close enough to infinity
    }

    if ((x3 - x2) != 0) {
        m2 = (y3 - y2) / (x3 - x2);
    } else {
        m2 = (float)1e+10;  // close enough to infinity
    }

    // compute constants
    a1 = m1;
    a2 = m2;

    b1 = -1;
    b2 = -1;

    c1 = (y0 - m1 * x0);
    c2 = (y2 - m2 * x2);

    // compute the inverse of the determinate
    detInv = 1 / (a1 * b2 - a2 * b1);

    // use Kramers rule to compute xi and yi
    *xi = ((b1 * c2 - b2 * c1) * detInv);
    *yi = ((a2 * c1 - a1 * c2) * detInv);
}

