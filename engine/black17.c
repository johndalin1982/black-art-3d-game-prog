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

// include all of our stuff

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black17.h"

float ClipNearZ    = 100,   // the near or hither clipping plane
      ClipFarZ     = 3200,  // the far or yon clipping plane
      ScreenWidth  = 320,   // dimensions of the screen
      ScreenHeight = 200;

float ViewingDistance = 200;        // distance of projection plane from camera

Point3D ViewPoint = { 0, 0, 0, 1 }; // position of camera

Vector3D LightSource = { -0.913913, 0.389759, -0.113369 }; // position of point light source

float AmbientLight = 6;             // ambient light level

Dir3D ViewAngle = { 0, 0, 0 };      // angle of camera

Matrix4x4 GlobalView;               // the global inverse world to camera matrix

RgbPalette ColorPalette3D;          // the color palette used for the 3D system

int NumObjects;                     // number of objects in the world

ObjectPtr WorldObjectList[MAX_OBJECTS];     // the objects in the world

int NumPolysFrame;                          // the number of polys in this frame

FacetPtr WorldPolys[MAX_POLYS_PER_FRAME];   // the visible polygons for this frame

Facet WorldPolyStorage[MAX_POLYS_PER_FRAME]; // the storage for the visible
                                             // polygons is pre-allocated
                                             // so it doesn't need to be
                                             // allocated frame by frame

// look up tables

float SinLook[360 + 1], // SIN from 0 to 360
      CosLook[360 + 1]; // COSINE from 0 to 360

// the clipping region, set it to default on start up

int PolyClipMinX = POLY_CLIP_MIN_X, // the minimum boundaries
    PolyClipMinY = POLY_CLIP_MIN_Y,

    PolyClipMaxX = POLY_CLIP_MAX_X, // the maximum boundaries
    PolyClipMaxY = POLY_CLIP_MAX_Y;


Sprite Textures;    // this holds the textures

void triangleLine(
    unsigned char FAR* destAddr,
    unsigned int xs,
    unsigned int xe,
    int color) {

    // this function draws a fast horizontal line by using WORD size writes
    // the assembly language version is called triangle16Line()

    // plot pixels at ends of line first
    if (xs & 0x0001) {
        // plot a single starting pixel and move xs to an even boundary
        destAddr[xs++] = (unsigned char)color;
    }

    if (!(xe & 0x0001)) {
        // plot a single terminating pixel and move xe back to an odd boundary
        destAddr[xe--] = (unsigned char)color;
    }

    // now plot the line
    destAddr += xs;

    // now blast the middle part of the line a WORD at a time, later use
    // an external assembly program to do it a DOUBLE WORD at a time!
#ifdef DOS_32_BIT
    _asm {
        mov edi, destAddr       // point edi at data area (flat 32-bit)
        mov al, BYTE PTR color  // move into al and ah the color
        mov ah, al
        mov ecx, xe             // compute number of words to move (xe-xs+1)/2
        sub ecx, xs
        inc ecx
        shr ecx, 1              // divide by 2
        rep stosw               // draw the line
    }
#else
    _asm {
        les di, destAddr        // point es:di at data area
        mov al, BYTE PTR color  // move into al and ah the color
        mov ah, al
        mov cx, xe              // compute number of words to move (xe-xs+1)/2
        sub cx, xs
        inc cx
        shr cx, 1               // divide by 2
        rep stosw               // draw the line
    }
#endif
}

void triangleQLine(
    unsigned char FAR* destAddr,
    unsigned int xs,
    unsigned int xe,
    unsigned int color) {

    // this is the C version of the quad byte line renderer, not much faster than
    // the word version...not worth the extra complexity. the assembly language
    // version is called triangle32Line()

    long length,
         lcolor;

    // test special cases
    switch (xe - xs) {
        case 0: {
            destAddr[xs] = color;
            return;
        } break;

        case 1: {
            destAddr[xs++] = color;
            destAddr[xs]   = color;
            return;
        } break;

        case 2: {
            destAddr[xs++] = color;
            destAddr[xs++] = color;
            destAddr[xs]   = color;
            return;
        } break;

        case 3: {
            destAddr[xs++] = color;
            destAddr[xs++] = color;
            destAddr[xs++] = color;
            destAddr[xs]   = color;
            return;
        } break;

        default: break;
    }

    // line is longer than 4 pixels, so we can use algorithm

    // process left side
    switch (xs & 0x03) {
        case 1: {
            destAddr[xs++] = color;
            destAddr[xs++] = color;
            destAddr[xs++] = color;
        } break;

        case 2: {
            destAddr[xs++] = color;
            destAddr[xs++] = color;
        } break;

        case 3: {
            destAddr[xs++] = color;
        } break;

        default: break;
    }

    // process right side
    switch (xe & 0x03) {
        case 0: {
            destAddr[xe--] = color;
        } break;

        case 1: {
            destAddr[xe--] = color;
            destAddr[xe--] = color;
        } break;

        case 2: {
            destAddr[xe--] = color;
            destAddr[xe--] = color;
            destAddr[xe--] = color;
        } break;

        default: break;
    }

    // draw the middle part of the line now using words

    // now plot the line
    destAddr += xs;

    lcolor = (color << 8 | color);
    lcolor = (lcolor << 16 | lcolor);
    length = ((xe - xs + 1) >> 2);

    fquadset(destAddr, lcolor, length);
}

void drawTopTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int color) {
    // this function draws a triangle that has a flat top

    float dxRight,  // the dx/dy ratio of the right edge of line
          dxLeft,   // the dx/dy ratio of the left edge of line
          xs, xe,   // the starting and ending points of the edges
          height;   // the height of the triangle

    int tempX,      // used during sorting as temps
        tempY,
        right,      // used by clipping
        left;

    unsigned char FAR* destAddr;

    // test order of x1 and x2
    if (x2 < x1) {
        tempX = x2;
        x2    = x1;
        x1    = tempX;
    }

    // compute deltas
    height = y3 - y1;

    dxLeft  = (x3 - x1) / height;
    dxRight = (x3 - x2) / height;

    // set starting points
    xs = (float)x1;
    xe = (float)x2 + (float)0.5;

    // perform y clipping
    if (y1 < PolyClipMinY) {
        // compute new xs and ys
        xs = xs + dxLeft  * (float)(-y1 + PolyClipMinY);
        xe = xe + dxRight * (float)(-y1 + PolyClipMinY);

        // reset y1
        y1 = PolyClipMinY;
    }

    if (y3 > PolyClipMaxY) {
        y3 = PolyClipMaxY;
    }

    // compute starting address in video memory
    destAddr = DoubleBuffer + (y1 << 8) + (y1 << 6);

    // test if x clipping is needed
    if (x1 >= PolyClipMinX && x1 <= PolyClipMaxX &&
        x2 >= PolyClipMinX && x2 <= PolyClipMaxX &&
        x3 >= PolyClipMinX && x3 <= PolyClipMaxX) {

        // draw the triangle
#if 0
        for (tempY = y1; tempY <= y3; tempY++, destAddr += 320) {
            triangleLine(destAddr, (unsigned int)xs, (unsigned int)xe, color);

            // adjust starting point and ending point
            xs += dxLeft;
            xe += dxRight;
        }
#endif

        // use the external assembly language triangle engine based on fixed point
        if (y3 > y1) {
            triangleAsm(destAddr, y1, y3, xs, xe, dxLeft, dxRight, color);
        }
    } else {
        // clip x axis with slower version

        // draw the triangle
        for (tempY = y1; tempY <= y3; tempY++, destAddr += 320) {
            // do x clip
            left  = (int)xs;
            right = (int)xe;

            // adjust starting point and ending point
            xs += dxLeft;
            xe += dxRight;

            // clip line
            if (left < PolyClipMinX) {
                left = PolyClipMinX;

                if (right < PolyClipMinX) {
                    continue;
                }
            }

            if (right > PolyClipMaxX) {
                right = PolyClipMaxX;

                if (left > PolyClipMaxX) {
                    continue;
                }
            }

            triangle16Line(destAddr, (unsigned int)left, (unsigned int)right, color);
        }
    }
}

void drawBottomTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int color) {
    // this function draws a triangle that has a flat bottom

    float dxRight,  // the dx/dy ratio of the right edge of line
          dxLeft,   // the dx/dy ratio of the left edge of line
          xs, xe,   // the starting and ending points of the edges
          height;   // the height of the triangle

    int tempX,      // used during sorting as temps
        tempY,
        right,      // used by clipping
        left;

    unsigned char FAR* destAddr;

    // test order of x1 and x2
    if (x3 < x2) {
        tempX = x2;
        x2    = x3;
        x3    = tempX;
    }

    // compute deltas
    height = y3 - y1;

    dxLeft  = (x2 - x1) / height;
    dxRight = (x3 - x1) / height;

    // set starting points
    xs = (float)x1;
    xe = (float)x1 + (float)0.5;

    // perform y clipping
    if (y1 < PolyClipMinY) {
        // compute new xs and ys
        xs = xs + dxLeft  * (float)(-y1 + PolyClipMinY);
        xe = xe + dxRight * (float)(-y1 + PolyClipMinY);

        // reset y1
        y1 = PolyClipMinY;
    }

    if (y3 > PolyClipMaxY) {
        y3 = PolyClipMaxY;
    }

    // compute starting address in video memory
    destAddr = DoubleBuffer + (y1 << 8) + (y1 << 6);

    // test if x clipping is needed
    if (x1 >= PolyClipMinX && x1 <= PolyClipMaxX &&
        x2 >= PolyClipMinX && x2 <= PolyClipMaxX &&
        x3 >= PolyClipMinX && x3 <= PolyClipMaxX) {

        // draw the triangle
#if 0
        for (tempY = y1; tempY <= y3; tempY++, destAddr += 320) {
            triangle16Line(destAddr, (unsigned int)xs, (unsigned int)xe, color);

            // adjust starting point and ending point
            xs += dxLeft;
            xe += dxRight;
        }
#endif

        // use the external assembly language triangle engine based on fixed point
        if (y3 > y1) {
            triangleAsm(destAddr, y1, y3, xs, xe, dxLeft, dxRight, color);
        }
    } else {
        // clip x axis with slower version

        // draw the triangle
        for (tempY = y1; tempY <= y3; tempY++, destAddr += 320) {
            // do x clip
            left  = (int)xs;
            right = (int)xe;

            // adjust starting point and ending point
            xs += dxLeft;
            xe += dxRight;

            // clip line
            if (left < PolyClipMinX) {
                left = PolyClipMinX;

                if (right < PolyClipMinX) {
                    continue;
                }
            }

            if (right > PolyClipMaxX) {
                right = PolyClipMaxX;

                if (left > PolyClipMaxX) {
                    continue;
                }
            }

            triangle16Line(destAddr, (unsigned int)left, (unsigned int)right, color);
        }
    }
}

void drawTriangle2D(
    int x1, int y1,
    int x2, int y2,
    int x3, int y3,
    int color) {

    int tempX,
        tempY,
        newX;

    // test for h lines and v lines
    if ((x1 == x2 && x2 == x3) || (y1 == y2 && y2 == y3)) {
        return;
    }

    // sort p1,p2,p3 in ascending y order
    if (y2 < y1) {
        tempX = x2;
        tempY = y2;
        x2    = x1;
        y2    = y1;
        x1    = tempX;
        y1    = tempY;
    }

    // now we know that p1 and p2 are in order
    if (y3 < y1) {
        tempX = x3;
        tempY = y3;
        x3    = x1;
        y3    = y1;
        x1    = tempX;
        y1    = tempY;
    }

    // finally test y3 against y2
    if (y3 < y2) {
        tempX = x3;
        tempY = y3;
        x3    = x2;
        y3    = y2;
        x2    = tempX;
        y2    = tempY;
    }

    // do trivial rejection tests
    if (y3 < PolyClipMinY || y1 > PolyClipMaxY ||
        (x1 < PolyClipMinX && x2 < PolyClipMinX && x3 < PolyClipMinX) ||
        (x1 > PolyClipMaxX && x2 > PolyClipMaxX && x3 > PolyClipMaxX)) {
        return;
    }

    // test if top of triangle is flat
    if (y1 == y2) {
        drawTopTriangle(x1, y1, x2, y2, x3, y3, color);
    } else if (y2 == y3) {
        drawBottomTriangle(x1, y1, x2, y2, x3, y3, color);
    } else {
        // general triangle that's needs to be broken up along long edge
        newX = x1 + (int)((float)(y2 - y1) * (float)(x3 - x1) / (float)(y3 - y1));

        // draw each sub-triangle
        drawBottomTriangle(x1, y1, newX, y2, x2, y2, color);

        drawTopTriangle(x2, y2, newX, y2, x3, y3, color);
    }
}

void buildLookUpTables(void) {
    // this function builds all the look up tables for the engine

    int angle;  // the current angle being computed
    float rad;  // used in conversion from degrees to radians

    // generate sin/cos look up tables
    for (angle = 0; angle <= 360; angle++) {
        rad = (float)(3.14159 * (float)angle / (float)180);

        CosLook[angle] = (float)cos(rad);
        SinLook[angle] = (float)sin(rad);
    }
}

float dotProduct3D(Vector3DPtr u, Vector3DPtr v) {
    // this function computes the dot product of two vectors
    return (u->x * v->x) + (u->y * v->y) + (u->z * v->z);
}

void makeVector3D(
    Point3DPtr init,
    Point3DPtr term,
    Vector3DPtr result) {

    // this function creates a vector from two points in 3D space
    result->x = term->x - init->x;
    result->y = term->y - init->y;
    result->z = term->z - init->z;
}

void crossProduct3D(
    Vector3DPtr u,
    Vector3DPtr v,
    Vector3DPtr normal) {

    // this function computes the cross product between two vectors
    normal->x =  (u->y * v->z - u->z * v->y);
    normal->y = -(u->x * v->z - u->z * v->x);
    normal->z =  (u->x * v->y - u->y * v->x);
}

float vectorMag3D(Vector3DPtr v) {
    // computes the magnitude of a vector
    return (float)sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
}

void matPrint4x4(Matrix4x4 a) {
    // this function prints out a 4x4 matrix

    int row,    // looping variables
        column;

    for (row = 0; row < 4; row++) {
        printf("\n");

        for (column = 0; column < 4; column++) {
            printf("%f ", a[row][column]);
        }
    }

    printf("\n");
}

void matPrint1x4(Matrix1x4 a) {
    // this function prints out a 1x4 matrix

    int column; // looping variable

    printf("\n");

    for (column = 0; column < 4; column++) {
        printf("%f ", a[column]);
    }

    printf("\n");
}

void matMul4x4With4x4(
    Matrix4x4 a,
    Matrix4x4 b,
    Matrix4x4 result) {

    // this function multiplies a 4x4 by a 4x4 and stores the result in a 4x4

    // first row
    result[0][0] = a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0];
    result[0][1] = a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1];
    result[0][2] = a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2];
    result[0][3] = 0; // can probably get rid of this too, it's always 0

    // second row
    result[1][0] = a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0];
    result[1][1] = a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1];
    result[1][2] = a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2];
    result[1][3] = 0; // can probably get rid of this too, it's always 0

    // third row
    result[2][0] = a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0];
    result[2][1] = a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1];
    result[2][2] = a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2];
    result[2][3] = 0; // can probably get rid of this too, it's always 0

    // fourth row
    result[3][0] = a[3][0] * b[0][0] + a[3][1] * b[1][0] + a[3][2] * b[2][0] + b[3][0];
    result[3][1] = a[3][0] * b[0][1] + a[3][1] * b[1][1] + a[3][2] * b[2][1] + b[3][1];
    result[3][2] = a[3][0] * b[0][2] + a[3][1] * b[1][2] + a[3][2] * b[2][2] + b[3][2];
    result[3][3] = 1; // can probably get rid of this too, it's always 0
}

void matMul1x4With4x4(
    Matrix1x4 a,
    Matrix4x4 b,
    Matrix1x4 result) {

    // this function multiplies a 1x4 by a 4x4 and stores the result in a 1x4

    int indexJ, // column index
        indexK; // row index

    float sum;  // temp used to hold sum of products

    // loop thru columns of b
    for (indexJ = 0; indexJ < 4; indexJ++) {
        // multiply ith row of a by jth column of b and store the sum
        // of products in the position i,j of result
        sum = 0;

        for (indexK = 0; indexK < 4; indexK++) {
            sum += a[indexK] * b[indexK][indexJ];
        }

        // store result
        result[indexJ] = sum;
    }
}

void matIdentity4x4(Matrix4x4 a) {
    // this function creates a 4x4 identity matrix
    memset((void*)a, 0, sizeof(float) * 16);

    // set main diagonal to 1's
    a[0][0] = a[1][1] = a[2][2] = a[3][3] = 1;
}

void matZero4x4(Matrix4x4 a) {
    // this function zero's out a 4x4 matrix
    memset((void*)a, 0, sizeof(float) * 16);
}

void matCopy4x4(Matrix4x4 source, Matrix4x4 destination) {
    // this function copies one 4x4 matrix to another
    memcpy((void*)destination, (void*)source, sizeof(float) * 16);
}

void localToWorldObject(ObjectPtr object) {
    // this function converts an objects local coordinates to world coordinates
    // by translating each point in the object by the objects current position

    int index;  // looping variable

    // move object from local position to world position
    for (index = 0; index < object->numVertices; index++) {
        object->verticesWorld[index].x = object->verticesLocal[index].x +
                                         object->worldPos.x;

        object->verticesWorld[index].y = object->verticesLocal[index].y +
                                         object->worldPos.y;

        object->verticesWorld[index].z = object->verticesLocal[index].z +
                                         object->worldPos.z;
    }

    // reset visibility flags for all polys
    for (index = 0; index < object->numPolys; index++) {
        object->polys[index].visible = 1;
        object->polys[index].clipped = 0;
    }
}

