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

#include <black3.h>
#include <black4.h>
#include <black5.h>
#include <black6.h>
#include <black8.h>
#include <black9.h>
#include <black11.h>

float ClipNearZ = 100,                                      // the near or hither clipping plane
      ClipFarZ = 3000,                                      // the far or yon clipping plane
      ScreenWidth = 320,                                    // dimensions of the screen
      ScreenHeight = 200;
int ViewingDistance = 200;                                  // distance of projection plane from camera
Point3D ViewPoint = { 0, 0, 0, 1 };                         // position of camera
Vector3D LightSource = { -0.913913, 0.389759, -0.113369 };  // position of point light source
float AmbientLight = 6;                                     // ambient light level
Dir3D ViewAngle = { 0, 0, 0 };                              // angle of camera
Matrix4x4 GlobalView;                                       // the global inverse world to camera
RgbPalette ColorPalette3D;                                  // the color palette used for the 3D system
int NumObjects;                                             // number of objects in the world
ObjectPtr WorldObjectList[MAX_OBJECTS];                     // the objects in the world
int NumPolysFrame;                                          // the number of polys in this frame
FacetPtr WorldPolys[MAX_POLYS_PER_FRAME];                   // the visible polygons for this frame
Facet WorldPolyStorage[MAX_POLYS_PER_FRAME];                // the storage for the visible
                                                            // polygons is pre-allocated
                                                            // so it doesn't need to be
                                                            // allocated frame by frame

// Look up tables
float SinLook[360 + 1],  // SIN from 0 to 360
      CosLook[360 + 1];  // COSINE from 0 to 360

// the clipping region, set it to default on startup
int PolyClipMinX = POLY_CLIP_MIN_X,
    PolyClipMinY = POLY_CLIP_MIN_Y,
    PolyClipMaxX = POLY_CLIP_MAX_X,
    PolyClipMaxY = POLY_CLIP_MAX_Y;

Sprite Textures; // this holds the textures

int loadPaletteDisk(char* filename, RgbPalettePtr palette) {
    // this function loads a color palette from disk
    int index;
    RgbColor color;
    FILE* fp;

    // open the disk file
    if (!(fp = fopen(filename, "r"))) {
        return 0;
    }

    // load in all the colors
    for (index = 0; index <= 255; index++) {
        // get the next color
        fscanf(fp, "%d %d %d", &color.red, &color.green, &color.blue);

        // store the color in next element of palette
        palette->colors[index].red = color.red;
        palette->colors[index].green = color.green;
        palette->colors[index].blue = color.blue;
    }

    // set palette size to a full palette
    palette->startReg = 0;
    palette->endReg = 255;

    // close the file and return success
    fclose(fp);

    return 1;
}

int savePaletteDisk(char* filename, RgbPalettePtr palette) {
    // this function saves a palette to disk
    int index;
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
        color.red = palette->colors[index].red;
        color.green = palette->colors[index].green;
        color.blue = palette->colors[index].blue;

        // write the color to disk file
        fprintf(fp, "\n%d %d %d", color.red, color.green, color.blue);
    }

    // close the file and return success
    fclose(fp);

    return 1;
}

float computeObjectRadius(ObjectPtr object) {
    // this function computes maximum radius of object, maybe a better method would
    // use average radius? Note that this function shouldn't be used during
    // runtime but when an object is created
    float newRadius,    // used in average radius calculation of object
          x, y, z;      // a single vertex
    int index;

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

int clipLine(
    int* x1, int* y1,
    int* x2, int* y2) {

    // this function clips the sent line using the globally defined clipping region
    int point1 = 0, point2 = 0; // tracks if each point is visible or invisible
    int clipAlways = 0;         // used for clipping override
    int xi, yi;                 // point of intersection
    int rightEdge = 0,          // which edges are the endpoints beyond
        leftEdge = 0,
        topEdge = 0,
        bottomEdge = 0;
    int success = 0;            // was there a successful clipping
    float dx, dy;               // used to hold slope deltas

    // test if line is completely visible
    if (*x1 >= PolyClipMinX && *x1 <= PolyClipMaxX &&
        *y1 >= PolyClipMinY && *y1 <= PolyClipMaxY) {

        point1 = 1;
    }

    if (*x2 >= PolyClipMinX && *x2 <= PolyClipMaxX &&
        *y2 >= PolyClipMinY && *y2 <= PolyClipMaxY) {

        point2 = 1;
    }

    // test endpoints
    if (point1 == 1 && point2 == 1) {
        return 1;
    }

    // test if line is completely invisible
    if (point1 == 0 && point2 == 0) {
        // must test to see if each endpoint is on the same side of one of
        // the bounding planes created by each clipping region boundary
        if (*x1 < PolyClipMinX && *x2 < PolyClipMinX ||
            *x1 > PolyClipMaxX && *x2 > PolyClipMaxX ||
            *y1 < PolyClipMinY && *y2 < PolyClipMinY ||
            *y1 > PolyClipMaxY && *y2 > PolyClipMaxY) {

            // no need to draw line
            return 0;
        }

        // if we got here we have the special case where the line cuts into and
        // out of the clipping region
        clipAlways = 1;
    }

    // take care of case where either endpoint is in clipping region
    if (point1 == 1 || point1 == 0 && point2 == 0) {
        // compute deltas
        dx = *x2 - *x1;
        dy = *y2 - *y1;

        // compute what boundary line need to be clipped against
        if (*x2 > PolyClipMaxX) {
            // flag right edge
            rightEdge = 1;

            // compute intersection with right edge
            if (dx != 0) {
                yi = (int)(0.5f + dy / dx * (PolyClipMaxX - *x1) + *y1);
            } else {
                yi = -1;    // invalidate intersection
            }
        } else if (*x2 < PolyClipMinX) {
            // flag left edge
            leftEdge = 1;

            // compute intersection with left edge
            if (dx != 0) {
                yi = (int)(0.5f + dy / dx * (PolyClipMinX - *x1) + *y1);
            } else {
                yi = -1;    // invalidate intersection
            }
        }

        // horizontal intersections
        if (*y2 > PolyClipMaxY) {
            // flag bottom edge
            bottomEdge = 1;

            // compute intersection with right edge
            if (dy != 0) {
                xi = (int)(0.5f + dx / dy * (PolyClipMaxY - *y1) + *x1);
            } else {
                xi = -1;    // invalidate intersection
            }
        } else if (*y2 < PolyClipMinY) {
            // flag top edge
            topEdge = 1;

            // compute intersection with top edge
            if (dy != 0) {
                xi = (int)(0.5f + dx / dy * (PolyClipMinY - *y1) + *x1);
            } else {
                xi = -1;    // invalidate intersection
            }
        }

        // now we know where the line passed thru
        // compute which edge is the proper intersection
        if (rightEdge == 1 && yi >= PolyClipMinY && yi <= PolyClipMaxY) {
            *x2 = PolyClipMaxX;
            *y2 = yi;
            success = 1;
        } else if (leftEdge == 1 && yi >= PolyClipMinY && yi <= PolyClipMaxY) {
            *x2 = PolyClipMinX;
            *y2 = yi;
            success = 1;
        }

        if (bottomEdge == 1 && xi >= PolyClipMinX && xi <= PolyClipMaxX) {
            *x2 = xi;
            *y2 = PolyClipMaxY;
            success = 1;
        } else if (topEdge == 1 && xi >= PolyClipMinX && xi <= PolyClipMaxX) {
            *x2 = xi;
            *y2 = PolyClipMinY;
            success = 1;
        }
    }

    // reset edge flags
    rightEdge = leftEdge = topEdge = bottomEdge = 0;

    // test second endpoint
    if (point2 == 1 || point1 == 0 && point2 == 0) {
        // compute deltas
        dx = *x1 - *x2;
        dy = *y1 - *y2;

        // compute what boundary line need to be clipped against
        if (*x1 > PolyClipMaxX) {
            // flag right edge
            rightEdge = 1;

            // compute intersection with right edge
            if (dx != 0) {
                yi = (int)(0.5f + dy / dx * (PolyClipMaxX - *x2) + *y2);
            } else {
                yi = -1;    // invalidate intersection
            }
        } else if (*x1 < PolyClipMinX) {
            // flag left edge
            leftEdge = 1;

            // compute intersection with left edge
            if (dx != 0) {
                yi = (int)(0.5f + dy / dx * (PolyClipMinX - *x2) + *y2);
            } else {
                yi = -1;    // invalidate intersection
            }
        }

        // horizontal intersections
        if (*y1 > PolyClipMaxY) {
            // flag bottom edge
            bottomEdge = 1;

            // compute intersection with right edge
            if (dy != 0) {
                xi = (int)(0.5f + dx / dy * (PolyClipMaxY - *y2) + *x2);
            } else {
                xi = -1;    // invalidate intersection
            }
        } else if (*y1 < PolyClipMinY) {
            // flag top edge
            topEdge = 1;

            // compute intersection with top edge
            if (dy != 0) {
                xi = (int)(0.5f + dx / dy * (PolyClipMinY - *y2) + *x2);
            } else {
                xi = -1;    // invalidate intersection
            }
        }

        // now we know where the line passed thru
        // compute which edge is the proper intersection
        if (rightEdge == 1 && yi >= PolyClipMinY && yi <= PolyClipMaxY) {
            *x1 = PolyClipMaxX;
            *y1 = yi;
            success = 1;
        } else if (leftEdge == 1 && yi >= PolyClipMinY && yi <= PolyClipMaxY) {
            *x1 = PolyClipMinX;
            *y1 = yi;
            success = 1;
        }

        if (bottomEdge == 1 && xi >= PolyClipMinX && xi <= PolyClipMaxX) {
            *x1 = xi;
            *y1 = PolyClipMaxY;
            success = 1;
        } else if (topEdge == 1 && xi >= PolyClipMinX && xi <= PolyClipMaxX) {
            *x1 = xi;
            *y1 = PolyClipMinY;
            success = 1;
        }
    }

    return success;
}

void buildLookUpTables(void) {
    // this function builds all the look up tables for the engine
    int angle;
    float rad;

    // generate sin/cos look up tables
    for (angle = 0; angle <= 360; angle++) {
        rad = 3.14159f * angle / 180.0f;
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
    normal->x =  ((u->y * v->z) - (u->z * v->y));
    normal->y = -((u->x * v->z) - (u->z * v->x));
    normal->z =  ((u->x * v->y) - (u->y * v->x));
}

float vectorMag3D(Vector3DPtr v) {
    // computes the magnitude of a vector
    return (float)sqrt((v->x * v->x) + (v->y * v->y) + (v->z * v->z));
}

void matPrint4x4(Matrix4x4 a) {
    // this function prints out a 4x4 matrix
    int row,
        column;

    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            printf("%f ", a[row][column]);
        }
    }
}

