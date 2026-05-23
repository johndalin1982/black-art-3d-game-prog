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
#include "black11.h"
#include "black15.h"

#define MAX_LINES               64  // this is the maximum number of lines in the demo
                                    // these lines will later become 3-D walls

#define LINE_INVALID            0   // constants that define the state of a line
#define LINE_VALID              1

#define ACTION_STARTING_LINE    0   // used to determine if user is starting a new line
#define ACTION_ENDING_LINE      1   // or ending one
#define ACTION_DELETE_LINE      2   // deleting a line

#define GADGET_WIDTH            64  // size of the gadgets (buttons)
#define GADGET_HEIGHT           12

// the pointers

#define POINTER_CROSS           0   // the cross hair pointer used to draw walls
#define POINTER_END             1   // the modified cross hair that indicates the end of
                                    // a line is being drawn
#define POINTER_DEL             2   // this is a modified cross hair that indicates a line
                                    // is to be deleted
#define POINTER_HAND            3   // the hand pointer used to make selections

// the control buttons sprite values

#define CONTROL_CLEAR           0
#define CONTROL_DEL_WALL        1
#define CONTROL_BUILD_BSP       2
#define CONTROL_PRINT_BSP       3
#define CONTROL_VIEW            4
#define CONTROL_EXIT            5
#define CONTROL_EDITOR          6

// the control buttons interface values

#define CONTROL_ID_CLEAR        0
#define CONTROL_ID_DEL_WALL     1
#define CONTROL_ID_BUILD_BSP    2
#define CONTROL_ID_PRINT_BSP    3
#define CONTROL_ID_VIEW         4
#define CONTROL_ID_EXIT         5
#define CONTROL_ID_EDITOR       6

// control areas of GUI

#define BSP_MIN_X               0   // the bounding rectangle of the
#define BSP_MAX_X               222 // BSP editor area
#define BSP_MIN_Y               0
#define BSP_MAX_Y               199

#define INTERFACE_MIN_X         226 // the bounding rectangle of the control
#define INTERFACE_MAX_X         319 // button area to the right of editor
#define INTERFACE_MIN_Y         0
#define INTERFACE_MAX_Y         199

// this holds the ultra simple "gadget" basically a button
typedef struct GadgetType {
    int x, y;   // position of gadget
    int id;     // id number of gadget
} Gadget, *GadgetPtr;

// this structure holds the 2-D line that is used to represent vertical walls
// later in the BSP creation process
typedef struct Line2DType {
    int sx, sy; // starting point
    int ex, ey; // ending point
    int valid;  // flags if this line is valid
} Line2D, *Line2DPtr;

void convertLinesToWalls(void);

int deleteLine(int x, int y);

void waitForMouseUp(void);

int buttonPressed(GadgetPtr gadgets, int numGadgets, int posX, int posY);

PcxPicture ImagePcx,        // general PCX image used to load background and imagery
           InterfacePcx;    // holds interface

Sprite ControlsSpr,         // the sprite that holds the control buttons
       PointerSpr;

// gadget lists

Gadget BspControls[6] = {
    { 240, 94,  CONTROL_ID_CLEAR },
    { 240, 111, CONTROL_ID_DEL_WALL },
    { 240, 128, CONTROL_ID_BUILD_BSP },
    { 240, 145, CONTROL_ID_PRINT_BSP },
    { 240, 162, CONTROL_ID_VIEW },
    { 240, 179, CONTROL_ID_EXIT }
};

Line2D Lines[MAX_LINES];    // this holds the lines in the system, we could
                            // use a linked list, but let's not make this harder
                            // than it already is!

int TotalLines = 0;         // number of lines that have been defined

WallPtr WallList = NULL,    // pointer to the linked list of walls
        BspRoot = NULL;     // pointer to root of BSP tree

