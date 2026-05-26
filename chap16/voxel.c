// Single texture based, ray casted voxel engine

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

#include "black3.h"
#include "black4.h"
#include "black5.h"

#define WORLD_X_SIZE        320 // width of universe
#define WORLD_Y_SIZE        200 // height of universe

// constants used to represent angles for the ray casting a 60 degree field of
// view

#define ANGLE_0             0
#define ANGLE_1             5
#define ANGLE_2             10
#define ANGLE_4             20
#define ANGLE_5             25
#define ANGLE_6             30
#define ANGLE_15            80
#define ANGLE_30            160
#define ANGLE_45            240
#define ANGLE_60            320
#define ANGLE_90            480
#define ANGLE_135           720
#define ANGLE_180           960
#define ANGLE_225           1200
#define ANGLE_270           1440
#define ANGLE_315           1680
#define ANGLE_360           1920

// there are 1920 degrees in our circle or 360/1920 is the conversion factor
// from real degrees to our degrees

#define ANGULAR_INCREMENT   ((float)0.1875)

// conversion constants from radians to degrees and vicversa

#define DEG_TO_RAD          ((float)3.1415926 / (float)180)
#define RAD_TO_DEG          ((float)180 / (float)3.1415926)

PcxPicture ImagePcx;            // general pcx image

int PlayX     = 1000,           // the current world x of player
    PlayY     = 1000,           // the current world y of player
    PlayZ     = 150,
    PlayAng   = ANGLE_90,       // the current viewing angle of player
    PlayDist  = 70,
    MountainScale = 10;         // scaling factor for mountains

float PlayDirX,                 // the direction the player is pointing in
      PlayDirY,
      PlayDirZ,
      CosLook[ANGLE_360],       // cosine look up table
      SinLook[ANGLE_360],       // sin look up table
      SphereCancel[ANGLE_60];   // cancels fish eye distortion