void createWorldToCamera(void) {
    // this function creates the global inverse transformation matrix
    // used to transform world coordinate to camera coordinates

    Matrix4x4 translate,    // the translation matrix
              rotateX,      // the x,y and z rotation matrices
              rotateY,
              rotateZ,
              result1,
              result2;

    int activeAxes = 0;

    // create identity matrices
    matIdentity4x4(translate);

    // make a translation matrix based on the inverse of the viewpoint
    translate[3][0] = -ViewPoint.x;
    translate[3][1] = -ViewPoint.y;
    translate[3][2] = -ViewPoint.z;

    // test if there is any X rotation in view angles
    if (ViewAngle.angX) {
        matIdentity4x4(rotateX);

        // x matrix
        rotateX[1][1] =  ( CosLook[ViewAngle.angX]);
        rotateX[1][2] = -( SinLook[ViewAngle.angX]);
        rotateX[2][1] = -(-SinLook[ViewAngle.angX]);
        rotateX[2][2] =  ( CosLook[ViewAngle.angX]);

        activeAxes += 1;
    }

    if (ViewAngle.angY) {
        matIdentity4x4(rotateY);

        // y matrix
        rotateY[0][0] =  ( CosLook[ViewAngle.angY]);
        rotateY[0][2] = -(-SinLook[ViewAngle.angY]);
        rotateY[2][0] = -( SinLook[ViewAngle.angY]);
        rotateY[2][2] =  ( CosLook[ViewAngle.angY]);

        activeAxes += 2;
    }

    if (ViewAngle.angZ) {
        matIdentity4x4(rotateZ);

        // z matrix
        rotateZ[0][0] =  ( CosLook[ViewAngle.angZ]);
        rotateZ[0][1] = -( SinLook[ViewAngle.angZ]);
        rotateZ[1][0] = -(-SinLook[ViewAngle.angZ]);
        rotateZ[1][1] =  ( CosLook[ViewAngle.angZ]);

        activeAxes += 4;
    }

    // multiply all the matrices together to obtain a final world to camera
    // viewing transformation matrix i.e.
    // translation * rotateX * rotateY * rotateZ, however, only, multiply
    // matrices that can possible add to the final view
    switch (activeAxes) {
        case 0: {
            // translation only
            matCopy4x4(translate, GlobalView);
        } break;

        case 1: {
            // translation and X axis

            // since only a single axis is active manually set up the matrix

            // matMul4x4With4x4(translate, rotateX, GlobalView);

            // manually create matrix using knowledge that final product is
            // of the form

            // | 1            0            0          0|
            // | 0            c            -s         0|
            // | 0            s            c          0|
            // |(-tx)       (-ty*c-tz*s) (ty*s-tz*c)  1|

            matCopy4x4(rotateX, GlobalView);

            // now copy last row into GlobalView
            GlobalView[3][0] = (-ViewPoint.x);

            GlobalView[3][1] = (-ViewPoint.y * CosLook[ViewAngle.angY] -
                                 ViewPoint.z * SinLook[ViewAngle.angY]);

            GlobalView[3][2] = (ViewPoint.y * SinLook[ViewAngle.angY] -
                                ViewPoint.z * CosLook[ViewAngle.angY]);
        } break;

        case 2: {
            // translation and Y axis

            // matMul4x4With4x4(translate, rotateY, GlobalView);

            // manually create matrix using knowledge that final product is
            // of the form

            // | c            0            s          0|
            // | 0            1            0          0|
            // | -s           0            c          0|
            // |(tx*c+tz*s) (-ty)        (-tx*s-tz*c) 1|

            matCopy4x4(rotateY, GlobalView);

            // now copy last row into GlobalView
            GlobalView[3][0] = (-ViewPoint.x * CosLook[ViewAngle.angY] +
                                 ViewPoint.z * SinLook[ViewAngle.angY]);

            GlobalView[3][1] = (-ViewPoint.y);

            GlobalView[3][2] = (-ViewPoint.x * SinLook[ViewAngle.angY] -
                                 ViewPoint.z * CosLook[ViewAngle.angY]);
        } break;

        case 3: {
            // translation and X and Y
            matMul4x4With4x4(translate, rotateX, result1);
            matMul4x4With4x4(result1, rotateY, GlobalView);
        } break;

        case 4: {
            // translation and Z

            // matMul4x4With4x4(translate, rotateZ, GlobalView);

            // manually create matrix using knowledge that final product is
            // of the form

            // | c            -s           0          0|
            // | s            c            0          0|
            // | 0            s            c          0|
            // |(-tx*c-ty*s) (tx*s-ty*c)  (-tz)       1|

            matCopy4x4(rotateZ, GlobalView);

            // now copy last row into GlobalView
            GlobalView[3][0] = (-ViewPoint.x * CosLook[ViewAngle.angZ] -
                                 ViewPoint.y * SinLook[ViewAngle.angZ]);

            GlobalView[3][1] = (ViewPoint.x * SinLook[ViewAngle.angZ] -
                                ViewPoint.y * CosLook[ViewAngle.angZ]);

            GlobalView[3][2] = (-ViewPoint.z);
        } break;

        case 5: {
            // translation and X and Z
            matMul4x4With4x4(translate, rotateX, result1);
            matMul4x4With4x4(result1, rotateZ, GlobalView);
        } break;

        case 6: {
            // translation and Y and Z
            matMul4x4With4x4(translate, rotateY, result1);
            matMul4x4With4x4(result1, rotateZ, GlobalView);
        } break;

        case 7: {
            // translation and X and Y and Z
            matMul4x4With4x4(translate, rotateX, result1);
            matMul4x4With4x4(result1, rotateY, result2);
            matMul4x4With4x4(result2, rotateZ, GlobalView);
        } break;

        default: break;
    }
}

void worldToCameraObject(ObjectPtr object) {
    // this function converts an objects world coordinates to camera coordinates
    // by multiplying each point of the object by the inverse viewing transformation
    // matrix which is generated by concatenating the inverse of the view position
    // and the view angles the result of which is in GlobalView

    int index;  // looping variable
    int activeAxes = 0;

    if (ViewAngle.angX) {
        activeAxes += 1;
    }

    if (ViewAngle.angY) {
        activeAxes += 2;
    }

    if (ViewAngle.angZ) {
        activeAxes += 4;
    }

    // based on active angles only compute what's neccessary
    switch (activeAxes) {
        case 0: {
            // T-1
            for (index = 0; index < object->numVertices; index++) {
                // multiply the point by the viewing transformation matrix

                // x component
                object->verticesCamera[index].x =
                    object->verticesWorld[index].x +
                                                   GlobalView[3][0];

                // y component
                object->verticesCamera[index].y =
                    object->verticesWorld[index].y +
                                                   GlobalView[3][1];

                // z component
                object->verticesCamera[index].z =
                    object->verticesWorld[index].z +
                                                   GlobalView[3][2];
            }
        } break;

        case 1: {
            // T-1 * Rx-1
            for (index = 0; index < object->numVertices; index++) {
                // multiply the point by the viewing transformation matrix

                // x component
                object->verticesCamera[index].x =
                    object->verticesWorld[index].x +
                                                   GlobalView[3][0];

                // y component
                object->verticesCamera[index].y =
                    object->verticesWorld[index].y * GlobalView[1][1] +
                    object->verticesWorld[index].z * GlobalView[2][1] +
                                                     GlobalView[3][1];

                // z component
                object->verticesCamera[index].z =
                    object->verticesWorld[index].y * GlobalView[1][2] +
                    object->verticesWorld[index].z * GlobalView[2][2] +
                                                     GlobalView[3][2];
            }
        } break;

        case 2: {
            // T-1 * Ry-1, this is the standard rotation in a plane
            for (index = 0; index < object->numVertices; index++) {
                // multiply the point by the viewing transformation matrix

                // x component
                object->verticesCamera[index].x =
                    object->verticesWorld[index].x * GlobalView[0][0] +
                    object->verticesWorld[index].z * GlobalView[2][0] +
                                                     GlobalView[3][0];

                // y component
                object->verticesCamera[index].y =
                    object->verticesWorld[index].y +
                                                   GlobalView[3][1];

                // z component
                object->verticesCamera[index].z =
                    object->verticesWorld[index].x * GlobalView[0][2] +
                    object->verticesWorld[index].z * GlobalView[2][2] +
                                                     GlobalView[3][2];
            }
        } break;

        case 4: {
            // T-1 * Rz-1
            for (index = 0; index < object->numVertices; index++) {
                // multiply the point by the viewing transformation matrix

                // x component
                object->verticesCamera[index].x =
                    object->verticesWorld[index].x * GlobalView[0][0] +
                    object->verticesWorld[index].y * GlobalView[1][0] +
                                                     GlobalView[3][0];

                // y component
                object->verticesCamera[index].y =
                    object->verticesWorld[index].x * GlobalView[0][1] +
                    object->verticesWorld[index].y * GlobalView[1][1] +
                                                     GlobalView[3][1];

                // z component
                object->verticesCamera[index].z =
                    object->verticesWorld[index].z +
                                                   GlobalView[3][2];
            }
        } break;

        // these can all be optimized by pre-computing the form of the world
        // to camera matrix and using the same logic as the cases above
        case 3:  // T-1 * Rx-1 * Ry-1
        case 5:  // T-1 * Rx-1 * Rz-1
        case 6:  // T-1 * Ry-1 * Rz-1
        case 7:  // T-1 * Rx-1 * Ry-1 * Rz-1
        {
            for (index = 0; index < object->numVertices; index++) {
                // multiply the point by the viewing transformation matrix

                // x component
                object->verticesCamera[index].x =
                    object->verticesWorld[index].x * GlobalView[0][0] +
                    object->verticesWorld[index].y * GlobalView[1][0] +
                    object->verticesWorld[index].z * GlobalView[2][0] +
                                                     GlobalView[3][0];

                // y component
                object->verticesCamera[index].y =
                    object->verticesWorld[index].x * GlobalView[0][1] +
                    object->verticesWorld[index].y * GlobalView[1][1] +
                    object->verticesWorld[index].z * GlobalView[2][1] +
                                                     GlobalView[3][1];

                // z component
                object->verticesCamera[index].z =
                    object->verticesWorld[index].x * GlobalView[0][2] +
                    object->verticesWorld[index].y * GlobalView[1][2] +
                    object->verticesWorld[index].z * GlobalView[2][2] +
                                                     GlobalView[3][2];
            }
        } break;

        default: break;
    }
}