void convertLinesToWalls(void) {
    // this function converts the list of 2-D lines into a linked list of 3-D
    // walls, computes the normal of each wall and sets the pointers up
    // also the function labels the lines on the screen so the user can see them

    int index; // looping index

    WallPtr lastWall,   // used to track the last wall processed
            tempWall;   // used as a temporary to build a wall up

    Vector3D u, v;  // working vectors

    float length;   // a general length var

    char buffer[80];

    // process each 2-d line and convert it into a 3-d wall
    for (index = 0; index < TotalLines; index++) {
        // draw a numeric label near the line
        sprintf(buffer, "%d", index);

        printString((Lines[index].sx + Lines[index].ex) / 2,
                    (Lines[index].sy + Lines[index].ey) / 2,
                    10,
                    buffer,
                    1);

        // allocate the memory for the wall
        tempWall = (WallPtr)malloc(sizeof(Wall));

        // set up links
        tempWall->link  = NULL;
        tempWall->front = NULL;
        tempWall->back  = NULL;

        // assign points, note how y and z are transposed and the y's of the
        // walls are fixed, this is because we are looking down on the universe
        // from an aerial view and the wall height is arbitrary, however, with
        // the constants we have selected the walls are 20 units tall centered
        // about the x-z plane

        // vertex 0
        tempWall->wallWorld[0].x = WORLD_SCALE_X * (SCREEN_TO_WORLD_X + Lines[index].sx);
        tempWall->wallWorld[0].y = WALL_CEILING;
        tempWall->wallWorld[0].z = WORLD_SCALE_Z * (SCREEN_TO_WORLD_Z + Lines[index].sy);

        // vertex 1
        tempWall->wallWorld[1].x = WORLD_SCALE_X * (SCREEN_TO_WORLD_X + Lines[index].ex);
        tempWall->wallWorld[1].y = WALL_CEILING;
        tempWall->wallWorld[1].z = WORLD_SCALE_Z * (SCREEN_TO_WORLD_Z + Lines[index].ey);

        // vertex 2
        tempWall->wallWorld[2].x = WORLD_SCALE_X * (SCREEN_TO_WORLD_X + Lines[index].ex);
        tempWall->wallWorld[2].y = WALL_FLOOR;
        tempWall->wallWorld[2].z = WORLD_SCALE_Z * (SCREEN_TO_WORLD_Z + Lines[index].ey);

        // vertex 3
        tempWall->wallWorld[3].x = WORLD_SCALE_X * (SCREEN_TO_WORLD_X + Lines[index].sx);
        tempWall->wallWorld[3].y = WALL_FLOOR;
        tempWall->wallWorld[3].z = WORLD_SCALE_Z * (SCREEN_TO_WORLD_Z + Lines[index].sy);

        // compute normal to wall

        // find two vectors co-planer in the wall
        makeVector3D((Point3DPtr)&tempWall->wallWorld[0],
                     (Point3DPtr)&tempWall->wallWorld[1],
                     (Vector3DPtr)&u);

        makeVector3D((Point3DPtr)&tempWall->wallWorld[0],
                     (Point3DPtr)&tempWall->wallWorld[3],
                     (Vector3DPtr)&v);

        // use cross product to compute normal
        crossProduct3D((Vector3DPtr)&u,
                       (Vector3DPtr)&v,
                       (Vector3DPtr)&tempWall->normal);

        // normalize the normal vector
        length = vectorMag3D((Vector3DPtr)&tempWall->normal);

        tempWall->normal.x /= length;
        tempWall->normal.y /= length;
        tempWall->normal.z /= length;

        // set id number for debugging
        tempWall->id = index;

        // test if this is first wall
        if (index == 0) {
            // set head of list pointer and last wall pointer
            WallList = tempWall;
            lastWall = tempWall;
        } else {
            // the first wall has been taken care of

            // link the last wall to the next wall
            lastWall->link = tempWall;

            // move the last wall to the next wall
            lastWall = tempWall;
        }
    }
}