void lineVDb(int y1, int y2, int x, int color) {
    // draw a vertical line, note that a memset function can no longer be
    // used since the pixel addresses are no longer contiguous in memory

    unsigned char FAR* startOffset; // starting memory offset of line

    int index,  // loop index
        temp;   // used for temporary storage during swap

    // make sure y2 > y1
    if (y1 > y2) {
        temp = y1;
        y1   = y2;
        y2   = temp;
    }

    // clip to screen bounds.  the caller can produce y values outside
    // [0, 199] when scale (= MountainScale * pixelColor / rayLength) gets
    // larger than 50+row at low altitudes, and a negative y1 would point
    // the write address well outside the double buffer.
    if (y2 < 0 || y1 > 199) {
        return;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (y2 > 199) {
        y2 = 199;
    }

    // compute starting position
    startOffset = DoubleBuffer + ((y1 << 8) + (y1 << 6)) + x;

    for (index = 0; index <= y2 - y1; index++) {
        // set the pixel
        *startOffset = (unsigned char)color;

        // move downward to next line
        startOffset += 320;
    }
}

int initialize(char* filename) {
    // this function builds all the look up tables for the terrain generator and
    // loads in the terrain texture map

    int ang;            // looping variable

    float radAngle;     // current angle in radians

    // create sin and cos look up first
    for (ang = 0; ang < ANGLE_360; ang++) {
        // compute current angle in radians
        radAngle = (float)ang * ANGULAR_INCREMENT * DEG_TO_RAD;

        // now compute the sin and cos
        SinLook[ang] = sin(radAngle);
        CosLook[ang] = cos(radAngle);
    }

    // create inverse cosine viewing distortion filter
    for (ang = 0; ang < ANGLE_30; ang++) {
        // compute current angle in radians
        radAngle = (float)ang * ANGULAR_INCREMENT * DEG_TO_RAD;

        // now compute the sin and cos
        SphereCancel[ang + ANGLE_30] = 1 / cos(radAngle);
        SphereCancel[ANGLE_30 - ang] = 1 / cos(radAngle);
    }

    // intialize the pcx structure
    pcxInit((PcxPicturePtr)&ImagePcx);

    // load in the textures
    return pcxLoad(filename, (PcxPicturePtr)&ImagePcx, 1);
}

void drawTerrain(
    int playX,
    int playY,
    int playZ,
    int playAng,
    int playDist) {

    // this function draws the entire terrain based on the location and orientation
    // of the player's viewpoint

    int currAng,        // current angle being processed
        xr, yr,         // location of ray in world coords
        xFine, yFine,   // the texture coordinates the ray hit
        pixelColor,     // the color of textel
        ray,            // looping variable
        row,            // the current video row begin processed
        rowInv,         // the inverted row to make upward positive
        scale,          // the scale of the current strip
        top,            // top of strip
        bottom;         // bottom of strip

    float rayLength;    // the length of the ray after distortion compensation

    // start the current angle off -30 degrees to the left of the player's
    // current viewing direction
    currAng = playAng - ANGLE_30;

    // test for underflow
    if (currAng < 0) {
        currAng += ANGLE_360;
    }

    // cast a series of rays for every column of the screen
    for (ray = 1; ray < 320; ray++) {
        // for each column compute the pixels that should be displayed
        // for each screen pixel, process from top to bottom
        for (row = 100; row < 150; row++) {
            // compute length of ray
            rowInv = 200 - row;

            // use the current height and distance to compute length of ray.
            // promote the multiplication to float so the product can't
            // overflow a 16-bit int when playDist and playZ get large.
            rayLength = SphereCancel[ray] * (((float)playDist * playZ) /
                                              (float)(playZ - rowInv));

            // rotate ray into position of sample
            xr = (int)((float)playX + rayLength * CosLook[currAng]);
            yr = (int)((float)playY - rayLength * SinLook[currAng]);

            // compute texture coords
            xFine = xr % WORLD_X_SIZE;
            yFine = yr % WORLD_Y_SIZE;

            // using texture index locate texture pixel in textures
            pixelColor = ImagePcx.buffer[xFine + (yFine * 320)];

            // draw the strip
            scale = (int)MountainScale * pixelColor / (int)(rayLength + 1);

            top    = 50 + row - scale;
            bottom = top + scale;

            lineVDb(top, bottom, ray, pixelColor);
        }

        // move to next angle
        if (++currAng >= ANGLE_360) {
            currAng = ANGLE_0;
        }
    }
}

void main(int argc, char** argv) {
    char buffer[80];

    int done = 0;       // exit flag

    float speed = 0;    // speed of player

    // check to see if command line parms are correct
    if (argc <= 2) {
        // not enough parms
        printf("\nUsage: voxel.exe filename.pcx height");
        printf("\nExample: voxel voxterr4.pcx 10\n");

        // return to DOS
        exit(1);
    }

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create a double buffer
    createDoubleBuffer(200);

    // create look up tables and load textures
    if (!initialize(argv[1])) {
        printf("\nError loading file %s", argv[1]);
        exit(1);
    }

    // install keyboard driver
    keyboardInstallDriver();

#ifdef DOS_32_BIT
    // load the ROM font from font.bin
    if (!initRomCharSet()) {
        printf("\nFatal error: Could not initialize font system\n");
        return;
    }
#endif

    // set scale of mountains
    MountainScale = atoi(argv[2]);

    // draw the first frame
    drawTerrain(PlayX, PlayY, PlayZ, PlayAng, PlayDist);

    displayDoubleBuffer(DoubleBuffer, 0);

    // main event loop
    while (!done) {
        // reset velocity
        speed = 0;

        // test if user is hitting keyboard
        if (KeysActive) {
            // what is user trying to do

            // change viewing distance
            if (KeyboardState[MAKE_F]) {
                PlayDist += 10;
                if (PlayDist > 1000) {
                    PlayDist = 1000;
                }
            }

            if (KeyboardState[MAKE_C]) {
                PlayDist -= 10;
                if (PlayDist < 10) {
                    PlayDist = 10;
                }
            }

            // change viewing height — minimum 110 keeps (playZ - rowInv) > 0
            // for every row in the drawTerrain loop (rowInv ranges 51..100),
            // which prevents a divide-by-zero in the perspective formula
            if (KeyboardState[MAKE_U]) {
                PlayZ += 10;
                if (PlayZ > 1000) {
                    PlayZ = 1000;
                }
            }

            if (KeyboardState[MAKE_D]) {
                PlayZ -= 10;
                if (PlayZ < 110) {
                    PlayZ = 110;
                }
            }

            // change viewing position
            if (KeyboardState[MAKE_RIGHT]) {
                if ((PlayAng += ANGLE_5) >= ANGLE_360) {
                    PlayAng -= ANGLE_360;
                }
            }

            if (KeyboardState[MAKE_LEFT]) {
                if ((PlayAng -= ANGLE_5) < 0) {
                    PlayAng += ANGLE_360;
                }
            }

            // move foward
            if (KeyboardState[MAKE_UP]) {
                speed = 20;
            }

            // move backward
            if (KeyboardState[MAKE_DOWN]) {
                speed = -20;
            }

            // exit demo
            if (KeyboardState[MAKE_ESC]) {
                done = 1;
            }

            // compute trajectory vector for this view angle
            PlayDirX =  CosLook[PlayAng];
            PlayDirY = -SinLook[PlayAng];
            PlayDirZ = 0;

            // translate viewpoint
            PlayX += speed * PlayDirX;
            PlayY += speed * PlayDirY;
            PlayZ += speed * PlayDirZ;

            // draw the terrain
            fillDoubleBuffer(0);

            drawTerrain(PlayX, PlayY, PlayZ, PlayAng, PlayDist);

            // draw tactical
            sprintf(buffer, "Height = %d Distance = %d     ", PlayZ, PlayDist);
            printStringDb(0, 0, 10, buffer, 0);

            sprintf(buffer, "Pos: X=%d, Y=%d, Z=%d    ", PlayX, PlayY, PlayZ);
            printStringDb(0, 10, 10, buffer, 0);

            displayDoubleBuffer(DoubleBuffer, 0);
        }
    }

#ifdef DOS_32_BIT
    freeRomCharSet();
#endif

    // reset back to text mode
    setGraphicsMode(TEXT_MODE);

    // remove the keyboard handler
    keyboardRemoveDriver();

#ifdef DOS_32_BIT
    exitToDos(0);
#endif
}