void rotateObject(ObjectPtr object, int angleX, int angleY, int angleZ) {
    // this function rotates an object relative to it's own local coordinate system
    // and allows simultaneous rotations

    int index,      // looping variable
        product = 0; // used to determine which matrices need multiplying

    Matrix4x4 rotateX, // the x,y and z rotation matrices
              rotateY,
              rotateZ,
              rotate,  // the final rotation matrix
              temp;    // temporary working matrix

    float tempX, // used to hold intermediate results during rotation
          tempY,
          tempZ;

    // test if we need to rotate at all
    if (angleX == 0 && angleY == 0 && angleZ == 0) {
        return;
    }

    // create identity matrix
    matIdentity4x4(rotate);

    // figure out which axes are active
    if (angleX) {
        product += 4;
    }

    if (angleY) {
        product += 2;
    }

    if (angleZ) {
        product += 1;
    }

    // compute final rotation matrix and perform rotation all in one!
    switch (product) {
        case 1: {
            // final matrix = z

            // set up matrix
            rotate[0][0] = ( CosLook[angleZ]);
            rotate[0][1] = ( SinLook[angleZ]);
            rotate[1][0] = (-SinLook[angleZ]);
            rotate[1][1] = ( CosLook[angleZ]);

            // matrix is of the form

            // | cos   sin  0     0 |
            // | -sin  cos  0     0 |
            // | 0     0    1     0 |
            // | 0     0    0     1 |

            // hence we can remove a number of multiplications during the
            // computations of x,y and z since many times each variable
            // isn't a function of the other two

            // perform rotation

            // now multiply each point in object by transformation matrix
            for (index = 0; index < object->numVertices; index++) {
                // x component
                tempX = object->verticesLocal[index].x * rotate[0][0] +
                        object->verticesLocal[index].y * rotate[1][0];
                       // object->verticesLocal[index].z * rotate[2][0];

                // y component
                tempY = object->verticesLocal[index].x * rotate[0][1] +
                        object->verticesLocal[index].y * rotate[1][1];
                        // object->verticesLocal[index].z * rotate[2][1];

                // z component
                tempZ = object->verticesLocal[index].z;

                // store rotated point back into local array
                object->verticesLocal[index].x = tempX;
                object->verticesLocal[index].y = tempY;
                object->verticesLocal[index].z = tempZ;
            }
        } break;

        case 2: {
            // final matrix = y
            rotate[0][0] = ( CosLook[angleY]);
            rotate[0][2] = (-SinLook[angleY]);
            rotate[2][0] = ( SinLook[angleY]);
            rotate[2][2] = ( CosLook[angleY]);

            // matrix is of the form

            // | cos   0    -sin  0 |
            // | 0     1    0     0 |
            // | sin   0    cos   0 |
            // | 0     0    0     1 |

            // hence we can remove a number of multiplications during the
            // computations of x,y and z since many times each variable
            // isn't a function of the other two

            // now multiply each point in object by transformation matrix
            for (index = 0; index < object->numVertices; index++) {
                // x component
                tempX = object->verticesLocal[index].x * rotate[0][0] +
                        // object->verticesLocal[index].y * rotate[1][0] +
                        object->verticesLocal[index].z * rotate[2][0];

                // y component
                tempY = object->verticesLocal[index].y;

                // z component
                tempZ = object->verticesLocal[index].x * rotate[0][2] +
                        // object->verticesLocal[index].y * rotate[1][2] +
                        object->verticesLocal[index].z * rotate[2][2];

                // store rotated point back into local array
                object->verticesLocal[index].x = tempX;
                object->verticesLocal[index].y = tempY;
                object->verticesLocal[index].z = tempZ;
            }
        } break;

        case 3: {
            // final matrix = y*z

            // take advantage of the fact that the product of Ry*Rz is
            //
            // | (cos angy)*(cos angz)  (cos angy)*(sin angz) -sin angy 0|
            // | -sin angz              cos angz              0         0|
            // | (sin angy)*(cos angz)  (sin angy)*(sin angz) cos angy  0|
            // | 0                      0                     0         1|

            // also notice the 0 in the 3rd column, we can use it to get
            // rid of one multiplication in the for loop below per point
            rotate[0][0] = CosLook[angleY] * CosLook[angleZ];
            rotate[0][1] = CosLook[angleY] * SinLook[angleZ];
            rotate[0][2] = -SinLook[angleY];

            rotate[1][0] = -SinLook[angleZ];
            rotate[1][1] = CosLook[angleZ];

            rotate[2][0] = SinLook[angleY] * CosLook[angleZ];
            rotate[2][1] = SinLook[angleY] * SinLook[angleZ];
            rotate[2][2] = CosLook[angleY];

            // now multiply each point in object by transformation matrix
            for (index = 0; index < object->numVertices; index++) {
                // x component
                tempX = object->verticesLocal[index].x * rotate[0][0] +
                        object->verticesLocal[index].y * rotate[1][0] +
                        object->verticesLocal[index].z * rotate[2][0];

                // y component
                tempY = object->verticesLocal[index].x * rotate[0][1] +
                        object->verticesLocal[index].y * rotate[1][1] +
                        object->verticesLocal[index].z * rotate[2][1];

                // z component
                tempZ = object->verticesLocal[index].x * rotate[0][2] +
                        // object->verticesLocal[index].y * rotate[1][2] +
                        object->verticesLocal[index].z * rotate[2][2];

                // store rotated point back into local array
                object->verticesLocal[index].x = tempX;
                object->verticesLocal[index].y = tempY;
                object->verticesLocal[index].z = tempZ;
            }
        } break;

        case 4: {
            // final matrix = x
            rotate[1][1] = ( CosLook[angleX]);
            rotate[1][2] = ( SinLook[angleX]);
            rotate[2][1] = (-SinLook[angleX]);
            rotate[2][2] = ( CosLook[angleX]);

            // matrix is of the form

            // | 1     s    0     0 |
            // | 0     cos  sin   0 |
            // | 0    -sin  cos   0 |
            // | 0     0    0     1 |

            // hence we can remove a number of multiplications during the
            // computations of x,y and z since many times each variable
            // isn't a function of the other two

            // now multiply each point in object by transformation matrix
            for (index = 0; index < object->numVertices; index++) {
                // x component
                tempX = object->verticesLocal[index].x;

                // y component
                tempY = // object->verticesLocal[index].x * rotate[0][1] +
                        object->verticesLocal[index].y * rotate[1][1] +
                        object->verticesLocal[index].z * rotate[2][1];

                // z component
                tempZ = // object->verticesLocal[index].x * rotate[0][2] +
                        object->verticesLocal[index].y * rotate[1][2] +
                        object->verticesLocal[index].z * rotate[2][2];

                // store rotated point back into local array
                object->verticesLocal[index].x = tempX;
                object->verticesLocal[index].y = tempY;
                object->verticesLocal[index].z = tempZ;
            }
        } break;

        case 5: {
            // final matrix = x*z

            // take advantage of the fact that the product of Rx*Rz is
            //
            // | cos angz                sin angz               0         0|
            // | -(cos angx)*(sin angz)  (cos angx)*(cos angz)  sin angx  0|
            // | (sin angx)*(sin angz)  -(sin angx)*(cos angz)  cos angx  0|
            // | 0                       0                      0         1|

            // also notice the 0 in the 3rd column, we can use it to get
            // rid of one multiplication in the for loop below per point
            rotate[0][0] = CosLook[angleZ];
            rotate[0][1] = SinLook[angleZ];

            rotate[1][0] = -CosLook[angleX] * SinLook[angleZ];
            rotate[1][1] = CosLook[angleX] * CosLook[angleZ];
            rotate[1][2] = SinLook[angleX];

            rotate[2][0] = SinLook[angleX] * SinLook[angleZ];
            rotate[2][1] = -SinLook[angleX] * CosLook[angleZ];
            rotate[2][2] = CosLook[angleX];

            // now multiply each point in object by transformation matrix
            for (index = 0; index < object->numVertices; index++) {
                // x component
                tempX = object->verticesLocal[index].x * rotate[0][0] +
                        object->verticesLocal[index].y * rotate[1][0] +
                        object->verticesLocal[index].z * rotate[2][0];

                // y component
                tempY = object->verticesLocal[index].x * rotate[0][1] +
                        object->verticesLocal[index].y * rotate[1][1] +
                        object->verticesLocal[index].z * rotate[2][1];

                // z component
                tempZ = // object->verticesLocal[index].x * rotate[0][2] +
                        object->verticesLocal[index].y * rotate[1][2] +
                        object->verticesLocal[index].z * rotate[2][2];

                // store rotated point back into local array
                object->verticesLocal[index].x = tempX;
                object->verticesLocal[index].y = tempY;
                object->verticesLocal[index].z = tempZ;
            }
        } break;

        case 6: {
            // final matrix = x*y

            // take advantage of the fact that the product of Rx*Ry is
            //
            // | cos angy                0          -sin angy             0|
            // | (sin angx)*(sin angy)   cos angx   (sin angx)*(cos angy) 0|
            // | (cos angx)*(sin angy)   -sin angx  (cos angx)*(cos angy) 0|
            // | 0                       0          0                     1|

            // also notice the 0 in the 2nd column, we can use it to get
            // rid of one multiplication in the for loop below per point
            rotate[0][0] = CosLook[angleY];
            rotate[0][2] = -SinLook[angleY];

            rotate[1][0] = SinLook[angleX] * SinLook[angleY];
            rotate[1][1] = CosLook[angleX];
            rotate[1][2] = SinLook[angleX] * CosLook[angleY];

            rotate[2][0] = CosLook[angleX] * SinLook[angleY];
            rotate[2][1] = -SinLook[angleX];
            rotate[2][2] = CosLook[angleX] * CosLook[angleY];

            // now multiply each point in object by transformation matrix
            for (index = 0; index < object->numVertices; index++) {
                // x component
                tempX = object->verticesLocal[index].x * rotate[0][0] +
                        object->verticesLocal[index].y * rotate[1][0] +
                        object->verticesLocal[index].z * rotate[2][0];

                // y component
                tempY = // object->verticesLocal[index].x * rotate[0][1] +
                        object->verticesLocal[index].y * rotate[1][1] +
                        object->verticesLocal[index].z * rotate[2][1];

                // z component
                tempZ = object->verticesLocal[index].x * rotate[0][2] +
                        object->verticesLocal[index].y * rotate[1][2] +
                        object->verticesLocal[index].z * rotate[2][2];

                // store rotated point back into local array
                object->verticesLocal[index].x = tempX;
                object->verticesLocal[index].y = tempY;
                object->verticesLocal[index].z = tempZ;
            }
        } break;

        case 7: {
            // final matrix = x*y*z, do it the hard way
            matIdentity4x4(rotateX);

            rotateX[1][1] = ( CosLook[angleX]);
            rotateX[1][2] = ( SinLook[angleX]);
            rotateX[2][1] = (-SinLook[angleX]);
            rotateX[2][2] = ( CosLook[angleX]);

            matIdentity4x4(rotateY);

            rotateY[0][0] = ( CosLook[angleY]);
            rotateY[0][2] = (-SinLook[angleY]);
            rotateY[2][0] = ( SinLook[angleY]);
            rotateY[2][2] = ( CosLook[angleY]);

            matIdentity4x4(rotateZ);

            rotateZ[0][0] = ( CosLook[angleZ]);
            rotateZ[0][1] = ( SinLook[angleZ]);
            rotateZ[1][0] = (-SinLook[angleZ]);
            rotateZ[1][1] = ( CosLook[angleZ]);

            matMul4x4With4x4(rotateX, rotateY, temp);
            matMul4x4With4x4(temp, rotateZ, rotate);

            // now multiply each point in object by transformation matrix
            for (index = 0; index < object->numVertices; index++) {
                // x component
                tempX = object->verticesLocal[index].x * rotate[0][0] +
                        object->verticesLocal[index].y * rotate[1][0] +
                        object->verticesLocal[index].z * rotate[2][0];

                // y component
                tempY = object->verticesLocal[index].x * rotate[0][1] +
                        object->verticesLocal[index].y * rotate[1][1] +
                        object->verticesLocal[index].z * rotate[2][1];

                // z component
                tempZ = object->verticesLocal[index].x * rotate[0][2] +
                        object->verticesLocal[index].y * rotate[1][2] +
                        object->verticesLocal[index].z * rotate[2][2];

                // store rotated point back into local array
                object->verticesLocal[index].x = tempX;
                object->verticesLocal[index].y = tempY;
                object->verticesLocal[index].z = tempZ;
            }
        } break;

        default: break;
    }
}