int deleteLine(int x, int y) {
    // this function hunts thru the lines and deletes the one closest to
    // the sent position (which is the center of the cross hairs

    int currLine,       // current line being processed
        sx, sy,         // starting coordinates of test line
        ex, ey,         // ending coordinates of test line
        lengthLine,     // total length of line being tested
        length1,        // length of lines from endpoints of test line to target area
        length2,
        minX, maxX,     // bounding box of test line
        minY, maxY,
        bestLine = -1;  // the best match so far in process

    float bestError = 10000,    // start error off really large
          testError;            // current error being processed

    // process each line and find best fit
    for (currLine = 0; currLine < TotalLines; currLine++) {
        // extract line parameters
        sx = Lines[currLine].sx;
        sy = Lines[currLine].sy;

        ex = Lines[currLine].ex;
        ey = Lines[currLine].ey;

        // first compute length of line
        lengthLine = sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy));

        // compute length of first endpoint to selected position
        length1 = sqrt((x - sx) * (x - sx) + (y - sy) * (y - sy));

        // compute length of second endpoint to selected position
        length2 = sqrt((ex - x) * (ex - x) + (ey - y) * (ey - y));

        // compute the bounding box of line
        minX = __min(sx, ex);
        minY = __min(sy, ey);
        maxX = __max(sx, ex);
        maxY = __max(sy, ey);

        // if the selection position is within bounding box then compute distance
        // errors and save this line as a possibility
        if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
            // compute percent error of total length to length of lines
            // from endpoint to selected position
            testError = (float)(100 * abs((length1 + length2) - lengthLine))
                        / (float)lengthLine;

            // test if this line is a better selection than the last
            if (testError < bestError) {
                // make this line the "best selection" so far
                bestError = testError;
                bestLine  = currLine;
            }
        }
    }

    // did we get a line to delete?
    if (bestLine != -1) {
        // delete the line from the line array by copying another line into
        // the position

        // test for special cases
        if (TotalLines == 1) {
            // a single line
            TotalLines = 0;
        } else if (bestLine == TotalLines - 1) {
            // the line to delete is the last in array
            TotalLines--;
        } else {
            // the line to delete must be in the 0th to total_lines-1 position
            // so copy the last line into the deleted one and decrement the
            // number of lines in system
            Lines[bestLine] = Lines[--TotalLines];
        }

        // redraw the interface
        pcxCopyToBuffer((PcxPicturePtr)&InterfacePcx, DoubleBuffer);

        // redraw the remaining lines
        for (currLine = 0; currLine < TotalLines; currLine++) {
            // draw a line between points
            drawLine(Lines[currLine].sx, Lines[currLine].sy,
                     Lines[currLine].ex, Lines[currLine].ey,
                     13,
                     DoubleBuffer);

            // plot endpoint pixels
            writePixelDb(Lines[currLine].sx, Lines[currLine].sy, 15);
            writePixelDb(Lines[currLine].ex, Lines[currLine].ey, 15);
        }

        // return the line number that was deleted
        return currLine;
    } else {
        return -1;
    }
}

void waitForMouseUp(void) {
    // this function waits for the mouse buttons to be released

    int mx, my,         // mouse position dummy variables
        buttons = 1;    // mouse buttons

    // loop until mouse is released
    while (buttons) {
        mouseControl(MOUSE_POSITION_BUTTONS, &mx, &my, &buttons);
    }
}

int buttonPressed(GadgetPtr gadgets, int numGadgets, int posX, int posY) {
    // this function takes as input an array of gadgets (buttons) and determines
    // which button is pressed, then the function returns the id number of the
    // gadget or -1 if there isn't a hit

    int currGadget; // looping variable

    // loop thru all gadgets in list and try to find the right one
    for (currGadget = 0; currGadget < numGadgets; currGadget++) {
        // test if sent selection position is within bounding box of one of the
        // gadgets
        if (posX >  gadgets[currGadget].x                   &&
            posX < (gadgets[currGadget].x + GADGET_WIDTH)   &&
            posY >  gadgets[currGadget].y                   &&
            posY < (gadgets[currGadget].y + GADGET_HEIGHT)) {

            // click the button
            ControlsSpr.x = gadgets[currGadget].x;
            ControlsSpr.y = gadgets[currGadget].y;

            ControlsSpr.currFrame = gadgets[currGadget].id + 7;
            spriteDrawClip((SpritePtr)&ControlsSpr, VideoBuffer, 1);
            spriteDrawClip((SpritePtr)&PointerSpr, VideoBuffer, 1);

            // wait for user to release buttons on mouse
            waitForMouseUp();

            ControlsSpr.currFrame = gadgets[currGadget].id;
            spriteDrawClip((SpritePtr)&ControlsSpr, VideoBuffer, 1);

            // return the id number of the gadget
            return gadgets[currGadget].id;
        }
    }

    // return no hit
    return -1;
}