void matPrint1x4(Matrix1x4 a) {
    // this function prints ouf a 1x4 matrix
    int column;

    for (column = 0; column < 4; column++) {
        printf("%f ", a[column]);
    }
}

void matMul4x4With4x4(
    Matrix4x4 a,
    Matrix4x4 b,
    Matrix4x4 result) {

    // this function multiplies a 4x4 by a 4x4 and stores the result in a 4x4
    int i, j, k;
    float sum;

    for (i = 0; i < 4; i++) {
        // loop thru columns of b
        for (j = 0; j < 4; j++) {
            // multiply ith row of a by jth column of b and store the sum
            // of products in the position i,j of result
            sum = 0;

            for (k = 0; k < 4; k++) {
                sum += a[i][k] * b[k][j];
            }

            // store result
            result[i][j] = sum;
        }
    }
}

void matMul1x4With4x4(
    Matrix1x4 a,
    Matrix4x4 b,
    Matrix1x4 result) {

    // this function multiples a 1x4 by a 4x4 and stores the result in a 1x4
    int j, k;
    float sum;

    for (j = 0; j < 4; j++) {
        // multiply ith row of a by jth column of b and store the sum
        // of products in the position i,j of result
        sum = 0;

        for (k = 0; k < 4; k++) {
            sum += a[k] * b[k][j];
        }

        // store result
        result[j] = sum;
    }
}

void matIdentity4x4(Matrix4x4 a) {
    // this function creates a 4x4 identity matrix
    a[0][1] = a[0][2] = a[0][3] = 0;
    a[1][0] = a[1][2] = a[1][3] = 0;
    a[2][0] = a[2][1] = a[2][3] = 0;
    a[3][0] = a[3][1] = a[3][2] = 0;

    // set main diagonal to 1's
    a[0][0] = a[1][1] = a[2][2] = a[3][3] = 1;
}

void matZero4x4(Matrix4x4 a) {
    // this function zero's out a 4x4 matrix
    a[0][0] = a[0][1] = a[0][2] = a[0][3] = 0;
    a[1][0] = a[1][1] = a[1][2] = a[1][3] = 0;
    a[2][0] = a[2][1] = a[2][2] = a[2][3] = 0;
    a[3][0] = a[3][1] = a[3][2] = a[3][3] = 0;
}

void matCopy4x4(Matrix4x4 source, Matrix4x4 destination) {
    // this function copies one 4x4 matrix to another
    int i, j;

    // copy the matrix row by row
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            destination[i][j] = source[i][j];
        }
    }
}

void localToWorldObject(ObjectPtr object) {
    // this function converts an object's local coordinates to world coordinates
    // by translating each point in the object by the object's current position
    int index;

    // move object from local position to world position
    for (index = 0; index < object->numVertices; index++) {
        object->verticesWorld[index].x = object->verticesLocal[index].x + object->worldPos.x;
        object->verticesWorld[index].y = object->verticesLocal[index].y + object->worldPos.y;
        object->verticesWorld[index].z = object->verticesLocal[index].z + object->worldPos.z;
    }

    // reset visibility flags for all polys
    for (index = 0; index < object->numPolys; index++) {
        object->polys[index].visible = 1;
        object->polys[index].clipped = 0;
    }
}

void createWorldToCamera(void) {
    // this function creates the global inverse transformation matrix
    // used to transform world coordinates to camera coordinates
    Matrix4x4 translate,
              rotateX,
              rotateY,
              rotateZ,
              result1,
              result2;

    // create identity matrices
    matIdentity4x4(translate);
    matIdentity4x4(rotateX);
    matIdentity4x4(rotateY);
    matIdentity4x4(rotateZ);

    // make a translation matrix based on the inverse of the viewpoint
    translate[3][0] = -ViewPoint.x;
    translate[3][1] = -ViewPoint.y;
    translate[3][2] = -ViewPoint.z;

    // make rotation matrices based on the inverse of the view angles
    // note that since we use lookup tables for sin and cos, it's hard to
    // use negative angles, so we will use that fact that cos(-x) = cos(x)
    // and sin(-x) = -sin(x) to implement the inverse instead of using
    // an offset in the lookup table or using the technique that
    // a rotation of -x = 360-x. Note the original rotation formulas will be
    // kept in parentheses, so you can better see the inversion.

    // x matrix
    rotateX[1][1] =  ( CosLook[ViewAngle.angX] );
    rotateX[1][2] = -( SinLook[ViewAngle.angX] );
    rotateX[2][1] = -(-SinLook[ViewAngle.angX] );
    rotateX[2][2] =  ( CosLook[ViewAngle.angX] );

    // y matrix
    rotateY[0][0] =  ( CosLook[ViewAngle.angY] );
    rotateY[0][2] = -(-SinLook[ViewAngle.angY] );
    rotateY[2][0] = -( SinLook[ViewAngle.angY] );
    rotateY[2][2] =  ( CosLook[ViewAngle.angY] );

    // z matrix
    rotateZ[0][0] =  ( CosLook[ViewAngle.angZ] );
    rotateZ[0][1] = -( SinLook[ViewAngle.angZ] );
    rotateZ[1][0] = -(-SinLook[ViewAngle.angZ] );
    rotateZ[1][1] =  ( CosLook[ViewAngle.angZ] );

    // multiply all the matrices together to obtain a final world to camera
    // viewing transformation matrix i.e.
    // translation * rotateX * rotateY * rotateZ
    matMul4x4With4x4(translate, rotateX, result1);
    matMul4x4With4x4(result1, rotateY, result2);
    matMul4x4With4x4(result2, rotateZ, GlobalView);
}