void positionObject(ObjectPtr object, int x, int y, int z) {
    // this function positions an object in the world
    object->worldPos.x = x;
    object->worldPos.y = y;
    object->worldPos.z = z;
}

void translateObject(ObjectPtr object, int xTrans, int yTrans, int zTrans) {
    // this function translates an object relative to it's own local
    // coordinate system
    object->worldPos.x += xTrans;
    object->worldPos.y += yTrans;
    object->worldPos.z += zTrans;
}

void scaleObject(ObjectPtr object, float scaleFactor) {
    // this function scales an object relative to it's own local coordinate system
    // equally in x,y and z

    int currPoly,   // the current polygon being processed
        currVertex; // the current vertex being processed

    float scale2;   // holds the sqaure of the scaling factor, needed to
                    // resize the surface normal for lighting calculations

    // multiply each vertex in the object definition by the scaling factor
    for (currVertex = 0; currVertex < object->numVertices; currVertex++) {
        object->verticesLocal[currVertex].x *= scaleFactor;
        object->verticesLocal[currVertex].y *= scaleFactor;
        object->verticesLocal[currVertex].z *= scaleFactor;
    }

    // compute scaling factor squared
    scale2 = scaleFactor * scaleFactor;

    // now scale all pre-computed normals
    for (currPoly = 0; currPoly < object->numPolys; currPoly++) {
        object->polys[currPoly].normalLength *= scale2;
    }

    // finally scale the radius up
    object->radius *= scaleFactor;
}

int objectsCollide(ObjectPtr object1, ObjectPtr object2) {
    // this function tests if the bounding spheres of two objects overlaps
    // if a more accurate test is needed then polygons should be tested against
    // polygons. note the function uses the fact that if x > y then x^2 > y^2
    // to avoid using square roots. Finally, the function might be altered
    // so that the bounding spheres are shrank to make sure that the collision
    // is "solid"/ finally, soft and hard collisions are both detected

    float dx, dy, dz,           // deltas in x,y and z
          radius1, radius2,     // radi of each object
          distance;             // distance between object centers

    // compute deltas
    dx = (object1->worldPos.x - object2->worldPos.x);
    dy = (object1->worldPos.y - object2->worldPos.y);
    dz = (object1->worldPos.z - object2->worldPos.z);

    // compute length
    distance = dx * dx + dy * dy + dz * dz;

    // compute radius of each object squared
    radius1 = object1->radius * object1->radius;
    radius2 = object2->radius * object2->radius;

    // test if distance is smaller than of radi
    if (distance < radius1 || distance < radius2) {
        return HARD_COLLISION;
    } else if (distance < radius1 + radius2) {
        return SOFT_COLLISION;
    } else {
        return NO_COLLISION;
    }
}

char* plgGetLine(char* string, int maxLength, FILE* fp) {
    // this function gets a line from a PLG file and strips comments
    // just pretend it's a black box!

    char buffer[80];    // temporary string storage

    int length,         // length of line read
        index = 0,      // looping variables
        index2 = 0,
        parsed = 0;     // has the current input line been parsed

    // get the next line of input, make sure there is something on the line
    while (1) {
        // get the line
        if (!fgets(buffer, maxLength, fp)) {
            return NULL;
        }

        // get length of line
        length = strlen(buffer);

        // kill the carriage return
        buffer[length - 1] = 0;

        // reset index
        index = 0;

        // eat leading white space
        while (buffer[index] == ' ') {
            index++;
        }

        // read line into buffer, if "#" arrives in data stream then disregard
        // rest of line
        parsed = 0;
        index2 = 0;

        while (!parsed) {
            if (buffer[index] != '#' && buffer[index] != ';') {
                // insert character into output string
                string[index2] = buffer[index];

                // test if this is a null terminator
                if (string[index2] == 0) {
                    parsed = 1;
                }

                // move to next character
                index++;
                index2++;
            } else {
                // insert a null termination since this is the end of the
                // string for all intense purposes
                string[index2] = 0;

                parsed = 1;
            }
        }

        // make sure we got a string and not a blank line
        if (strlen(string)) {
            return string;
        }
    }
}