void main(void) {
    int index,          // loop variable
        mouseX,         // mouse position
        mouseY,
        buttons,        // state of buttons
        buttonDown = 0,
        done = 0,       // system exit flag
        sel,
        action = ACTION_STARTING_LINE;  // this is used to track the current action the
                                        // user is in the middle of

    char buffer[80];    // string input buffer

    // build all look up tables for standard poly 3-d engine
    buildLookUpTables();

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

#ifdef DOS_32_BIT
    // load the ROM font from font.bin
    if (!initRomCharSet()) {
        printf("\nFatal error: Could not initialize font system\n");
        return;
    }
#endif

    // load the imagery for control buttons
    pcxInit((PcxPicturePtr)&ImagePcx);
    pcxLoad("bspbutt.pcx", (PcxPicturePtr)&ImagePcx, 1);

    // initialize the controls sprite
    spriteInit((SpritePtr)&ControlsSpr, 0, 0, 64, 12, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for control buttons
    for (index = 0; index < 14; index++) {
        pcxGetSprite((PcxPicturePtr)&ImagePcx,
                     (SpritePtr)&ControlsSpr, index, index / 7, index % 7);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete((PcxPicturePtr)&ImagePcx);

    // load the imagery for pointer
    pcxInit((PcxPicturePtr)&ImagePcx);
    pcxLoad("bsppoint.pcx", (PcxPicturePtr)&ImagePcx, 1);

    // initialize the pointer sprite
    spriteInit((SpritePtr)&PointerSpr, 0, 0, 9, 9, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for pointer
    for (index = 0; index < 4; index++) {
        pcxGetSprite((PcxPicturePtr)&ImagePcx,
                     (SpritePtr)&PointerSpr, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete((PcxPicturePtr)&ImagePcx);

    // set pointer to cross hair
    PointerSpr.currFrame = POINTER_CROSS;

    // load the imagery for interface
    pcxInit((PcxPicturePtr)&InterfacePcx);
    pcxLoad("bspint.pcx", (PcxPicturePtr)&InterfacePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer((PcxPicturePtr)&InterfacePcx, DoubleBuffer);

    // reset the mouse and hide the pointer
    mouseControl(MOUSE_RESET, NULL, NULL, &buttons);
    mouseControl(MOUSE_HIDE, NULL, NULL, NULL);

    // scan under the pointer
    spriteUnderClip((SpritePtr)&PointerSpr, DoubleBuffer);

    // main event loop, process until keyboard hit
    while (!done) {
        // erase the pointer
        spriteEraseClip((SpritePtr)&PointerSpr, DoubleBuffer);

        mouseControl(MOUSE_POSITION_BUTTONS, &mouseX, &mouseY, &buttons);

        // map the mouse position to the screen and assign it to pointer
        mouseX = PointerSpr.x = (mouseX >> 1) - 16;
        mouseY = PointerSpr.y = mouseY;

        // test if pointer icon needs to be changed
        if (PointerSpr.x > BSP_MAX_X) {
            PointerSpr.currFrame = POINTER_HAND;
        } else {
            // pointer must be in bsp editor section

            // test if a line is being deleted
            if (action == ACTION_DELETE_LINE) {
                PointerSpr.currFrame = POINTER_DEL;
            } else if (action == ACTION_STARTING_LINE) {
                PointerSpr.currFrame = POINTER_CROSS;
            } else if (action == ACTION_ENDING_LINE) {
                PointerSpr.currFrame = POINTER_END;
            }
        }

        // test if player is trying to do something
        if (buttons == MOUSE_LEFT_BUTTON) {
            // figure out which area this is happening in
            if (mouseX >= BSP_MIN_X && mouseX <= BSP_MAX_X &&
                mouseY >= BSP_MIN_Y && mouseY <= BSP_MAX_Y) {
                // process bsp editor area click

                // test if this is starting point or endpoint
                if (action == ACTION_STARTING_LINE) {
                    // set starting field, note the offsets to center the
                    // starting point in middle of cross hairs
                    Lines[TotalLines].sx = mouseX + 4;
                    Lines[TotalLines].sy = mouseY + 4;

                    // set point type to ending point
                    action = ACTION_ENDING_LINE;

                    // plot pixel
                    writePixelDb(Lines[TotalLines].sx, Lines[TotalLines].sy, 15);

                    // wait for mouse button release
                    waitForMouseUp();
                } else if (action == ACTION_ENDING_LINE) {
                    // must be the end of a wall or ending point
                    Lines[TotalLines].ex = mouseX + 4;
                    Lines[TotalLines].ey = mouseY + 4;

                    // set point type to ending point
                    action = ACTION_STARTING_LINE;

                    // draw a line between points
                    drawLine(Lines[TotalLines].sx, Lines[TotalLines].sy,
                             Lines[TotalLines].ex, Lines[TotalLines].ey,
                             13,
                             DoubleBuffer);

                    // plot endpoint pixels
                    writePixelDb(Lines[TotalLines].sx, Lines[TotalLines].sy, 15);
                    writePixelDb(Lines[TotalLines].ex, Lines[TotalLines].ey, 15);

                    // advance number of lines
                    if (++TotalLines >= 64) {
                        TotalLines = 64;
                    }

                    // wait for mouse button release
                    waitForMouseUp();
                } else if (action == ACTION_DELETE_LINE) {
                    // try and delete the line nearest selected point
                    deleteLine(mouseX + 4, mouseY + 4);

                    // wait for mouse release
                    waitForMouseUp();

                    // reset action to starting line
                    action = ACTION_STARTING_LINE;
                }
            } else {
                // must be a click in the interface control area
                sel = buttonPressed((GadgetPtr)BspControls, 6, mouseX, mouseY);

                // which button was pressed
                switch (sel) {
                    case CONTROL_ID_CLEAR: {
                        // clear the screen and delete all lines

                        // reset number of lines
                        TotalLines = 0;

                        // reset state
                        action = ACTION_STARTING_LINE;

                        // refresh screen
                        pcxCopyToBuffer((PcxPicturePtr)&InterfacePcx, DoubleBuffer);

                        // delete BSP tree if there is one
                        if (BspRoot) {
                            // delete the bsp tree
                            bspDelete(BspRoot);

                            // reset pointers
                            WallList = BspRoot = NULL;
                        }
                    } break;

                    case CONTROL_ID_DEL_WALL: {
                        // delete a single line (wall)
                        action = ACTION_DELETE_LINE;
                    } break;

                    case CONTROL_ID_BUILD_BSP: {
                        // show the building of bsp

                        // delete the last bsp tree if there is one
                        bspDelete(BspRoot);

                        // convert 2-d lines to 3-d walls
                        convertLinesToWalls();

                        // build the 3-D bsp tree
                        buildBspTree(WallList);

                        // alias the bsp tree to the wall list first node
                        // which has now become the root of the tree
                        BspRoot = WallList;

                        // translate the walls in the bsp tree out on the
                        // z axis a bit
                        bspTranslate(BspRoot, WORLD_POS_X, WORLD_POS_Y, WORLD_POS_Z);

                        // shade the walls in the bsp
                        bspShade(BspRoot);

                        // wait for user to key keyboard to continue
                        getch();
                    } break;

                    case CONTROL_ID_PRINT_BSP: {
                        // print the bsp data structure

                        // open output file
                        FpOut = fopen("bspdemo.out", "w");

                        fprintf(FpOut, "\nPrinting BSP tree:\n");

                        bspPrint(BspRoot);

                        fclose(FpOut);
                    } break;

                    case CONTROL_ID_VIEW: {
                        // fly thru 3-d bsp world

                        // call the self contained viewing function
                        bspView(BspRoot);

                        // clear the editor out for a new bsp

                        // reset number of lines
                        TotalLines = 0;

                        // reset state
                        action = ACTION_STARTING_LINE;

                        // refresh screen
                        pcxCopyToBuffer((PcxPicturePtr)&InterfacePcx, DoubleBuffer);

                        // delete BSP tree if there is one
                        if (BspRoot) {
                            // delete the bsp tree
                            bspDelete(BspRoot);

                            // reset pointers
                            WallList = BspRoot = NULL;
                        }
                    } break;

                    case CONTROL_ID_EXIT: {
                        // exit system

                        // set exit flag
                        done = 1;
                    } break;

                    default: break;
                }
            }
        }

        if (buttons == MOUSE_RIGHT_BUTTON) {
            // reset system to starting line
            action = ACTION_STARTING_LINE;
        }

        // scan under pointer
        spriteUnderClip((SpritePtr)&PointerSpr, DoubleBuffer);

        // draw the pointer
        spriteDrawClip((SpritePtr)&PointerSpr, DoubleBuffer, 1);

        // draw the coordinates
        sprintf(buffer, "(%d,%d) ", (mouseX + 4) + SCREEN_TO_WORLD_X,
                                    -((mouseY + 4) + SCREEN_TO_WORLD_Z));

        printStringDb(228, 56, 10, buffer, 0);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);
    }

    // free up all resources
    deleteDoubleBuffer();

#ifdef DOS_32_BIT
    freeRomCharSet();
#endif

    // restore graphics to text mode
    setGraphicsMode(TEXT_MODE);

#ifdef DOS_32_BIT
    exitToDos(0);
#endif
}