void worldToCameraObject(ObjectPtr object) {
    // this function converts an object's world coordinates to camera coordinates
    // by multiplying each point of the object by the inverse viewing transformation
    // matrix which is generated by concatenating the inverse of the view position
    // and the view angles the result of which is in GlobalView
    int index;

    // iterate thru all vertices of object and transform them into camera coordinates
    for (index = 0; index <= object->numVertices; index++) {
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
}

void rotateObject(ObjectPtr object, int angleX, int angleY, int angleZ) {
    // this function rotates an object relative to its own local coordinate sy,stem
    // and allows simultaneous rotations
    int index,
        product = 0;
    Matrix4x4 rotateX,
              rotateY,
              rotateZ,
              rotate,
              temp;
    float tempX,
          tempY,
          tempZ;

    // test if we need to rotate at all
    if (angleX == 0 && angleY == 0 && angleZ == 0) {
        return;
    }

    // create identity matrix
    matIdentity4x4(rotate);

    // create X rotation matrix
    if (angleX) {
        // x matrix
        matIdentity4x4(rotateX);

        rotateX[1][1] =  CosLook[angleX];
        rotateX[1][2] =  SinLook[angleX];
        rotateX[2][1] = -SinLook[angleX];
        rotateX[2][2] =  CosLook[angleX];
    }

    // create Y rotation matrix
    if (angleY) {
        matIdentity4x4(rotateY);

        rotateY[0][0] =  CosLook[angleY];
        rotateY[0][2] = -SinLook[angleY];
        rotateY[2][0] =  SinLook[angleY];
        rotateY[2][2] =  CosLook[angleY];
    }

    if (angleZ) {
        matIdentity4x4(rotateZ);

        rotateZ[0][0] =  CosLook[angleZ];
        rotateZ[0][1] =  SinLook[angleZ];
        rotateZ[1][0] = -SinLook[angleZ];
        rotateZ[1][1] =  CosLook[angleZ];
    }

    // compute final rotation matrix, determine the proper product of matrices
    // use a switch statement along with a bit pattern to determine which
    // matrices need multiplying, this is worth the time it would take
    // to concatenate matrices together that don't have any effect

    // if bit 2 of product is 1 then there is an x rotation
    // if bit 1 of product is 1 then there is a y rotation
    // if bit 0 of product is 1 then there is a z rotation

    if (angleX) {
        product |= 4;
    }

    if (angleY) {
        product |= 2;
    }

    if (angleZ) {
        product |= 1;
    }

    // compute proper final rotation matrix
    switch (product) {
        case 0: {
            // do nothing there isn't any rotation
        } break;

        case 1: {
            // final matrix = z
            matCopy4x4(rotateZ, rotate);
        } break;

        case 2: {
            // final matrix = y
            matCopy4x4(rotateY, rotate);
        } break;

        case 3: {
            // final matrix = y * z
            matMul4x4With4x4(rotateY, rotateZ, rotate);
        } break;

        case 4: {
            // final matrix = x
            matCopy4x4(rotateX, rotate);
        } break;

        case 5: {
            // final matrix = x * z
            matMul4x4With4x4(rotateX, rotateZ, rotate);
        } break;

        case 6: {
            // final matrix = x * y
            matMul4x4With4x4(rotateX, rotateY, rotate);
        } break;

        case 7: {
            // final matrix = x * y * z
            matMul4x4With4x4(rotateX, rotateY, temp);
            matMul4x4With4x4(temp, rotateZ, rotate);
        } break;

        default:
            break;
    }

    // now multiply each point in object by transformation matrix
    for (index = 0; index < object->numVertices; index++) {
        // x component
        tempX =
            object->verticesLocal[index].x * rotate[0][0] +
            object->verticesLocal[index].y * rotate[1][0] +
            object->verticesLocal[index].z * rotate[2][0];

        // y component
        tempY =
            object->verticesLocal[index].x * rotate[0][1] +
            object->verticesLocal[index].y * rotate[1][1] +
            object->verticesLocal[index].z * rotate[2][1];

        // z component
        tempZ =
            object->verticesLocal[index].x * rotate[0][2] +
            object->verticesLocal[index].y * rotate[1][2] +
            object->verticesLocal[index].z * rotate[2][2];

        // store rotated point back into local array
        object->verticesLocal[index].x = tempX;
        object->verticesLocal[index].y = tempY;
        object->verticesLocal[index].z = tempZ;
    }
}

void translateObject(ObjectPtr object, int xTrans, int yTrans, int zTrans) {
    // this function translates an object relative to its own local coordinate system
    object->worldPos.x += xTrans;
    object->worldPos.y += yTrans;
    object->worldPos.z += zTrans;
}

int objectsCollide(ObjectPtr object1, ObjectPtr object2) {
    // this function tests if the bounding spheres of two objects overlaps.
    // if a more accurate test is needed then polygons should be tested against
    // polygons. note the function uses the fact that if x > y then x^2 > y^2
    // to avoid using square roots. Finally, the function might be altered
    // so that the bounding spheres are shrank to make sure that the collision
    // is "solid". finally, soft and hard collisions are both detected
    float dx, dy, dz,
          radius1, radius2,
          distance;

    // compute deltas
    dx = object1->worldPos.x - object2->worldPos.x;
    dy = object1->worldPos.y - object2->worldPos.y;
    dz = object1->worldPos.z - object2->worldPos.z;

    // compute length
    distance = dx * dx + dy * dy + dz * dz;

    // compute radius of each object squared
    radius1 = object1->radius * object1->radius;
    radius2 = object2->radius * object2->radius;

    // test if distance is smaller than of radius
    if (distance < radius1 || distance < radius2) {
        return HARD_COLLISION;
    } else if (distance < radius1 + radius2) {
        return SOFT_COLLISION;
    } else {
        return NO_COLLISION;
    }
}

void scaleObject(ObjectPtr object, float scaleFactor) {
    // this function scales an object relative to its own local coordinate system
    // equally in x, y, z
    int currPoly,
        currVertex;
    float scale2;   // holds the square of the scaling factor, needed to
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

void positionObject(ObjectPtr object, int x, int y, int z) {
    // this function positions an object in the world
    object->worldPos.x = x;
    object->worldPos.y = y;
    object->worldPos.z = z;
}

char* plgGetLine(
    char* string,
    int maxLength,
    FILE* fp) {

    // this function gets a line from PLG file and strips comments
    // just pretend it's a black box!
    char buffer[80];
    int length,
        index = 0,
        index2 = 0,
        parsed = 0; // has the current input line been parsed

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

int plgLoadObject(
    ObjectPtr object,
    char* filename,
    float scale) {

    // this function loads an object off disk and allows it to be scaled
    FILE* fp;
    static int IdNumber = 0;
    char buffer[80],        // holds input string
         objectName[32];    // name of 3-D object
    char* token;            // curent parsing token
    unsigned int totalVertices, // total vertices in object
                 totalPolys,    // total polygons per object
                 numVertices,   // number of vertices on a polygon
                 colorDes,      // the color descriptor of a polygon
                 logicalColor,  // the final color of polygon
                 shading,       // the type of shading used on polygon
                 index,         // looping variable
                 index2,
                 vertexNum,     // vertex numbers
                 vertex0,
                 vertex1,
                 vertex2;
    float x, y, z;              // a single vertex
    Vector3D u, v, normal;      // working vectors

    // open the disk file
    if ((fp = fopen(filename, "r")) == NULL) {
        printf("Couldn't open file %s", filename);
        return 0;
    }

    // first we are looking for the header line that has the object name and
    // the number of vertices and polygons
    if (!plgGetLine(buffer, 80, fp)) {
        printf("Error with PLG file %s", filename);
        fclose(fp);
        return 0;
    }

    // extract object name and number of vertices and polygons
    sscanf(buffer, "%s %d %d", objectName, &totalVertices, &totalPolys);

    // set proper fields in object
    object->numVertices = totalVertices;
    object->numPolys = totalPolys;
    object->state = 1;

    object->worldPos.x = 0;
    object->worldPos.y = 0;
    object->worldPos.z = 0;

    // set id number, maybe later also add the name of object in the structure???
    object->id = IdNumber++;

    // based on number of vertices, read vertex list into object
    for (index = 0; index < totalVertices; index++) {
        // read in vertex
        if (!plgGetLine(buffer, 80, fp)) {
            printf("Error with PLG file %s", filename);
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
            printf("Error with PLG file %s", filename);
            fclose(fp);
            return 0;
        }

        // initialize token getter and get first token which is color descriptor
        if (!(token = strtok(buffer, " "))) {
            printf("Error with PLG file %s", filename);
            fclose(fp);
            return 0;
        }

        // test if number if hexadecimal
        if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
            sscanf(&token[2], "%x", &colorDes);
        } else {
            colorDes = atoi(token);
        }

        // extract base color and type of shading
        logicalColor = colorDes & 0x00ff;
        shading = colorDes >> 12;

        // read number of vertices in polygon
        if (!(token = strtok(NULL, " "))) {
            printf("Error with PLG file %s", filename);
            fclose(fp);
            return 0;
        }

        if ((numVertices = atoi(token)) == 0) {
            printf("Error with PLG file (number of vertices) %s", filename);
            fclose(fp);
            return 0;
        }

        // set fields in polygon structure
        object->polys[index].numPoints = numVertices;
        object->polys[index].color = logicalColor;
        object->polys[index].shading = shading;
        object->polys[index].twoSided = 0;
        object->polys[index].visible = 1;
        object->polys[index].clipped = 0;
        object->polys[index].active = 1;

        // now read in polygon vertices list
        for (index2 = 0; index2 < numVertices; index2++) {
            // read in next vertex number
            if (!(token = strtok(NULL, " "))) {
                printf("Error with PLG file %s", filename);
                fclose(fp);
                return 0;
            }

            vertexNum = atoi(token);

            // insert vertex number into polygon
            object->polys[index].vertexList[index2] = vertexNum;
        }

        // compute length of the two co-planar edges of the polygon, since they
        // will be used in the computation of the dot-product later
        vertex0 = object->polys[index].vertexList[0];
        vertex1 = object->polys[index].vertexList[1];
        vertex2 = object->polys[index].vertexList[2];

        // the vector u = v0->v1
        makeVector3D(
            &object->verticesLocal[vertex0],
            &object->verticesLocal[vertex1],
            &u);

        // the vector v = v0->v2
        makeVector3D(
            &object->verticesLocal[vertex0],
            &object->verticesLocal[vertex2],
            &v);

        crossProduct3D(&v, &u, &normal);

        // compute magnitude of normal and store in polygon structure
        object->polys[index].normalLength = vectorMag3D(&normal);
    }

    // close the file
    fclose(fp);

    // compute object radius
    computeObjectRadius(object);

    // return success
    return 1;
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
          x4, y4, z4,   // working variables used to hold vertices

          x1Compare,    // used to hold clipping points on x and y
          y1Compare,
          x2Compare,
          y2Compare,
          x3Compare,
          y3Compare,
          x4Compare,
          y4Compare;

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

            // test if this is a squad
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

                // pre-compute x comparison ranges
                x1Compare = (HALF_SCREEN_WIDTH * z1) / ViewingDistance;
                x2Compare = (HALF_SCREEN_WIDTH * z2) / ViewingDistance;
                x3Compare = (HALF_SCREEN_WIDTH * z3) / ViewingDistance;
                x4Compare = (HALF_SCREEN_WIDTH * z4) / ViewingDistance;

                // perform x test
                if (!((x1 > x1Compare || x2 > -x2Compare || x3 > -x3Compare || x4 > -x4Compare) &&
                      (x1 < x1Compare || x2 < x2Compare || x3 < x3Compare || x4 < x4Compare))) {

                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }

                // pre-compute x comparison ranges
                y1Compare = (HALF_SCREEN_HEIGHT * z1) / ViewingDistance;
                y2Compare = (HALF_SCREEN_HEIGHT * z2) / ViewingDistance;
                y3Compare = (HALF_SCREEN_HEIGHT * z3) / ViewingDistance;
                y4Compare = (HALF_SCREEN_HEIGHT * z4) / ViewingDistance;

                // perform x test
                if (!((y1 > -y1Compare || y2 > -y1Compare || y3 > -y3Compare || y4 > -y4Compare) &&
                      (y1 < y1Compare || y2 < y2Compare || y3 < y3Compare || y4 < y4Compare))) {

                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }
            } else {
                // must be a triangle, perform clipping tests on only 3 vertices

                // do clipping tests

                // perform near and far z clipping test first
                if (!((z1 > ClipNearZ || z2 > ClipNearZ || z3 > ClipNearZ) &&
                      (z1 < ClipFarZ || z2 < ClipFarZ || z3 < ClipFarZ))) {

                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }

                // pre-compute x comparison ranges
                x1Compare = (HALF_SCREEN_WIDTH * z1) / ViewingDistance;
                x2Compare = (HALF_SCREEN_WIDTH * z2) / ViewingDistance;
                x3Compare = (HALF_SCREEN_WIDTH * z3) / ViewingDistance;

                // perform x test
                if (!((x1 > -x1Compare || x2 > -x2Compare || x3 > -x3Compare) &&
                      (x1 < x1Compare || x2 < x2Compare || x3 < x3Compare))) {

                    // set clipped flag
                    object->polys[currPoly].clipped = 1;
                    continue;
                }

                // pre-compute x comparison ranges
                y1Compare = (HALF_SCREEN_HEIGHT * z1) / ViewingDistance;
                y2Compare = (HALF_SCREEN_HEIGHT * z2) / ViewingDistance;
                y3Compare = (HALF_SCREEN_HEIGHT * z3) / ViewingDistance;

                // perform x test
                if (!((y1 > -y1Compare || y2 > -y2Compare || y3 > -y3Compare) &&
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
    // this function removes all the backfaces of an object by setting the removed
    // flag. This function assumes that the object has been transformed into
    // camera coordinates. Also, the function computes the flat shading of the
    // object
    int vertex0,
        vertex1,
        vertex2,
        currPoly;
    float dp,
          intensity;
    Vector3D u,
             v,
             normal,
             sight;

    // for each polygon in the object determine if it is pointing away from the
    // viewpoint and direction
    for (currPoly = 0; currPoly < object->numPolys; currPoly++) {
        // compute two vectors on polygon that have the same initial points
        vertex0 = object->polys[currPoly].vertexList[0];
        vertex1 = object->polys[currPoly].vertexList[1];
        vertex2 = object->polys[currPoly].vertexList[2];

        // the vector u = v0 - v1
        makeVector3D(
            &object->verticesWorld[vertex0],
            &object->verticesWorld[vertex1],
            &u);

        // the vector v = v0 - v2
        makeVector3D(
            &object->verticesWorld[vertex0],
            &object->verticesWorld[vertex2],
            &v);

        // compute the normal to polygon v x u
        crossProduct3D(&v, &u, &normal);

        // compute the line of sight vector, since all coordinates are world all
        // object vertices are already relative to (0,0,0)
        sight.x = ViewPoint.x - object->verticesWorld[vertex0].x;
        sight.y = ViewPoint.y - object->verticesWorld[vertex0].y;
        sight.z = ViewPoint.z - object->verticesWorld[vertex0].z;

        // compute the dot product between line of sight vector and normal to surface
        dp = dotProduct3D(&normal, &sight);

        // set the clip flagged appropriately
        if (dp > 0) {
            // set visibility
            object->polys[currPoly].visible = 1;

            // compute light intensity if needed
            if (object->polys[currPoly].shading == FLAT_SHADING) {
                // compute the dot product between the light source vector
                // and normal vector to surface
                dp = dotProduct3D(&normal, &LightSource);

                // test if light ray is reflecting off surface
                if (dp > 0) {
                    // now cos0 = (u.v)/|u||v|
                    intensity = AmbientLight + 15 * dp / object->polys[currPoly].normalLength;

                    // test if intensity has overflowed
                    if (intensity > 15) {
                        intensity = 15;
                    }

                    // intensity now varies from 0-1, 0 being black or grazing and 1 being
                    // totally illuminated. use the value to index into color table
                    object->polys[currPoly].shade =
                        object->polys[currPoly].color - intensity;
                    // printf("intensity of polygon %d is %f", currPoly, intensity);
                } else {
                    object->polys[currPoly].shade =
                        object->polys[currPoly].color - AmbientLight;
                }
            } else {
                // assume constant shading and simply assign color to shade
                object->polys[currPoly].shade = object->polys[currPoly].color;
            }
        } else {
            object->polys[currPoly].visible = 0; // set invisible flag
        }

#if DEBUG
        printf("polygon #%d\n", currPoly);
        printf("u      = [%f,%f,%f]\n", u.x, u.y, u.z);
        printf("v      = [%f,%f,%f]\n", v.x, v.y, v.z);
        printf("normal = [%f,%f,%f]\n", normal.x, normal.y, normal.z);
        printf("sight  = [%f,%f,%f]\n", sight.x, sight.y, sight.z);
        printf("dp     = %f\n", dp);
#endif
    }
}

int removeObject(ObjectPtr object, int mode) {
    // this function determines if an entire object is within the viewing volume
    // or not by testing if the bounding sphere of the object in question
    // is within the viewing volume. In essence, this function "culls" entire objects
    float xBSphere,     // the x, y, z components of the projected center of object
          yBSphere,
          zBSphere,
          radius,       // the radius of object
          xCompare,     // the extents of the clipping volume in x and y at the
          yCompare;     // bounding spheres current z

    // first transform world position of object into camera coordinates

    // compute x component
    xBSphere =
        object->worldPos.x * GlobalView[0][0] +
        object->worldPos.y * GlobalView[1][0] +
        object->worldPos.z * GlobalView[2][0] +
        GlobalView[3][0];

    // compute y component
    yBSphere =
        object->worldPos.x * GlobalView[0][1] +
        object->worldPos.y * GlobalView[1][1] +
        object->worldPos.z * GlobalView[2][1] +
        GlobalView[3][1];

    // compute z component
    zBSphere =
        object->worldPos.x * GlobalView[0][2] +
        object->worldPos.y * GlobalView[1][2] +
        object->worldPos.z * GlobalView[2][2] +
        GlobalView[3][2];

    // extract radius of object
    radius = object->radius;

    if (mode == OBJECT_CULL_Z_MODE) {
        // first test against near and far z planes
        if (((zBSphere - radius) > ClipFarZ) ||
            ((zBSphere + radius) < ClipNearZ)) {

            return 1;
        } else {
            return 0;
        }
    } else {
        // perform full x,y,z test
        if (((zBSphere - radius) > ClipFarZ) ||
            ((zBSphere + radius) < ClipNearZ)) {

            return 1;
        }

        // test against x right and left planes, first compute viewing volume
        // extents at position z position of bounding sphere
        xCompare = (HALF_SCREEN_WIDTH * zBSphere) / ViewingDistance;

        if (((xBSphere - radius) > xCompare) ||
            ((xBSphere + radius) < -xCompare)) {

            return 1;
        }

        yCompare = (INVERSE_ASPECT_RATIO * HALF_SCREEN_HEIGHT * zBSphere) / ViewingDistance;

        if (((yBSphere - radius) > yCompare) ||
            ((yBSphere + radius) < -yCompare)) {

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
            WorldPolyStorage[NumPolysFrame].color = object->polys[currPoly].color;
            WorldPolyStorage[NumPolysFrame].shade = object->polys[currPoly].shade;
            WorldPolyStorage[NumPolysFrame].shading = object->polys[currPoly].shading;
            WorldPolyStorage[NumPolysFrame].twoSided = object->polys[currPoly].twoSided;
            WorldPolyStorage[NumPolysFrame].visible = object->polys[currPoly].visible;
            WorldPolyStorage[NumPolysFrame].clipped = object->polys[currPoly].clipped;
            WorldPolyStorage[NumPolysFrame].active = object->polys[currPoly].active;

            // now copy vertices
            for (currVertex = 0; currVertex < object->polys[currPoly].numPoints; currVertex++) {
                // extract vertex number
                vertex = object->polys[currPoly].vertexList[currVertex];

                // extract x,y and z
                WorldPolyStorage[NumPolysFrame].vertexList[currVertex].x = object->verticesCamera[vertex].x;
                WorldPolyStorage[NumPolysFrame].vertexList[currVertex].y = object->verticesCamera[vertex].y;
                WorldPolyStorage[NumPolysFrame].vertexList[currVertex].z = object->verticesCamera[vertex].z;
            }

            // assign pointer to it
            WorldPolys[NumPolysFrame] = &WorldPolyStorage[NumPolysFrame];

            // increment number of polys
            NumPolysFrame++;
        }
    }
}

int polyCompare(FacetPtr* arg1, FacetPtr* arg2) {
    // this function compares the average z's of two polygons and is used by the
    // depth sort surface ordering algorithm
    float z1, z2;
    FacetPtr poly1, poly2;

    // dereference the poly pointers
    poly1 = (FacetPtr)*arg1;
    poly2 = (FacetPtr)*arg2;

    // compute z average of each polygon
    if (poly1->numPoints == 3) {
        // compute average of 3 point polygon
        z1 = 0.33333f * (poly1->vertexList[0].z +
                         poly1->vertexList[1].z +
                         poly1->vertexList[2].z);
    } else {
        // compute average of 4 point polygon
        z1 = 0.25f * (poly1->vertexList[0].z +
                      poly1->vertexList[1].z +
                      poly1->vertexList[2].z +
                      poly1->vertexList[3].z);
    }

    // now polygon 2
    if (poly2->numPoints == 3) {
        // compute average of 3 point polygon
        z2 = 0.33333f * (poly2->vertexList[0].z +
                         poly2->vertexList[1].z +
                         poly2->vertexList[2].z);
    } else {
        // compuare average of 4 point polygon
        z2 = 0.25f * (poly2->vertexList[0].z +
                      poly2->vertexList[1].z +
                      poly2->vertexList[2].z +
                      poly2->vertexList[3].z);
    }

    // compare z1 and z2, such that polys' will be sorted in descending Z order
    if (z1 > z2) {
        return -1;
    } else {
        if (z1 < z2) {
            return 1;
        } else {
            return 0;
        }
    }
}

void sortPolyList(void) {
    // this function does a simple z sort on the poly list to order surfaces
    // the list is sorted in descending order, i.e. farther polygons first
    qsort(
        (void*)WorldPolys,
        NumPolysFrame,
        sizeof(FacetPtr),
        (int (__watcall*)(void const*, void const*))polyCompare);
}

void projectPolys(void) {
    // this function performs the final 3-D to 2-D perspective projection of the
    // polygons
}

void drawLine(
    int x0, int y0,
    int x1, int y1,
    unsigned char color,
    unsigned char FAR* vbStart) {

    // this function draws a line from x0,y0 to x1,y1 using differential error
    // terms (based on Bresenham's work)
    int dx,         // difference in x's
        dy,         // difference in y's
        xInc,       // amount in pixel space to move during drawing
        yInc,       // amount in pixel space to move during drawing
        error = 0,  // the discriminant i.e. error i.e. decision variable
        index;      // used for looping

    // pre-compute first pixel address in video buffer
    vbStart = vbStart + ((y0 << 6) + (y0 << 8) + x0);

    // compute horizontal and vertical deltas
    dx = x1 - x0;
    dy = y1 - y0;

    // test which direction the line is going in i.e. slope angle
    if (dx >= 0) {
        xInc = 1;
    } else {
        xInc = -1;
        dx = -dx;   // need absolute value
    }

    // test y component of slope
    if (dy >= 0) {
        yInc = 320; // 320 bytes per line
    } else {
        yInc = -320;
        dy = -dy;   // need absolute value
    }

    // now based on which delta is greater we can draw the line
    if (dx > dy) {
        // draw the line
        for (index = 0; index <= dx; index++) {
            // set the pixel
            *vbStart = color;

            // adjust the error term
            error += dy;

            // test if error has overflowed
            if (error > dx) {
                error -= dx;

                // move to next line
                vbStart += yInc;
            }

            // move to the next pixel
            vbStart += xInc;
        }
    } else {
        // draw the line
        for (index = 0; index <= dy; index++) {
            // set the pixel
            *vbStart = color;

            // adjust the error term
            error += dx;

            // test if error overflowed
            if (error > 0) {
                error -= dy;

                // move to next line
                vbStart += xInc;
            }

            // move to the next pixel
            vbStart += yInc;
        }
    }
}

void drawObjectWire(ObjectPtr object) {
    // this function draws an object out of wires
    int currPoly,
        currVertex,
        vertex;
    float x1, y1, z1,
          x2, y2, z2;
    int ix1, iy1,
        ix2, iy2;

    // compute position of object in world
    for (currPoly = 0; currPoly < object->numPolys; currPoly++) {
        // is this polygon visible?
        if (object->polys[currPoly].visible == 0 ||
            object->polys[currPoly].clipped) {

            continue;
        }

        for (currVertex = 0; currVertex < object->polys[currPoly].numPoints - 1; currVertex++) {
            // extract two endpoints
            vertex = object->polys[currPoly].vertexList[currVertex];

            x1 = object->verticesCamera[vertex].x;
            y1 = object->verticesCamera[vertex].y;
            z1 = object->verticesCamera[vertex].z;

            vertex = object->polys[currPoly].vertexList[currVertex + 1];

            x2 = object->verticesCamera[vertex].x;
            y2 = object->verticesCamera[vertex].y;
            z2 = object->verticesCamera[vertex].z;

            // convert to screen coordinates
            x1 = HALF_SCREEN_WIDTH + x1 * ViewingDistance / z1;
            y1 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y1 * ViewingDistance / z1;

            x2 = HALF_SCREEN_WIDTH + x2 * ViewingDistance / z2;
            y2 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y2 * ViewingDistance / z2;

            // convert floats to integers for line clipper
            ix1 = (int)x1;
            iy1 = (int)y1;
            ix2 = (int)x2;
            iy2 = (int)y2;

            // draw clipped lines
            if (clipLine(&ix1, &iy1, &ix2, &iy2)) {
                drawLine(ix1, iy1, ix2, iy2, object->polys[currPoly].color, DoubleBuffer);
            }
        }

        // close polygon
        ix1 = (int)x2;
        iy1 = (int)y2;

        // extract starting point again to close polygon
        vertex = object->polys[currPoly].vertexList[0];

        x2 = object->verticesCamera[vertex].x;
        y2 = object->verticesCamera[vertex].y;
        z2 = object->verticesCamera[vertex].z;

        // compute screen coordinates
        x2 = HALF_SCREEN_WIDTH + x2 * ViewingDistance / z2;
        y2 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y2 * ViewingDistance / z2;

        // convert floats to integers
        ix2 = (int)x2;
        iy2 = (int)y2;

        // draw clipped lines
        if (clipLine(&ix1, &iy1, &ix2, &iy2)) {
            drawLine(ix1, iy1, ix2, iy2, object->polys[currPoly].color, DoubleBuffer);
        }
    }
}

void drawObjectSolid(ObjectPtr object) {
    // this function draws an object shaded solid and can perform
    // simple z extent clipping
    int currPoly,
        vertex1,
        vertex2,
        vertex3,
        vertex4,
        isQuad = 0;
    float x1, y1, z1,
          x2, y2, z2,
          x3, y3, z3,
          x4, y4, z4;

    // compute position of object in world
    for (currPoly = 0; currPoly < object->numPolys; currPoly++) {
        // is this polygon visible?
        if (object->polys[currPoly].visible == 0 ||
            object->polys[currPoly].clipped) {

            continue;
        }

        // extract the vertex numbers
        vertex1 = object->polys[currPoly].vertexList[0];
        vertex2 = object->polys[currPoly].vertexList[1];
        vertex3 = object->polys[currPoly].vertexList[2];

        // do Z clipping first before projection
        z1 = object->verticesCamera[vertex1].z;
        z2 = object->verticesCamera[vertex2].z;
        z3 = object->verticesCamera[vertex3].z;

        // test if this is a quad
        if (object->polys[currPoly].numPoints == 4) {
            // extract vertex number and z component for clipping and projection
            vertex4 = object->polys[currPoly].vertexList[3];
            z4 = object->verticesCamera[vertex4].z;

            // set quad flag
            isQuad = 1;
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
        x1 = object->verticesCamera[vertex1].x;
        y1 = object->verticesCamera[vertex1].y;

        x2 = object->verticesCamera[vertex2].x;
        y2 = object->verticesCamera[vertex2].y;

        x3 = object->verticesCamera[vertex3].x;
        y3 = object->verticesCamera[vertex3].y;

        // compute screen position of points
        x1 = HALF_SCREEN_WIDTH + x1 * ViewingDistance / z1;
        y1 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y1 * ViewingDistance / z1;

        x2 = HALF_SCREEN_WIDTH + x2 * ViewingDistance / z2;
        y2 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y2 * ViewingDistance / z2;

        x3 = HALF_SCREEN_WIDTH + x3 * ViewingDistance / z3;
        y3 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y3 * ViewingDistance / z3;

        // draw triangle
        drawTriangle2D(x1, y1, x2, y2, x3, y3, object->polys[currPoly].shade);

        // draw second poly if this is a quad
        if (isQuad) {
            // extract the point
            x4 = object->verticesCamera[vertex4].x;
            y4 = object->verticesCamera[vertex4].y;

            // project to screen
            x4 = HALF_SCREEN_WIDTH + x4 * ViewingDistance / z4;
            y4 = HALF_SCREEN_HEIGHT - ASPECT_RATIO * y4 * ViewingDistance / z4;

            // draw triangle
            drawTriangle2D(x1, y1, x3, y3, x4, y4, object->polys[currPoly].shade);
        }
    }
}

void drawPolyList(void) {
    // this function draws the global polygon list generated by calls to
    // generatePolyList
    int currPoly,   // the current polygon
        isQuad = 0; // quadrilateral flag
    float x1, y1, z1,   // working variables
          x2, y2, z2,
          x3, y3, z3,
          x4, y4, z4;

    // draw each polygon in list
    for (currPoly = 0; currPoly < NumPolysFrame; currPoly++) {
        // reset quad flag
        isQuad = 0;

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
        x1 = (HALF_SCREEN_WIDTH + x1 * ViewingDistance / z1);
        y1 = (HALF_SCREEN_HEIGHT - ASPECT_RATIO * y1 * ViewingDistance / z1);

        x2 = (HALF_SCREEN_WIDTH + x2 * ViewingDistance / z2);
        y2 = (HALF_SCREEN_HEIGHT - ASPECT_RATIO * y2 * ViewingDistance / z2);

        x3 = (HALF_SCREEN_WIDTH + x3 * ViewingDistance / z3);
        y3 = (HALF_SCREEN_HEIGHT - ASPECT_RATIO * y3 * ViewingDistance / z3);

        // draw triangle
        drawTriangle2D(
            (int)x1,
            (int)y1,
            (int)x2,
            (int)y2,
            (int)x3,
            (int)y3,
            WorldPolys[currPoly]->shade);

        // draw second poly if this is a quad
        if (isQuad) {
            // extract the point
            x4 = WorldPolys[currPoly]->vertexList[3].x;
            y4 = WorldPolys[currPoly]->vertexList[3].y;

            // project to screen
            x4 = (HALF_SCREEN_WIDTH + x4 * ViewingDistance / z4);
            y4 = (HALF_SCREEN_HEIGHT - ASPECT_RATIO * y4 * ViewingDistance / z4);

            // draw triangle
            drawTriangle2D(
                (int)x1,
                (int)y1,
                (int)x3,
                (int)y3,
                (int)x4,
                (int)y4,
                WorldPolys[currPoly]->shade);
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
    if (x1 == x2 && x2 == x3 || y1 == y2 && y2 == y3) {
        return;
    }

    // sort p1, p2, p3 in ascending y order
    if (y2 < y1) {
        tempX = x2;
        tempY = y2;
        x2 = x1;
        y2 = y1;
        x1 = tempX;
        y1 = tempY;
    }

    // now we know that p1 and p2 are in order
    if (y3 < y1) {
        tempX = x3;
        tempY = y3;
        x3 = x1;
        y3 = y1;
        x1 = tempX;
        y1 = tempY;
    }

    // finally test y3 against y2
    if (y3 < y2) {
        tempX = x3;
        tempY = y3;
        x3 = x2;
        y3 = y2;
        x2 = tempX;
        y2 = tempY;
    }

    // do trivial rejection tests
    if (y3 < PolyClipMinY ||
        y1 > PolyClipMaxY ||
        x1 < PolyClipMinX && x2 < PolyClipMinX && x3 < PolyClipMinX ||
        x1 > PolyClipMaxX && x2 > PolyClipMaxX && x3 > PolyClipMaxX) {

        return;
    }

    // test if top of triangle is flat
    if (y1 == y2) {
        drawTopTriangle(x1, y1, x2, y2, x3, y3, color);
    } else if (y2 == y3) {
        drawBottomTriangle(x1, y1, x2, y2, x3, y3, color);
    } else {
        // general triangle that needs to be broken up along long edge
        newX = x1 + (int)((float)(y2 - y1) * (float)(x3 - x1) / (float)(y3 - y1));

        // draw each sub-triangle
        drawBottomTriangle(x1, y1, newX, y2, x2, y2, color);
        drawTopTriangle(x2, y2, newX, y2, x3, y3, color);
    }
}

void drawTopTriangle(
    int x1, int y1,
    int x2, int y2,
    int x3, int y3,
    int color) {

    // this function draws a triangle that has a flat top
    float dxRight,  // the dx/dy ratio of the right edge of line
          dxLeft,   // the dx/dy ratio of the left edge of line
          xs,       // the starting and ending points of the edges
          xe,
          height;   // the height of the triangle
    int tempX,      // used during sorting as temps
        tempY,
        right,      // used by clipping
        left;
    unsigned char FAR* destAddr;

    if (x2 < x1) {
        tempX = x2;
        x2 = x1;
        x1 = tempX;
    }

    // compute delta's
    height = y3 - y1;
    dxLeft = (x3 - x1) / height;
    dxRight = (x3 - x2) / height;

    // set starting points
    xs = (float)x1;
    xe = (float)x2 + 0.5f;

    // perform y clipping
    if (y1 < PolyClipMinY) {
        // compute new xs and ys
        xs = xs + dxLeft * (float)(-y1 + PolyClipMinY);
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
        for (tempY = y1; tempY <= y3; tempY++, destAddr += 320) {
            triangleLine(destAddr, (unsigned int)xs, (unsigned int)xe, color);

            // adjust starting point and ending point
            xs += dxLeft;
            xe += dxRight;
        }
    } else {
        // clip x axis with slower version

        // draw the triangle
        for (tempY = y1; tempY <= y3; tempY++, destAddr += 320) {
            // do x clip
            left = (int)xs;
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

            triangleLine(destAddr, (unsigned int)left, (unsigned int)right, color);
        }
    }
}

void drawBottomTriangle(
    int x1, int y1,
    int x2, int y2,
    int x3, int y3,
    int color) {

    // this function draws a triangle that has a flat bottom
    float dxRight,  // the dx/dy ratio of the right edge of line
          dxLeft,   // the dx/dy ratio of the left edge of line
          xs,       // the starting and ending points of the edges
          xe,
          height;   // the height of the triangle
    int tempX,      // used during sorting as temps
        tempY,
        right,      // used by clipping
        left;
    unsigned char FAR* destAddr;

    // test order of x1 and x2
    if (x3 < x2) {
        tempX = x2;
        x2 = x3;
        x3 = tempX;
    }

    // compute delta's
    height = y3 - y1;
    dxLeft = (x2 - x1) / height;
    dxRight = (x3 - x1) / height;

    // set starting points
    xs = (float)x1;
    xe = (float)x1 + 0.5f;

    // perform y clipping
    if (y1 < PolyClipMinY) {
        // compute new xs and ys
        xs = xs + dxLeft * (float)(-y1 + PolyClipMinY);
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
        for (tempY = y1; tempY <= y3; tempY++, destAddr += 320) {
            triangleLine(destAddr, (unsigned int)xs, (unsigned int)xe, color);

            // adjust starting point and ending point
            xs += dxLeft;
            xe += dxRight;
        }
    } else {
        // clip x axis with slower version

        // draw the triangle
        for (tempY = y1; tempY <= y3; tempY++, destAddr += 320) {
            // do x clip
            left = (int)xs;
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

            triangleLine(destAddr, (unsigned int)left, (unsigned int)right, color);
        }
    }
}

void triangleLine(
    unsigned char FAR* destAddr,
    unsigned int xs,
    unsigned int xe,
    int color) {

    // this function draws a fast horizontal line by using WORD size writes
    // it's speed can be doubled by use of an external 32 bit DWORD version in
    // assembly...

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
        mov edi, destAddr           ; load flat pointer
        mov al, BYTE PTR color      ; setup color
        mov ah, al                  ; replicate to ax
        mov bx, ax                  ; copy to bx
        shl eax, 16                 ; shift to upper word
        mov ax, bx                  ; eax has 4 copies of color

        mov ecx, xe                 ; compute number of pixels
        sub ecx, xs
        inc ecx
        shr ecx, 2                  ; divide by 4 for dwords

        rep stosd                   ; draw with dwords

        ; handle remaining pixels (0-3 pixels)
        mov ecx, xe
        sub ecx, xs
        inc ecx
        and ecx, 3                  ; get remainder (pixels % 4)
        rep stosb                   ; draw remaining pixels
    }
#else
    _asm {
        les di,destAddr         ; point es:di at data area

        mov al,BYTE PTR color   ; move into al and ah the color
        mov ah,al

        mov cx,xe               ; compute number of words to move (xe - xs + 1)/2
        sub cx,xs
        inc cx
        shr cx,1                ; divide by 2

        rep stosw               ; draw the line
    }
#endif
}

void makeGreyPalette(void) {
    // this function generates 64 shades of grey and places them in the palette
    // at locations 16 to 96
    int index;
    RgbColor color;

    // generate 64 shades of grey
    for (index = 0; index < 64; index++) {
        // grey equals equal percentage of Red, Green and Blue
        color.red = index;
        color.green = index;
        color.blue = index;

        // write the color in the palette starting at location 16, so as not to
        // fry the EGA palette
        writeColorReg(index + 16, &color);
    }
}

void drawTriangle2DGouraud(
    int x1, int y1,
    int x2, int y2,
    int x3, int y3,
    unsigned char FAR* buffer,
    int intensity1,
    int intensity2,
    int intensity3) {

    // this function draws a Gouraud shaded triangle, but doesn't perform any clipping
    // and is very slow (mostly for demonstrative purposes)
    int x,
        y,
        bottom1,            // distance from top to middle of triangle on y axis
        bottom2;            // distance from middle to bottom triangle on y axis
    float dxRight,          // the dx/dy ratio of the right edge of line
          dxLeft,           // the dx/dy ratio of the left edge of line
          xs,               // the starting and ending points of the edges
          xe,
          heightLeft,       // the heights of the triangle
          heightRight;
    float intensityRight,   // the intensity of the right edge of the triangle
          intensityLeft,    // the intensity of the left edge of the triangle
          intensityMid,     // the average between the right and left
          deltaY21,         // the y deltas
          deltaY31;
    unsigned char FAR* destAddr;    // pointer to memory space of video write

    // compute height of sub triangles
    heightLeft = y2 - y1;
    heightRight = y3 - y1;

    // compute distances from starting y vertex
    if (y2 > y3) {
        bottom1 = y3;
        bottom2 = y2;
    } else {
        bottom1 = y2;
        bottom2 = y3;
    }

    // compute edge deltas
    dxLeft = (float)(x2 - x1) / heightLeft;
    dxRight = (float)(x3 - x1) / heightRight;

    // set starting points
    xs = (float)x1;
    xe = (float)x1 + 0.5f;

    // compute shading constants
    deltaY21 = 1.0f / (float)(y2 - y1);
    deltaY31 = 1.0f / (float)(y3 - y1);

    // compute starting address in video memory
    destAddr = buffer + (y1 << 8) + (y1 << 6);

    // draw the triangle using Gouraud shading
    for (y = y1; y <= bottom1; y++, destAddr += 320) {
        // compute left and right edge intensities as a function of y
        intensityLeft = deltaY21 * (float)((y2 - y) * intensity1 + (y - y1) * intensity2);
        intensityRight = deltaY31 * (float)((y3 - y) * intensity1 + (y - y1) * intensity3);

        // draw line
        for (x = (int)xs; x <= (int)xe; x++) {
            // compute x axis intensity interpolant
            intensityMid = ((xe - x) * intensityLeft + (x - xs) * intensityRight) / (xe - xs);

            // plot pixel on screen
            destAddr[x] = (unsigned char)(16 + intensityMid);
        }

        // adjust starting point and ending point
        xs += dxLeft;
        xe += dxRight;
    }

    // now recompute slope of shorter edge to make it complete triangle
    if (y3 > y2) {
        // recompute left edge slope
        dxLeft = (float)(x3 - x2) / (float)(y3 - y2);
    } else {
        // y2 > y3, recompute right edge slope
        dxRight = (float)(x2 - x3) / (float)(y2 - y3);
    }

    // draw remainder of triangle
    for (y--; y <= bottom2; y++, destAddr += 320) {
        // compute left and right edge intensities as a function of y
        intensityLeft = (float)((y3 - y) * intensity2 + (y - y2) * intensity3) / (float)(y3 - y2);
        intensityRight = deltaY31 * (float)((y3 - y) * intensity1 + (y - y1) * intensity3);

        // draw line
        for (x = (int)xs; x <= (int)xe; x++) {
            // compute x axis intensity interpolant
            intensityMid = ((xe - x) * intensityLeft + (x - xs) * intensityRight) / (xe - xs);

            // plot pixel on screen
            destAddr[x] = (unsigned char)(16 + intensityMid);
        }

        // adjust starting point and ending point
        xs += dxLeft;
        xe += dxRight;
    }
}

void drawTriangle2DText(
    int x1, int y1,
    int x2, int y2,
    int x3, int y3,
    unsigned char FAR* buffer,
    int textureIndex) {

    // this function draws a textured triangle, but doesn't perform any clipping
    // and is very slow (mostly for demonstrative purposes)
    int x,
        y,
        xIndex,         // integer texture coordinates
        yIndex,
        bottom1,        // distance from top to middle of triangle on y axis
        bottom2;        // distance from middle to bottom triangle on y axis
    float dxRight,      // the dx/dy ratio of the right edge of line
          dxLeft,       // the dx/dy ratio of the left edge of line
          xs,           // the starting and ending points of the edges
          xe,
          heightLeft,   // the heights of the triangle
          heightRight,
          a,            // texture mapping inverse matrix elements
          b,
          c,
          d,
          det,
          ix,           // texture vectors
          iy,
          jx,
          jy,
          deltaUY,      // pre-computed deltas of texture coordinates on y axis
          deltaVY,
          uStart,       // the starting u,v coordinates on each line
          vStart,
          uCurr,        // the current u,v texture coordinates
          vCurr;
    unsigned char FAR* destAddr;    // final destination address of memory write
    unsigned char FAR* text;        // texture memory

    // assign text pointer to current texture map
    text = Textures.frames[textureIndex];

    // compute height of sub triangles
    heightLeft = y2 - y1;
    heightRight = y3 - y1;

    // compute distance from starting y vertex
    if (y2 > y3) {
        bottom1 = y3;
        bottom2 = y2;
    } else {
        bottom1 = y2;
        bottom2 = y3;
    }

    // compute edge deltas
    dxLeft = (float)(x2 - x1) / heightLeft;
    dxRight = (float)(x3 - x1) / heightRight;

    // set starting points
    xs = (float)x1;
    xe = (float)x1 + 0.5f;

    // compute vector components for texture calculations
    ix = (float)(x3 - x1);
    iy = (float)(y3 - y1);
    jx = (float)(x2 - x1);
    jy = (float)(y2 - y1);

    // compute determinant
    det = ix * jy - iy * jx;

    // compute inverse matrix
    a =  jy / det;
    b = -jx / det;
    c = -iy / det;
    d =  ix / det;

    // compute starting texture coordinates
    uStart = 0;
    vStart = 0;

    // compute delta texture coordinates for unit change in y
    deltaUY = a * dxLeft + b;
    deltaVY = c * dxLeft + d;

    // compute starting address in video memory
    destAddr = buffer + (y1 << 8) + (y1 << 6);

    // draw the triangle

    for (y = y1; y <= bottom1; y++, destAddr += 320) {
        // start off working texture coordinates for current row
        uCurr = uStart;
        vCurr = vStart;

        // draw line

        for (x = (int)xs; x <= (int)xe; x++) {
            xIndex = abs((int)(uCurr * 63 + 0.5));
            yIndex = abs((int)(vCurr * 63 + 0.5));

            // printf("u=%d v=%d  ", xIndex, yIndex);

            destAddr[x] = text[yIndex * 64 + xIndex];

            // adjust x texture coordinates
            uCurr += a;
            vCurr += c;
        }

        // adjust starting point and ending point
        xs += dxLeft;
        xe += dxRight;

        // adjust texture coordinates based on y change
        uStart += deltaUY;
        vStart += deltaVY;
    }

    // now recompute slope of shorter edge to make it complete triangle
    if (y3 > y2) {
        // recompute left edge of slope
        dxLeft = (float)(x3 - x2) / (float)(y3 - y2);
    } else {
        // y2 > y3, recompute right edge slope
        dxRight = (float)(x2 - x3) / (float)(y2 - y3);
    }

    // recompute texture space coordinate increments for y changes
    deltaUY = a * dxLeft + b;
    deltaVY = c * dxLeft + d;

    // draw remainder of triangle
    for (y--; y <= bottom2; y++, destAddr += 320) {
        // start off working texture coordinates for current row
        uCurr = uStart;
        vCurr = vStart;

        // draw line
        for (x = (int)xs; x <= (int)xe; x++) {
            // scale each texture coordinate by 64 since textures are 64x64
            xIndex = (int)(uCurr * 63);
            yIndex = (int)(vCurr * 63);

            // plot texel on screen
            destAddr[x] = text[yIndex * 64 + xIndex];

            // adjust x texture coordinates
            uCurr += a;
            vCurr += c;
        }

        // adjust starting point and ending point
        xs += dxLeft;
        xe += dxRight;

        // adjust texture coordinates based on y change
        uStart += deltaUY;
        vStart += deltaVY;
    }
}