int plgLoadObject(ObjectPtr object, char* filename, float scale) {
    // this function loads an object off disk and allows it to be scaled, the files
    // are in a slightly enhanced PLG format that allows polygons to be defined
    // as one or two sided, this helps the hidden surface removal system. This
    // extra functionality has been encoded in the 2nd nibble from the left of the
    // color descriptor

    FILE* fp;   // disk file

    static int idNumber = 0;    // used to set object id's

    char buffer[80],        // holds input string
         objectName[32],    // name of 3-D object
         *token;            // current parsing token

    unsigned int totalVertices,     // total vertices in object
                 totalPolys,        // total polygons per object
                 numVertices,       // number of vertices on a polygon
                 colorDes,          // the color descriptor of a polygon
                 logicalColor,      // the final color of polygon
                 shading,           // the type of shading used on polygon
                 twoSided,          // flags if poly has two sides
                 index,             // looping variables
                 index2,
                 vertexNum,         // vertex numbers
                 vertex0,
                 vertex1,
                 vertex2;

    float x, y, z;          // a single vertex

    Vector3D u, v, normal;  // working vectors

    // open the disk file
    if ((fp = fopen(filename, "r")) == NULL) {
        printf("\nCouldn't open file %s", filename);
        return 0;
    }

    // first we are looking for the header line that has the object name and
    // the number of vertices and polygons
    if (!plgGetLine(buffer, 80, fp)) {
        printf("\nError with PLG file %s", filename);
        fclose(fp);
        return 0;
    }

    // extract object name and number of vertices and polygons
    sscanf(buffer, "%s %d %d", objectName, &totalVertices, &totalPolys);

    // set proper fields in object
    object->numVertices = totalVertices;
    object->numPolys    = totalPolys;
    object->state       = 1;

    object->worldPos.x  = 0;
    object->worldPos.y  = 0;
    object->worldPos.z  = 0;

    // set id number, maybe later also add the name of object in the
    // structure???
    object->id = idNumber++;

    // based on number of vertices, read vertex list into object
    for (index = 0; index < totalVertices; index++) {
        // read in vertex
        if (!plgGetLine(buffer, 80, fp)) {
            printf("\nError with PLG file %s", filename);
            fclose(fp);
            return 0;
        }

        sscanf(buffer, "%f %f %f", &x, &y, &z);

        // insert vertex into object
        object->verticesLocal[index].x = x * scale;
        object->verticesLocal[index].y = y * scale;
        object->verticesLocal[index].z = z * scale;
    }

    // now read in polygon list
    for (index = 0; index < totalPolys; index++) {
        // read in color and number of vertices for next polygon
        if (!plgGetLine(buffer, 80, fp)) {
            printf("\nError with PLG file %s", filename);
            fclose(fp);
            return 0;
        }

        // intialize token getter and get first token which is color descriptor
        if (!(token = strtok(buffer, " "))) {
            printf("\nError with PLG file %s", filename);
            fclose(fp);
            return 0;
        }

        // test if number is hexadecimal
        if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
            sscanf(&token[2], "%x", &colorDes);
        } else {
            colorDes = atoi(token);
        }

        // extract base color and type of shading
        logicalColor = colorDes & 0x00ff;
        shading      = colorDes >> 12;
        twoSided     = ((colorDes >> 8) & 0x0f);

        // read number of vertices in polygon
        if (!(token = strtok(NULL, " "))) {
            printf("\nError with PLG file %s", filename);
            fclose(fp);
            return 0;
        }

        if ((numVertices = atoi(token)) <= 0) {
            printf("\nError with PLG file (number of vertices) %s", filename);
            fclose(fp);
            return 0;
        }

        // set fields in polygon structure
        object->polys[index].numPoints = numVertices;
        object->polys[index].color     = logicalColor;
        object->polys[index].shading   = shading;
        object->polys[index].twoSided  = twoSided;
        object->polys[index].visible   = 1;
        object->polys[index].clipped   = 0;
        object->polys[index].active    = 1;

        // now read in polygon vertice list
        for (index2 = 0; index2 < numVertices; index2++) {
            // read in next vertex number
            if (!(token = strtok(NULL, " "))) {
                printf("\nError with PLG file %s", filename);
                fclose(fp);
                return 0;
            }

            vertexNum = atoi(token);

            // insert vertex number into polygon
            object->polys[index].vertexList[index2] = vertexNum;
        }

        // compute length of the two co-planer edges of the polygon, since they
        // will be used in the computation of the dot-product later
        vertex0 = object->polys[index].vertexList[0];
        vertex1 = object->polys[index].vertexList[1];
        vertex2 = object->polys[index].vertexList[2];

        // the vector u = vo->v1
        makeVector3D((Point3DPtr)&object->verticesLocal[vertex0],
                     (Point3DPtr)&object->verticesLocal[vertex1],
                     (Vector3DPtr)&u);

        // the vector v = vo->v2
        makeVector3D((Point3DPtr)&object->verticesLocal[vertex0],
                     (Point3DPtr)&object->verticesLocal[vertex2],
                     (Vector3DPtr)&v);

        crossProduct3D((Vector3DPtr)&v,
                       (Vector3DPtr)&u,
                       (Vector3DPtr)&normal);

        // compute magnitude of normal, take its inverse and multiply it by
        // 15, this will change the shading calculation of 15*dp/normal into
        // dp*normal_length, removing one division
        object->polys[index].normalLength = 15.0 / vectorMag3D((Vector3DPtr)&normal);
    }

    // close the file
    fclose(fp);

    // compute object radius
    computeObjectRadius(object);

    // return success
    return 1;
}

float computeObjectRadius(ObjectPtr object) {
    // this function computes maximum radius of object, maybe a better method would
    // use average radius? Note that this functiopn shouldn't be used during
    // runtime but when an object is created

    float newRadius,    // used in average radius calculation of object
          x, y, z;      // a single vertex

    int index;          // looping variable

    // reset object radius
    object->radius = 0;

    for (index = 0; index < object->numVertices; index++) {
        x = object->verticesLocal[index].x;
        y = object->verticesLocal[index].y;
        z = object->verticesLocal[index].z;

        // compute distance to point
        newRadius = (float)sqrt(x * x + y * y + z * z);

        // is this radius bigger than last?
        if (newRadius > object->radius) {
            object->radius = newRadius;
        }
    }

    // return radius just in case
    return object->radius;
}

void clipObject3D(ObjectPtr object, int mode) {
    // this function clips an object in camera coordinates against the 3D viewing
    // volume. the function has two modes of operation. In CLIP_Z_MODE the
    // function performs only a simple z extend clip with the near and far clipping
    // planes. In CLIP_XYZ_MODE mode the function performs a full 3-D clip

    int currPoly;   // the current polygon being processed

    float x1, y1, z1,
          x2, y2, z2,
          x3, y3, z3,
          x4, y4, z4, // working variables used to hold vertices
          x1Compare,  // used to hold clipping points on x and y
          y1Compare,
          x2Compare,
          y2Compare,
          x3Compare,
          y3Compare,
          x4Compare,
          y4Compare,
          fovWidth,   // width and height of projected viewing plane
          fovHeight;  // used to speed up computations, since it's constant
                      // for any frame

    // test if trivial z clipping is being requested
    if (mode == CLIP_Z_MODE) {
        // attempt to clip each polygon against viewing volume
        for (currPoly = 0; currPoly < object->numPolys; currPoly++) {
            // extract z components
            z1 = object->verticesCamera[object->polys[currPoly].vertexList[0]].z;
            z2 = object->verticesCamera[object->polys[currPoly].vertexList[1]].z;
            z3 = object->verticesCamera[object->polys[currPoly].vertexList[2]].z;

            // test if this is a quad
            if (object->polys[currPoly].numPoints == 4) {
                // extract 4th z component
                z4 = object->verticesCamera[object->polys[currPoly].vertexList[3]].z;
            } else {
                z4 = z3;
            }

            // perform near and far z clipping test
            if ((z1 < ClipNearZ && z2 < ClipNearZ && z3 < ClipNearZ && z4 < ClipNearZ) ||
                (z1 > ClipFarZ && z2 > ClipFarZ && z3 > ClipFarZ && z4 > ClipFarZ)) {
                // set clipped flag
                object->polys[currPoly].clipped = 1;
            }
        }
    } else {
        // CLIP_XYZ_MODE, perform full 3D viewing volume clip

        // compute dimensions of clipping extents at current viewing distance
        fovWidth  = ((float)HALF_SCREEN_WIDTH / ViewingDistance);
        fovHeight = ((float)HALF_SCREEN_HEIGHT / ViewingDistance);

        // process each polygon
        for (currPoly = 0; currPoly < object->numPolys; currPoly++) {
            // extract x,y and z components
            x1 = object->verticesCamera[object->polys[currPoly].vertexList[0]].x;
            y1 = object->verticesCamera[object->polys[currPoly].vertexList[0]].y;
            z1 = object->verticesCamera[object->polys[currPoly].vertexList[0]].z;

            x2 = object->verticesCamera[object->polys[currPoly].vertexList[1]].x;
            y2 = object->verticesCamera[object->polys[currPoly].vertexList[1]].y;
            z2 = object->verticesCamera[object->polys[currPoly].vertexList[1]].z;

            x3 = object->verticesCamera[object->polys[currPoly].vertexList[2]].x;
            y3 = object->verticesCamera[object->polys[currPoly].vertexList[2]].y;
            z3 = object->verticesCamera[object->polys[currPoly].vertexList[2]].z;

            // test if this is a quad
            if (object->polys[currPoly].numPoints == 4) {
                // extract 4th vertex
                x4 = object->verticesCamera[object->polys[currPoly].vertexList[3]].x;
                y4 = object->verticesCamera[object->polys[currPoly].vertexList[3]].y;
                z4 = object->verticesCamera[object->polys[currPoly].vertexList[3]].z;

                // do clipping tests

                // perform near and far z clipping test first
                if (!((z1 > ClipNearZ || z2 > ClipNearZ || z3 > ClipNearZ || z4 > ClipNearZ) &&
                      (z1 < ClipFarZ || z2 < ClipFarZ || z3 < ClipFarZ || z4 < ClipFarZ))) {
                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }

                // pre-compute x comparision ranges
                x1Compare = fovWidth * z1;
                x2Compare = fovWidth * z2;
                x3Compare = fovWidth * z3;
                x4Compare = fovWidth * z4;

                // perform x test
                if (!((x1 > -x1Compare || x2 > -x1Compare || x3 > -x3Compare || x4 > -x4Compare) &&
                      (x1 < x1Compare || x2 < x2Compare || x3 < x3Compare || x4 < x4Compare))) {
                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }

                // pre-compute x comparision ranges
                y1Compare = fovHeight * z1;
                y2Compare = fovHeight * z2;
                y3Compare = fovHeight * z3;
                y4Compare = fovHeight * z4;

                // perform x test
                if (!((y1 > -y1Compare || y2 > -y1Compare || y3 > -y3Compare || y4 > -y4Compare) &&
                      (y1 < y1Compare || y2 < y2Compare || y3 < y3Compare || y4 < y4Compare))) {
                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }
            } else {
                // must be triangle, perform clipping tests on only 3 vertices

                // do clipping tests

                // perform near and far z clipping test first
                if (!((z1 > ClipNearZ || z2 > ClipNearZ || z3 > ClipNearZ) &&
                      (z1 < ClipFarZ || z2 < ClipFarZ || z3 < ClipFarZ))) {
                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }

                // pre-compute x comparision ranges
                x1Compare = fovWidth * z1;
                x2Compare = fovWidth * z2;
                x3Compare = fovWidth * z3;

                // perform x test
                if (!((x1 > -x1Compare || x2 > -x1Compare || x3 > -x3Compare) &&
                      (x1 < x1Compare || x2 < x2Compare || x3 < x3Compare))) {
                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }

                // pre-compute x comparision ranges
                y1Compare = fovHeight * z1;
                y2Compare = fovHeight * z2;
                y3Compare = fovHeight * z3;

                // perform x test
                if (!((y1 > -y1Compare || y2 > -y1Compare || y3 > -y3Compare) &&
                      (y1 < y1Compare || y2 < y2Compare || y3 < y3Compare))) {
                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }
            }
        }
    }
}

void removeBackfacesAndShade(ObjectPtr object) {
    // this function removes all the backfaces of an object by setting the visibility
    // flag. This function assumes that the object has been transformed into
    // camera coordinates. Also, the function takes into consideration is the
    // polygons are one or two sided and executed the minimum amount of code
    // in addition to perform the shading calculations

    int vertex0,        // vertex indices
        vertex1,
        vertex2,
        currPoly;       // current polygon

    float dp,           // the result of the dot product
          intensity;    // the final intensity of the surface

    Vector3D u, v,      // general working vectors
             normal,    // the normal to the surface begin processed
             sight;     // line of sight vector

    // for each polygon in the object determine if it is pointing away from the
    // viewpoint and direction
    for (currPoly = 0; currPoly < object->numPolys; currPoly++) {
        // is this polygon two sised or one sided
        if (object->polys[currPoly].twoSided == ONE_SIDED) {
            // compute two vectors on polygon that have the same intial points
            vertex0 = object->polys[currPoly].vertexList[0];
            vertex1 = object->polys[currPoly].vertexList[1];
            vertex2 = object->polys[currPoly].vertexList[2];

            // the vector u = vo->v1
            makeVector3D((Point3DPtr)&object->verticesWorld[vertex0],
                         (Point3DPtr)&object->verticesWorld[vertex1],
                         (Vector3DPtr)&u);

            // the vector v = vo-v2
            makeVector3D((Point3DPtr)&object->verticesWorld[vertex0],
                         (Point3DPtr)&object->verticesWorld[vertex2],
                         (Vector3DPtr)&v);

            // compute the normal to polygon v x u
            crossProduct3D((Vector3DPtr)&v,
                           (Vector3DPtr)&u,
                           (Vector3DPtr)&normal);

            // compute the line of sight vector, since all coordinates are world all
            // object vertices are already relative to (0,0,0), thus
            sight.x = ViewPoint.x - object->verticesWorld[vertex0].x;
            sight.y = ViewPoint.y - object->verticesWorld[vertex0].y;
            sight.z = ViewPoint.z - object->verticesWorld[vertex0].z;

            // compute the dot product between line of sight vector and normal to surface
            dp = dotProduct3D((Vector3DPtr)&normal, (Vector3DPtr)&sight);

            // is surface visible
            if (dp > 0) {
                // set visible flag
                object->polys[currPoly].visible = 1;

                // compute light intensity if needed
                if (object->polys[currPoly].shading == FLAT_SHADING) {
                    // compute the dot product between the light source vector
                    // and normal vector to surface
                    dp = dotProduct3D((Vector3DPtr)&normal,
                                      (Vector3DPtr)&LightSource);

                    // test if light ray is reflecting off surface
                    if (dp > 0) {
                        // now cos 0 = (u.v)/|u||v| or
                        intensity = AmbientLight + (dp * (object->polys[currPoly].normalLength));

                        // test if intensity has overflowed
                        if (intensity > 15) {
                            intensity = 15;
                        }

                        // intensity now varies from 0-1, 0 being black or grazing and 1 being
                        // totally illuminated. use the value to index into color table
                        object->polys[currPoly].shade =
                            object->polys[currPoly].color - (int)intensity;
                    } else {
                        object->polys[currPoly].shade =
                            object->polys[currPoly].color - (int)AmbientLight;
                    }
                } else {
                    // assume constant shading and simply assign color to shade
                    object->polys[currPoly].shade = object->polys[currPoly].color;
                }
            } else {
                object->polys[currPoly].visible = 0;
            }
        } else {
            // else polygon is always visible i.e. two sided, set visibility flag
            // so engine renders it

            // set visibility
            object->polys[currPoly].visible = 1;

            // perform shading calculation
            if (object->polys[currPoly].shading == FLAT_SHADING) {
                // compute normal

                // compute two vectors on polygon that have the same intial points
                vertex0 = object->polys[currPoly].vertexList[0];
                vertex1 = object->polys[currPoly].vertexList[1];
                vertex2 = object->polys[currPoly].vertexList[2];

                // the vector u = vo->v1
                makeVector3D((Point3DPtr)&object->verticesWorld[vertex0],
                             (Point3DPtr)&object->verticesWorld[vertex1],
                             (Vector3DPtr)&u);

                // the vector v = vo-v2
                makeVector3D((Point3DPtr)&object->verticesWorld[vertex0],
                             (Point3DPtr)&object->verticesWorld[vertex2],
                             (Vector3DPtr)&v);

                // compute the normal to polygon v x u
                crossProduct3D((Vector3DPtr)&v,
                               (Vector3DPtr)&u,
                               (Vector3DPtr)&normal);

                // compute the dot product between the light source vector
                // and normal vector to surface
                dp = dotProduct3D((Vector3DPtr)&normal,
                                  (Vector3DPtr)&LightSource);

                // test if light ray is reflecting off surface
                if (dp > 0) {
                    // now cos 0 = (u.v)/|u||v| or
                    intensity = AmbientLight + (dp * (object->polys[currPoly].normalLength));

                    // test if intensity has overflowed
                    if (intensity > 15) {
                        intensity = 15;
                    }

                    // intensity now varies from 0-1, 0 being black or grazing and 1 being
                    // totally illuminated. use the value to index into color table
                    object->polys[currPoly].shade =
                        object->polys[currPoly].color - (int)intensity;
                } else {
                    object->polys[currPoly].shade =
                        object->polys[currPoly].color - (int)AmbientLight;
                }
            } else {
                // assume constant shading and simply assign color to shade
                object->polys[currPoly].shade = object->polys[currPoly].color;
            }
        }
    }
}

int removeObject(ObjectPtr object, int mode) {
    // this function determines if an entire object is within the viewing volume
    // or not by testing if the bounding sphere of the object in question
    // is within the viewing volume.In essence, this function "culls" entire objects

    float xBsphere,     // the x,y and z components of the projected center of object
          yBsphere,
          zBsphere,
          radius,       // the radius of object
          xCompare,     // the extents of the clipping volume in x and y at the
          yCompare;     // bounding spheres current z

    // first transform world position of object into camera coordinates

    // compute x component
    xBsphere = object->worldPos.x * GlobalView[0][0] +
               object->worldPos.y * GlobalView[1][0] +
               object->worldPos.z * GlobalView[2][0] +
                                    GlobalView[3][0];

    // compute y component
    yBsphere = object->worldPos.x * GlobalView[0][1] +
               object->worldPos.y * GlobalView[1][1] +
               object->worldPos.z * GlobalView[2][1] +
                                    GlobalView[3][1];

    // compute z component
    zBsphere = object->worldPos.x * GlobalView[0][2] +
               object->worldPos.y * GlobalView[1][2] +
               object->worldPos.z * GlobalView[2][2] +
                                    GlobalView[3][2];

    // extract radius of object
    radius = object->radius;

    if (mode == OBJECT_CULL_Z_MODE) {
        // first test against near and far z planes
        if (((zBsphere - radius) > ClipFarZ) ||
            ((zBsphere + radius) < ClipNearZ)) {
            return 1;
        } else {
            return 0;
        }
    } else {
        // perform full x,y,z test
        if (((zBsphere - radius) > ClipFarZ) ||
            ((zBsphere + radius) < ClipNearZ)) {
            return 1;
        }

        // test against x right and left planes, first compute viewing volume
        // extents at position z position of bounding sphere
        xCompare = (HALF_SCREEN_WIDTH * zBsphere) / ViewingDistance;

        if (((xBsphere - radius) > xCompare) ||
            ((xBsphere + radius) < -xCompare)) {
            return 1;
        }

        // finally test against y top and bottom planes
        yCompare = (INVERSE_ASPECT_RATIO * HALF_SCREEN_HEIGHT * zBsphere) / ViewingDistance;

        if (((yBsphere - radius) > yCompare) ||
            ((yBsphere + radius) < -yCompare)) {
            return 1;
        }

        // else it just can't be removed!!!
        return 0;
    }
}

void generatePolyList(ObjectPtr object, int mode) {
    // this function is used to generate the final polygon list that will be
    // rendered. Object by object the list is built up

    int vertex,
        currVertex,
        currPoly;

    // test if this is the first object to be inserted
    if (mode == RESET_POLY_LIST) {
        // reset number of polys to zero
        NumPolysFrame = 0;
        return;
    }

    // insert all visible polygons into polygon list
    for (currPoly = 0; currPoly < object->numPolys; currPoly++) {
        // test if this poly is visible, if so add it to poly list
        if (object->polys[currPoly].visible &&
            !object->polys[currPoly].clipped) {
            // add this poly to poly list

            // first copy data and vertices into an open slot in storage area
            WorldPolyStorage[NumPolysFrame].numPoints = object->polys[currPoly].numPoints;
            WorldPolyStorage[NumPolysFrame].color     = object->polys[currPoly].color;
            WorldPolyStorage[NumPolysFrame].shade     = object->polys[currPoly].shade;
            WorldPolyStorage[NumPolysFrame].shading   = object->polys[currPoly].shading;
            WorldPolyStorage[NumPolysFrame].twoSided  = object->polys[currPoly].twoSided;
            WorldPolyStorage[NumPolysFrame].visible   = object->polys[currPoly].visible;
            WorldPolyStorage[NumPolysFrame].clipped   = object->polys[currPoly].clipped;
            WorldPolyStorage[NumPolysFrame].active    = object->polys[currPoly].active;

            // now copy vertices
            for (currVertex = 0; currVertex < object->polys[currPoly].numPoints; currVertex++) {
                // extract vertex number
                vertex = object->polys[currPoly].vertexList[currVertex];

                // extract x,y and z
                WorldPolyStorage[NumPolysFrame].vertexList[currVertex].x
                    = object->verticesCamera[vertex].x;

                WorldPolyStorage[NumPolysFrame].vertexList[currVertex].y
                    = object->verticesCamera[vertex].y;

                WorldPolyStorage[NumPolysFrame].vertexList[currVertex].z
                    = object->verticesCamera[vertex].z;
            }

            // assign pointer to it
            WorldPolys[NumPolysFrame] = &WorldPolyStorage[NumPolysFrame];

            // increment number of polys
            NumPolysFrame++;
        }
    }
}

void drawPolyList(void) {
    // this function draws the global polygon list generated by calls to
    // generatePolyList

    int currPoly;       // the current polygon

    float x1, y1, z1,   // working variables
          x2, y2, z2,
          x3, y3, z3,
          x4, y4, z4,
          viewingDistanceAspect;    // the y axis corrected viewing distance

    viewingDistanceAspect = ViewingDistance * ASPECT_RATIO;

    // draw each polygon in list
    for (currPoly = 0; currPoly < NumPolysFrame; currPoly++) {
        // get z's for perspective
        z1 = WorldPolys[currPoly]->vertexList[0].z;
        z2 = WorldPolys[currPoly]->vertexList[1].z;
        z3 = WorldPolys[currPoly]->vertexList[2].z;

        // test if this is a quad
        if (WorldPolys[currPoly]->numPoints == 4) {
            // extract vertex number and z component for clipping and projection
            z4 = WorldPolys[currPoly]->vertexList[3].z;
        } else {
            z4 = z3;
        }

#if 0
        // perform z clipping test
        if ((z1 < ClipNearZ && z2 < ClipNearZ && z3 < ClipNearZ && z4 < ClipNearZ) ||
            (z1 > ClipFarZ && z2 > ClipFarZ && z3 > ClipFarZ && z4 > ClipFarZ)) {
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
        x1 = (HALF_SCREEN_WIDTH  + x1 * ViewingDistance / z1);
        y1 = (HALF_SCREEN_HEIGHT - y1 * viewingDistanceAspect / z1);

        x2 = (HALF_SCREEN_WIDTH  + x2 * ViewingDistance / z2);
        y2 = (HALF_SCREEN_HEIGHT - y2 * viewingDistanceAspect / z2);

        x3 = (HALF_SCREEN_WIDTH  + x3 * ViewingDistance / z3);
        y3 = (HALF_SCREEN_HEIGHT - y3 * viewingDistanceAspect / z3);

        // draw triangle
        drawTriangle2D((int)x1, (int)y1, (int)x2, (int)y2, (int)x3, (int)y3,
                       WorldPolys[currPoly]->shade);

        // draw second poly if this is a quad
        if (WorldPolys[currPoly]->numPoints == 4) {
            // extract the point
            x4 = WorldPolys[currPoly]->vertexList[3].x;
            y4 = WorldPolys[currPoly]->vertexList[3].y;

            // poject to screen
            x4 = (HALF_SCREEN_WIDTH  + x4 * ViewingDistance / z4);
            y4 = (HALF_SCREEN_HEIGHT - y4 * viewingDistanceAspect / z4);

            // draw triangle
            drawTriangle2D((int)x1, (int)y1, (int)x3, (int)y3, (int)x4, (int)y4,
                           WorldPolys[currPoly]->shade);
        }
    }
}

void computeAverageZ(void) {
    // this function pre-computes the average z of each polygon, so that the
    // polygon sorter doesn't compute each z more than once

    int index;

    for (index = 0; index < NumPolysFrame; index++) {
        if (WorldPolyStorage[index].numPoints == 3) {
            WorldPolyStorage[index].averageZ =
                (int)(0.3333 * (WorldPolyStorage[index].vertexList[0].z +
                                WorldPolyStorage[index].vertexList[1].z +
                                WorldPolyStorage[index].vertexList[2].z));
        } else {
            WorldPolyStorage[index].averageZ =
                (int)(0.25 * (WorldPolyStorage[index].vertexList[0].z +
                              WorldPolyStorage[index].vertexList[1].z +
                              WorldPolyStorage[index].vertexList[2].z +
                              WorldPolyStorage[index].vertexList[3].z));
        }
    }
}

int polyCompare(FacetPtr* arg1, FacetPtr* arg2) {
    // this function comapares the average z's of two polygons and is used by the
    // depth sort surface ordering algorithm

    FacetPtr poly1, poly2;

    // dereference the poly pointers
    poly1 = (FacetPtr)*arg1;
    poly2 = (FacetPtr)*arg2;

    if (poly1->averageZ > poly2->averageZ) {
        return -1;
    } else if (poly1->averageZ < poly2->averageZ) {
        return 1;
    } else {
        return 0;
    }
}

void sortPolyList(void) {
    // this function does a simple z sort on the poly list to order surfaces
    // the list is sorted in descending order, i.e. farther polygons first
    computeAverageZ();

    qsort(
        (void*)WorldPolys,
        NumPolysFrame,
        sizeof(FacetPtr),
        (int (__watcall*)(void const*, void const*))polyCompare);
}

int loadPaletteDisk(char* filename, RgbPalettePtr palette) {
    // this function loads a color palette from disk

    int index;  // used for looping

    RgbColor color;

    FILE* fp;

    // open the disk file
    if (!(fp = fopen(filename, "r"))) {
        return 0;
    }

    // load in all the colors
    for (index = 0; index <= 255; index++) {
        // get the next color
        fscanf(fp, "%hhu %hhu %hhu", &color.red, &color.green, &color.blue);

        // store the color in next element of palette
        palette->colors[index].red   = color.red;
        palette->colors[index].green = color.green;
        palette->colors[index].blue  = color.blue;
    }

    // set palette size to a full palette
    palette->startReg = 0;
    palette->endReg   = 255;

    // close the file and return success
    fclose(fp);

    return 1;
}

int savePaletteDisk(char* filename, RgbPalettePtr palette) {
    // this function saves a palette to disk

    int index;  // used for looping

    RgbColor color;

    FILE* fp;

    // open the disk file
    if (!(fp = fopen(filename, "w"))) {
        return 0;
    }

    // write 255 lines of r g b
    for (index = 0; index <= 255; index++) {
        // get the next color

        // store the color in next element of palette
        color.red   = palette->colors[index].red;
        color.green = palette->colors[index].green;
        color.blue  = palette->colors[index].blue;

        // write the color to disk file
        fprintf(fp, "\n%d %d %d", color.red, color.green, color.blue);
    }

    // close the file and return success
    fclose(fp);

    return 1;
}

void fillDoubleBuffer32(int icolor) {
    // this function fills in the double buffer with the sent color a QUAD at
    // a time

    long color; // used to create a 4 byte color descriptor

    color = icolor | (icolor << 8);

    // replicate color into all four bytes of quad
    color = color | (color << 16);

    fquadset((void FAR*)DoubleBuffer, color, (DoubleBufferSize >> 1));
}

void displayDoubleBuffer32(unsigned char FAR* buffer, int y) {
    // this functions copies the double buffer into the video buffer at the
    // starting y location using quad byte transfers
    fquadcpy((void FAR*)(VideoBuffer + y * 320),
             (void FAR*)DoubleBuffer,
             (DoubleBufferSize >> 1));
}
