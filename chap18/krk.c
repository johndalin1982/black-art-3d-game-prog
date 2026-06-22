// Kill or Be Killed

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
#include "black18.h"            // chap18 engine

// defines for the radar scanner

#define SCANNER_X               12      // position of scanner
#define SCANNER_Y               135
#define SCANNER_WIDTH           57      // size of scanner area
#define SCANNER_HEIGHT          42

// commands that can be sent to the scanner engine

#define SCANNER_CLEAR           0       // clear the scanner
#define SCANNER_LOGO            1       // draw the KRK logo
#define SCANNER_ERASE_BLIPS     2       // erase all the radar blips
#define SCANNER_DRAW_BLIPS      3       // draw all the radar blips

// defines for the general multi function tactical display

#define TACTICAL_X              249     // position of tactical
#define TACTICAL_Y              134
#define TACTICAL_WIDTH          57      // size of tactical
#define TACTICAL_HEIGHT         34

// different modes of operation for the tactical display

#define TACTICAL_MODE_STS       0       // ship status mode
#define TACTICAL_MODE_HULL      1       // hull damage report
#define TACTICAL_MODE_OFF       2       // tactical display off

// these are the different icon indexes for the tactical display

#define TACTICAL_ICON_BLANK     0       // a black square
#define TACTICAL_ICON_KRK       1       // the kill or be killed logo
#define TACTICAL_ICON_GAUGE     2       // the gauge set
#define TACTICAL_ICON_TALLON    3       // the tallon hull
#define TACTICAL_ICON_SLIDER    4       // the slider hull

#define TACTICAL_ICON_WIDTH     38      // size of sprite bitmaps for tactical
#define TACTICAL_ICON_HEIGHT    28

// commands the drawing engine of the tactical display

#define TACTICAL_CLEAR          0       // clear the tactical
#define TACTICAL_DRAW           1       // draw the tactical
#define TACTICAL_UPDATE         2       // refresh the tactical

// the main menu selection system defines

#define SELECT_BOX_SX           104     // starting position of selection boxes
#define SELECT_BOX_SY           58
#define SELECT_BOX_DY           19      // vertical change between boxes
#define SELECT_BOX_WIDTH        94      // dimensions of selection boxes
#define SELECT_BOX_HEIGHT       10

#define SELECT_LGT_SX           86      // starting position of little lights by box
#define SELECT_LGT_SY           61
#define SELECT_LGT_DY           19      // vertical change to next light
#define SELECT_LGT_WIDTH        4       // dimensions of light
#define SELECT_LGT_HEIGHT       4

#define MAX_SELECTION           3       // maximum number of selections

// defines for the background mountain scape

#define MOUNTAIN_HEIGHT         43      // height of mountain
#define MOUNTAIN_WIDTH          320     // total width of backdrop
#define MOUNTAIN_Y_POS          56      // starting y position to map down backdrop

// starting position of the intro startup sequence

#define START_MESS_X            2
#define START_MESS_Y            8

// position of alpha numeric input area

#define DISPLAY_X               0
#define DISPLAY_Y               0

// defines for the aliens and logic

#define ALIEN_DEAD              0       // alien is dead
#define ALIEN_DYING             1       // alien is dying
#define ALIEN_NEW_STATE         2       // alien wishes a new state
#define ALIEN_ATTACK            3       // alien is attacking player
#define ALIEN_RANDOM            4       // alien is moving in random directions
#define ALIEN_EVADE             5       // alien is evading player
#define ALIEN_STOP              6       // alien is at a full stop
#define ALIEN_TURN              7       // alien is making a turn

#define NUM_ALIENS              4       // total number of aliens in game

#define ALIEN_TALLON            0       // id for an alien with a tallon class ship
#define ALIEN_SLIDER            1       // id for an alien with a slider class ship

// dimensions of the game world

#define GAME_MAX_WORLD_X        7500    // x dimensions
#define GAME_MIN_WORLD_X        -7500

#define GAME_MAX_WORLD_Y        400     // y dimensions
#define GAME_MIN_WORLD_Y        -400

#define GAME_MAX_WORLD_Z        7500    // z dimensions
#define GAME_MIN_WORLD_Z        -7500

#define GUN_HEIGHT              -10     // height of weapon system from ground level

#define NUM_DYNAMIC             3       // total number of dynamic game objects

#define NUM_MISSILES            12      // maximum number of missiles in the game

// possible missile states

#define MISSILE_INACTIVE        0       // an inactive missile
#define MISSILE_ACTIVE          1       // an active and moving missile

#define MAX_PLAYER_MISSILES     6       // maximum number of missiles player
                                        // can fire at once

// used to track who fired a missile, helps for collision detection

#define NO_OWNER                0       // no owner, can cause damage to anyone
#define PLAYER_OWNER            1       // the player fired it
#define ALIEN_OWNER             2       // an alien fired it
#define OTHER_OWNER             3       // an unknown fired it

// these are used to index into the 3-D models

#define MISSILES_TEMPLATE       0       // the missile model
#define TALLONS_TEMPLATE        1       // the tallon model
#define SLIDERS_TEMPLATE        2       // the slider model

// these define the number of each object type

#define NUM_STATIONARY          6       // total number of stationary object types

#define NUM_OBSTACLES_1         32      // number of rocks
#define NUM_OBSTACLES_2         32      // number of crystals
#define NUM_BARRIERS            8       // number of laser barriers
#define NUM_TOWERS              4       // number of control towers
#define NUM_STATIONS            1       // number of power stations
#define NUM_TELEPODS            4       // number of telepods

// these are the model indices

#define OBSTACLES_1_TEMPLATE    0       // the rock model
#define OBSTACLES_2_TEMPLATE    1       // the crystal model
#define BARRIERS_TEMPLATE       2       // the laser barrier model
#define TOWERS_TEMPLATE         3       // the control tower model
#define STATIONS_TEMPLATE       4       // the power station model
#define TELEPODS_TEMPLATE       5       // the telepod model

// digital sound system

#define KRKMIS_VOC              0       // the sound a missile makes
#define KRKEMIS_VOC             1       // the sound an enemy missile makes
#define KRKTAC_VOC              2       // tactical engaging
#define KRKSCN_VOC              3       // scanner engaging
#define KRKHUD_VOC              4       // hud engaging
#define KRKPOW_VOC              5       // powering up
#define KRKKEY_VOC              6       // a key was pressed
#define KRKEX1_VOC              7       // the alien explosion
#define KRKEX2_VOC              8       // the blast of an alien

#define NUM_SOUND_FX            9       // the number of sound fx loaded in

// color fx registers for a multitude of objects

#define SHIELDS_REG             232     // the instrument panel shields color register
#define RADAR_REG               233     // the instrument panel radar color register
#define COMM_REG                234     // the instrument panel communications color register
#define HUD_REG                 235     // the instrument panel hud color register

#define STS_REG                 236     // the instrument panel ship status color register
#define HULL_REG                237     // the instrument panel hull color register
#define OFF_REG                 238     // the instrument panel off color register

#define PLAYERS_WEAPON_FIRE_REG 239     // the little light that glows when the player fires

#define ENGINES_TALLON_REG      240     // the engine flicker for tallons
#define ENGINES_SLIDER_REG      241     // the engine flicker for sliders
#define BARRIERS_REG            242     // the laser barriers
#define SHIELDS_FLICKER_REG     243     // the shields (not implemented)

#define START_PANEL_REG         224     // the start register for the main menu fx
#define END_PANEL_REG           (224 + 7) // the end register for the main menu fx

#define SELECT_REG              254     // the glowing currently selected main menu item

#define ALIEN_EXPL_BASE_REG     244     // the aliens that are killed all
                                        // get allocated a color register at this base

// defines for the gradient sky and ground

#define SKY_COLOR_1             50      // reds
#define SKY_COLOR_2             52
#define SKY_COLOR_3             53

#define GND_COLOR_1             216     // browns
#define GND_COLOR_2             215
#define GND_COLOR_3             214

// music system

#define NUM_INTRO_SEQUENCES     11      // the number of elements in intro music score
#define NUM_GAME_SEQUENCES      18      // the number of elements in game music score

// defines for briefing instructions

#define NUM_PAGES               8
#define NUM_LINES_PAGE          17
#define NUM_SHIP_SPECS          7

// state of the game itself

#define GAME_SETUP              0       // the game is in the setup mode
#define GAME_LINKING            1       // the communications link is being established
#define GAME_RUNNING            2       // the game is running
#define GAME_PAUSED             3       // the game is paused (not implemented)
#define GAME_OVER               4       // what do you think

// general object states

#define DEAD                    0       // these are general states for any
#define ALIVE                   1       // object
#define DYING                   2

// defines for the ship state machine

#define SHIP_STABLE             0       // the ship is stable
#define SHIP_HIT                1       // the ship has been hit
#define SHIP_TELEPORT           2       // the ship is teleporting (not implemented)

#define SHIP_FLAME_COLOR        96      // odd ball, the starting base color register
                                        // that is used to flicker the screen to simulate
                                        // a torpedo blast

// used to indicate which ship type the player is using

#define PLAYER_TALLON           0
#define PLAYER_SLIDER           1

// defines for setup selections

#define SETUP_CHALLENGE         0
#define SETUP_SELECT_MECH       1
#define SETUP_RULES             2
#define SETUP_EXIT              3

#define NUM_SETUP               4       // number of setup choices

// size of the "tech" font used in intro

#define TECH_FONT_WIDTH         4       // width of high tech font
#define TECH_FONT_HEIGHT        7       // height of high tech font
#define NUM_TECH_FONT           64      // number of characters in tech font

// this structure is used to replicate static similar objects based on the
// same model
typedef struct FixedObjType {
    int state;          // state of object
    int rx, ry, rz;     // rotation rate of object
    float x, y, z;      // position of object
} FixedObj, *FixedObjPtr;

// this structure is used for all projectiles in the game, similar
// to the above structure except with added fields for animation, collision, etc.
typedef struct ProjObjType {
    int state;              // state of projectile
    int owner;              // owner of projectile
    int lifetime;           // lifetime of projectile
    Vector3D direction;     // direction and velocity of projectile
    float x, y, z;          // position of projectile
} MissileObj, ProjObj, *MissileObjPtr, *ProjObjPtr;

// this data structure is used to hold an alien attacker
typedef struct AlienType {
    int state;              // state of alien
    int counter1;           // counters
    int counter2;
    int threshold1;         // thresholds for counters
    int threshold2;
    int aux1;               // auxiliary variables for whatever
    int aux2;
    int colorReg;           // color register of alien during explosion
    RgbColor color;         // actual rgb color of explosion
    int type;               // type of alien
    int angularHeading;     // curr angle about y axis, i.e. yaw
    int speed;              // speed of ship
    Vector3D direction;     // the current trajectory vector
    float x, y, z;          // position of alien
} Alien, *AlienPtr;

// prototypes

void selectMech(void);
void miscColorInit(void);
void tallonColorFx(void);
void sliderColorFx(void);
void barrierColorFx(void);
void drawHud(void);
void drawTactical(int command);
void loadTactical(void);
void drawScanner(int command);
void initMissiles(void);
void moveMissiles(void);
void drawMissiles(void);
int startMissile(int owner,
                 Vector3DPtr pos,
                 Vector3DPtr dir,
                 int speed,
                 int lifetime);
void initAliens(void);
void processAliens(void);
void drawAliens(void);
void drawBackground(int mountainPos);
void drawBox(int x1, int y1, int x2, int y2, int color);
void techPrint(int x, int y, char* string, unsigned char FAR* destination);
void fontEngine1(int x, int y,
                 int font, int color,
                 char* string, unsigned char FAR* destination);
void panelFx(int reset);
void introPlanet(void);
void closingScreen(void);
void introWaite(void);
void introKrk(void);
void introControls(void);
void introBriefing(void);
void resetSystem(void);
void musicInit(void);
void musicClose(void);
void digitalFxInit(void);
void digitalFxClose(void);
int digitalFxPlay(int theEffect, int priority);
void parseCommands(int argc, char** argv);
void drawStationaryObjects(void);
void set3DView(void);
void load3DObjects(void);

// globals

Object StaticObj[NUM_STATIONARY];           // there are six basic stationary object types

FixedObj Obstacles1[NUM_OBSTACLES_1];       // general obstacles, rocks etc.
FixedObj Obstacles2[NUM_OBSTACLES_2];       // general obstacles, rocks etc.

FixedObj Barriers[NUM_BARRIERS];            // boundary universe boundaries
FixedObj Towers[NUM_TOWERS];                // the control towers
FixedObj Stations[NUM_STATIONS];            // the power stations
FixedObj Telepods[NUM_TELEPODS];            // the teleporters

Object DynamicObj[NUM_DYNAMIC];             // this array holds the models
                                            // for the dynamic game objects

Alien Aliens[NUM_ALIENS];                   // take a wild guess!

MissileObj Missiles[NUM_MISSILES];          // holds the missiles

int ActivePlayerMissiles = 0;               // how many missiles has player activated

int TotalActiveMissiles = 0;                // total active missiles

Layer Mountains;            // the background mountains

PcxPicture ImagePcx,        // general PCX image used to load background and imagery
           ImageControls;   // this holds the controls screen

Sprite TacticalSpr,         // holds the images for the tactical displays
       ButtonsSpr;          // holds the images for the control buttons

RgbColor Color1, Color2;    // used for temporaries during color rotation

RgbPalette GamePalette;     // this will hold the startup system palette

Bitmap TechFont[NUM_TECH_FONT];     // the tech font bitmaps

int GameState = GAME_SETUP;         // the overall state of the game

int ScannerState  = 0,              // state of scanner
    HudState      = 0,
    TacticalState = TACTICAL_MODE_OFF;

int ShipPitch   = 0,    // current direction of ship
    ShipYaw     = 0,    // not used
    ShipRoll    = 0,    // not used
    ShipSpeed   = 0,    // speed of ship
    ShipEnergy  = 50,   // current energy of ship
    ShipDamage  = 0,    // current damage up to 50 points
    ShipMessage = 0,    // current message to ship state machine
    ShipTimer   = 0,    // a little timer
    ShipKills   = 0,    // how many bad guys has player killed
    ShipDeaths  = 0;    // how many times has player been killed

Vector3D UnitZ          = { 0, 0, 1, 1 },   // a unit vector in the Z direction
         ShipDirection  = { 0, 0, 1, 1 };   // the ships direction

int PlayersShipType = PLAYER_TALLON;    // the player starts off with a tallon

// musical sequence information

int MusicEnabled   = 0,         // flags that enable music and sound FX
    DigitalEnabled = 0;

int DigitalFxPriority = 10;     // the priority tracker of the current effect

int IntroSequence[] = {
    0 + 12, 1 + 12, 2 + 12, 3 + 12, 1 + 12, 2 + 12, 3 + 12, 2 + 12, 3 + 12, 1 + 12,
    2 + 12, 3 + 12, 2 + 12, 2 + 12
};

int IntroSeqIndex = 0;  // starting intro index number of sequence to be played

int GameSequence[] = { 11, 8, 2, 3, 1, 2, 5, 8, 6, 1, 0, 2, 4, 5, 2, 1, 0, 3, 4, 8 };

int GameSeqIndex = 0;   // starting game index number of sequence to be played

Music Song;     // the music structure

// sound fx stuff

Sound DigitalFx[NUM_SOUND_FX];

// basic colors

RgbColor BrightRed   = { 63, 0, 0 },    // bright red
         BrightBlue  = { 0, 0, 63 },    // bright blue
         BrightGreen = { 0, 63, 0 },    // bright green

         MediumRed   = { 48, 0, 0 },    // medium red
         MediumBlue  = { 0, 0, 48 },    // medium blue
         MediumGreen = { 0, 48, 0 },    // medium green

         DarkRed     = { 32, 0, 0 },    // dark red
         DarkBlue    = { 0, 0, 32 },    // dark blue
         DarkGreen   = { 0, 32, 0 },    // dark green

         Black       = { 0, 0, 0 },     // pure black
         White       = { 63, 63, 63 },  // pure white

         ColorA, ColorB, ColorC;        // general color variables

// the instruction pages

char* Instructions[] = {
    "KILL OR BE KILLED                  ",
    "                                   ",
    "INTERGALACTIC BATTLE FEDERATION    ",
    "THESE RULES APPLY TO ALL ENTRIES   ",
    "                                   ",
    "1. YOU MAY FREELY DESTROY ALL AND  ",
    "   ANY ENEMY THAT YOU MAY MEET UP  ",
    "   WITH.                           ",
    "                                   ",
    "2. ANY PARTICIPANT EXHIBITING MERCY",
    "   IN ANY FORM WILL BE SUMMARILY   ",
    "   TERMINATED. YOU MUST KILL!      ",
    "                                   ",
    "3. LEAVING THE GAME GRID WHILE     ",
    "   THERE ARE ANY OPPONENTS ALIVE IS",
    "   FORBIDDEN.                      ",
    "                1                  ",

    "PLAYING THE GAME                   ",
    "                                   ",
    "AT THE MAIN MENU SELECT THE BATTLE ",
    "MECH THAT YOU WISH TO PLAY WITH    ",
    "BY USING THE (SELECT MECH) OPTION  ",
    "ITEM. YOU WILL THEN BE SENT TO     ",
    "THE HOLOGRAPHIC SPECIFICATION AREA.",
    "                                   ",
    "USE THE RIGHT AND LEFT ARROW KEYS  ",
    "TO VIEW DIFFERENT MECHS. WHEN YOU  ",
    "ARE SATIFIED THEN PRESS (ENTER).   ",
    "                                   ",
    "TO EXIT WITHOUT MAKING A SELECTION ",
    "PRESS (ESC).                       ",
    "                                   ",
    "                                   ",
    "                2                  ",

    "AFTER YOU HAVE SELECTED A MECH THEN",
    "SELECT THE (CHALLENGE) ITEM OF THE ",
    "MAIN MENU. YOU WILL ENTER INTO THE ",
    "GAME AREA AT ITS EXACT CENTER.     ",
    "                                   ",
    "THE GAME AREA CONSISTS OF OBSTACLES",
    "SUCH AS ROCKS AND CRYSTAL GROWTHS. ",
    "                                   ",
    "AROUND THE PERIMETER OF THE GAME   ",
    "GRID IS AN LASER BARRIER SYSTEM    ",
    "POWERED BY LARGE BLACK MONOLITHS   ",
    "WITH GREEN BEACONS. STAY AWAY FROM ",
    "THESE IF POSSIBLE. AS NOTED ABOVE  ",
    "YOU WILL BE POSITIONED AT THE EXACT",
    "CENTER OF THE GAME GRID. IN FACT,  ",
    "THE POSITION YOU ARE STARTED AT    ",
    "                3                  ",

    "IS THE LOCATION OF THE MAIN POWER  ",
    "SOURCE FOR THE GAME GRID. IT MAY BE",
    "POSSIBLE TO RE-CHARGE BY DRIVING   ",
    "OVER THIS ROTATING ENERGY SOURCE.  ",
    "                                   ",
    "HOWEVER, THERE ARE FOUR COM TOWERS ",
    "OVERLOOKING THIS CENTRAL POWER     ",
    "STATION AND ENTRIES TRYING TO STEEL",
    "POWER MAY BE DEALT WITH...         ",
    "                                   ",
    "FINALLY TO MAKE THE GAME MORE      ",
    "INTERESTING THERE ARE FOUR TELEPODS",
    "LOCATED AT THE FOUR CORNERS OF THE ",
    "GAME GRID. TO TELEPORT TO ANOTHER  ",
    "POD, SIMPLY NAVIGATE UNDER ONE OF  ",
    "THE PODS.                          ",
    "                4                  ",

    "CONTROLING THE SHIP                ",
    "                                   ",
    "BOTH SHIPS HAVE THE SAME CONTROLS  ",
    "THEREFORE THE FOLLOWING EXPLAINA-  ",
    "TION IS VALID FOR BOTH TALLON AND  ",
    "SLIDER PILOTS.                     ",
    "                                   ",
    "INFORMATION DISPLAYS               ",
    "                                   ",
    "THE SHIPS ARE FITTED WITH THE      ",
    "FOLLOWING DISPLAYS.                ",
    "                                   ",
    "1. SCANNERS                        ",
    "2. HEADS UP DISPLAY (HUD)          ",
    "3. MULTI-FUNCTION DISPLAY (MFD)    ",
    "                                   ",
    "                5                  ",

    "ENGAGING THE DISPLAYS              ",
    "                                   ",
    " SCANNER....S TOGGLES              ",
    " HUD........H TOGGLES              ",
    " MFD........T TO SELECT VARIOUS    ",
    "              DATA OUTPUTS.        ",
    "                                   ",
    "THE SCANNER IS LOCATED ON THE LEFT ",
    "MAIN VIEW SCREEN AND THE MFD IS    ",
    "LOCATED ON THE RIGHT MAIN VIEWING  ",
    "SCREEN.                            ",
    "                                   ",
    "THE SCANNER REPRESENTS YOU AS A    ",
    "BLUE BLIP AND YOU ENEMIES AS RED.  ",
    "OTHER IMPORTANT OBJECTS ARE ALSO   ",
    "SCANNED AND DISPLAYED.             ",
    "                6                  ",

    "THE MFD IS CURRENTLY FIT TO DISPLAY",
    "TWO MAIN DATA SETS. THE FIRST IS   ",
    "THE SHIPS SPEED, DAMAGE AND ENERGY ",
    "IN LINEAR GRAPHS AND THE SECOND IS ",
    "AN OUTSIDE HULL DAMAGE SYSTEM.     ",
    "                                   ",
    "THE HUD IS A STANDARD POSITION,    ",
    "TRAJECTORY AND SPEED INDICATOR.    ",
    "                                   ",
    "MOTION AND WEAPONS CONTROL         ",
    "                                   ",
    "TO ROTATE THE SHIP RIGHT AND LEFT  ",
    "USE THE RIGHT AND LEFT ARROW KEYS. ",
    "                                   ",
    "TO INCREASE FORWARD THRUST USE THE ",
    "UP ARROW AND TO DECREASE THRUST USE",
    "                7                  ",

    "THE DOWN ARROW.                    ",
    "                                   ",
    "TO FIRE THE PULSE CANNON PRESS THE ",
    "SPACE BAR.                         ",
    "                                   ",
    "TO EXIT THE GAME PRESS ESCAPE.     ",
    "                                   ",
    "HAVE FUN AND REMEMBER:             ",
    "                                   ",
    "KILL OR BE KILLED!!!               ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "                                   ",
    "           END OF FILE             ",
};

// specification for the tallon

char* TallonSpecs[] = {
    "MASS:      2567 KS        ",
    "LENGTH:    25.3 M         ",
    "HEAT DIS:  2.3 E.S        ",
    "WEAPONS:   POSITRON PLASMA",
    "SHIELDS:   STANDARD E-MAG ",
    "MAX SPD:   50 M.S         ",
    "ANG VEL:   72 D.S         ",
    "PROP UNIT: FUSION PULSE   "
};

// specifications for the slider

char* SliderSpecs[] = {
    "MASS:      2245 KS        ",
    "LENGTH:    22.3 M         ",
    "HEAT DIS:  3.4 E.S        ",
    "WEAPONS:   POSITRON PLASMA",
    "SHIELDS:   STANDARD E-MAG ",
    "MAX SPD:   45 M.S         ",
    "ANG VEL:   80 D.S         ",
    "PROP UNIT: FUSION PULSE   "
};

void selectMech(void) {
    // this function allows the user to select a ship via the keyboard
    // the function draws the ship in wire frame, displays the specifications
    // of the ship and then allows the user to select or escape

    int done = 0,           // event loop exit flag
        currentShipType,    // current displayed ship
        index;              // looping variable

    // set current ship to the actual one player is using
    currentShipType = PlayersShipType;

    // clear the screen
    fillScreen(0);

    // make sure user isn't hitting the enter key still
    while (KeysActive);

    // enter into main event loop
    while (!done) {
        // clear the double buffer
        fillDoubleBuffer32(0);

        // get user input
        if (KeysActive) {
            // test for various inputs
            if (KeyboardState[MAKE_RIGHT]) {
                // move to next ship

                // click the next button
                if (++currentShipType == 2) {
                    currentShipType = 0;
                }

                // make a key sound
                digitalFxPlay(KRKKEY_VOC, 3);
            } else if (KeyboardState[MAKE_LEFT]) {
                // move to previous ship

                // click the previous button
                if (--currentShipType < 0) {
                    currentShipType = 1;
                }

                // make a key sound
                digitalFxPlay(KRKKEY_VOC, 3);
            } else if (KeyboardState[MAKE_ENTER]) {
                // select the current ship

                // make players ship the selected ship
                PlayersShipType = currentShipType;

                // exit
                done = 1;

                // make a key sound
                digitalFxPlay(KRKKEY_VOC, 3);
            } else if (KeyboardState[MAKE_ESC]) {
                // exit this menu and go back to main menu

                // exit
                done = 1;

                // make a key sound
                digitalFxPlay(KRKKEY_VOC, 3);
            }
        }

        // draw the object and it's stats
        if (currentShipType == PLAYER_TALLON) {
            // change heading of tallon model
            if ((DynamicObj[TALLONS_TEMPLATE].state += 5) > 360) {
                DynamicObj[TALLONS_TEMPLATE].state -= 360;
            }

            // perform the rotation
            rotateObject(&DynamicObj[TALLONS_TEMPLATE], 0, 5, 0);

            // update object template with new heading

            // set world position to a reasonable spot
            DynamicObj[TALLONS_TEMPLATE].worldPos.x = -50;
            DynamicObj[TALLONS_TEMPLATE].worldPos.y = 0;
            DynamicObj[TALLONS_TEMPLATE].worldPos.z = 400;

            // convert object local coordinates to world coordinates
            localToWorldObject(&DynamicObj[TALLONS_TEMPLATE]);

            // draw the object in wireframe
            drawObjectWire(&DynamicObj[TALLONS_TEMPLATE], 12);

            // draw the stats
            fontEngine1(0, 130, 0, 12, "TALLON SPECIFICATIONS", VideoBuffer);

            // draw the specification strings
            for (index = 0; index < NUM_SHIP_SPECS; index++) {
                fontEngine1(4, 140 + 8 * index, 0, 12, TallonSpecs[index], VideoBuffer);
            }
        } else {
            // else must be a slider

            // change heading of slider model
            if ((DynamicObj[SLIDERS_TEMPLATE].state += 5) > 360) {
                DynamicObj[SLIDERS_TEMPLATE].state -= 360;
            }

            // perform the rotation
            rotateObject(&DynamicObj[SLIDERS_TEMPLATE], 0, 5, 0);

            // set world position to a reasonable spot
            DynamicObj[SLIDERS_TEMPLATE].worldPos.x = -50;
            DynamicObj[SLIDERS_TEMPLATE].worldPos.y = 0;
            DynamicObj[SLIDERS_TEMPLATE].worldPos.z = 400;

            // convert object local coordinates to world coordinates
            localToWorldObject(&DynamicObj[SLIDERS_TEMPLATE]);

            // draw the object in wireframe
            drawObjectWire(&DynamicObj[SLIDERS_TEMPLATE], 12);

            // draw the stats
            fontEngine1(0, 130, 0, 12, "SLIDER SPECIFICATIONS", VideoBuffer);

            // draw the specification strings
            for (index = 0; index < NUM_SHIP_SPECS; index++) {
                fontEngine1(4, 140 + 8 * index, 0, 12, SliderSpecs[index], VideoBuffer);
            }
        }

        // draw the buttons
        ButtonsSpr.x         = 260;
        ButtonsSpr.y         = 20;
        ButtonsSpr.currFrame = 4;

        spriteDraw((SpritePtr)&ButtonsSpr, DoubleBuffer, 0);

        ButtonsSpr.y         = 40;
        ButtonsSpr.currFrame = 6;

        spriteDraw((SpritePtr)&ButtonsSpr, DoubleBuffer, 0);

        ButtonsSpr.y         = 60;
        ButtonsSpr.currFrame = 2;

        spriteDraw((SpritePtr)&ButtonsSpr, DoubleBuffer, 0);

        ButtonsSpr.y         = 80;
        ButtonsSpr.currFrame = 0;

        spriteDraw((SpritePtr)&ButtonsSpr, DoubleBuffer, 0);

        // draw the headers
        fontEngine1(60, 0, 0, 12, "KILL OR BE KILLED BATTLE MECH SELECTION", DoubleBuffer);

        // display double buffer
        displayDoubleBuffer32(DoubleBuffer, 0);

        // slow things down a bit
        timeDelay(1);

        // check on music
        if (MusicEnabled) {
            // test if piece is complete or has been stopped
            if (musicStatus() == 2 || musicStatus() == 0) {
                // advance to next sequence
                if (++IntroSeqIndex == 14) {
                    IntroSeqIndex = 0;
                }

                musicPlay((MusicPtr)&Song, IntroSequence[IntroSeqIndex]);
            }
        }
    }
}

void miscColorInit(void) {
    // this function initializes various color registers for the game phase of KRK

    writeColorReg(SHIELDS_REG,             (RgbColorPtr)&DarkBlue);
    writeColorReg(RADAR_REG,               (RgbColorPtr)&DarkBlue);
    writeColorReg(COMM_REG,                (RgbColorPtr)&DarkBlue);
    writeColorReg(HUD_REG,                 (RgbColorPtr)&DarkBlue);
    writeColorReg(STS_REG,                 (RgbColorPtr)&DarkGreen);
    writeColorReg(HULL_REG,                (RgbColorPtr)&DarkGreen);
    writeColorReg(OFF_REG,                 (RgbColorPtr)&BrightGreen);
    writeColorReg(PLAYERS_WEAPON_FIRE_REG, (RgbColorPtr)&Black);
    writeColorReg(ENGINES_TALLON_REG,      (RgbColorPtr)&Black);
    writeColorReg(ENGINES_SLIDER_REG,      (RgbColorPtr)&Black);
    writeColorReg(BARRIERS_REG,            (RgbColorPtr)&Black);
    writeColorReg(SHIELDS_FLICKER_REG,     (RgbColorPtr)&Black);
}

void tallonColorFx(void) {
    // this function flickers the tallons engines

    static int engineCounter = 0;   // used to track time from call to call

    // test if it's time to change color
    if (++engineCounter == 1) {
        writeColorReg(ENGINES_TALLON_REG, (RgbColorPtr)&BrightBlue);
    } else if (engineCounter == 2) {
        writeColorReg(ENGINES_TALLON_REG, (RgbColorPtr)&MediumBlue);
    } else if (engineCounter == 3) {
        engineCounter = 0;
    }
}

void sliderColorFx(void) {
    // this function flickers the sliders engines

    static int engineCounter = 0;   // used to track time from call to call

    // test if it's time to change color
    if (++engineCounter == 1) {
        writeColorReg(ENGINES_SLIDER_REG, (RgbColorPtr)&BrightGreen);
    } else if (engineCounter == 2) {
        writeColorReg(ENGINES_SLIDER_REG, (RgbColorPtr)&MediumGreen);
    } else if (engineCounter == 3) {
        engineCounter = 0;
    }
}

void barrierColorFx(void) {
    // this function flickers the game grid barriers beacon

    static int beaconCounter = 0;   // used to track time from call to call

    // test if it's time to change color
    if (++beaconCounter == 1) {
        writeColorReg(BARRIERS_REG, (RgbColorPtr)&BrightGreen);
    } else if (beaconCounter == 10) {
        writeColorReg(BARRIERS_REG, (RgbColorPtr)&DarkGreen);
    } else if (beaconCounter == 25) {
        beaconCounter = 0;
    }
}

void drawHud(void) {
    // this function draws the heads up display

    char buffer[64];    // local working buffer

    // create the string with the x,y,z and heading in it
    sprintf(buffer, "X(%5d) Y(%5d) Z(%5d) TRAJ(%4d) KILLS:(%3d)",
            (int)ViewPoint.x,
            (int)ViewPoint.y,
            (int)ViewPoint.z,
            (int)ShipYaw,
            (int)ShipKills);

    // print the string to the double buffer
    fontEngine1(16, 10, 0, 12, buffer, DoubleBuffer);
}

void drawTactical(int command) {
    // this function is responsible for both drawing the static tactical displays
    // and updating the currently active tactical display

    int index,      // looping variable
        color,      // holds a temp color
        length;     // used to hold length of indicator bars

    // test command and see what caller wants done
    switch (command) {
        case TACTICAL_CLEAR: {
            // totally clear the tactical display
            drawRectangle(TACTICAL_X,
                          TACTICAL_Y,
                          TACTICAL_X + TACTICAL_WIDTH - 1,
                          TACTICAL_Y + TACTICAL_HEIGHT - 1,
                          0);
        } break;

        case TACTICAL_DRAW: {
            // based on tactical to be displayed render static portion of it

            if (TacticalState == TACTICAL_MODE_STS) {
                TacticalSpr.currFrame = 2;

                // update button illuminations
                writeColorReg(STS_REG,  (RgbColorPtr)&BrightGreen);
                writeColorReg(HULL_REG, (RgbColorPtr)&DarkGreen);
                writeColorReg(OFF_REG,  (RgbColorPtr)&DarkGreen);
            } else if (TacticalState == TACTICAL_MODE_HULL) {
                // which hull should be displayed
                if (PlayersShipType == PLAYER_TALLON) {
                    TacticalSpr.currFrame = 3;
                } else {
                    TacticalSpr.currFrame = 4;
                }

                // update button illuminations
                writeColorReg(STS_REG,  (RgbColorPtr)&DarkGreen);
                writeColorReg(HULL_REG, (RgbColorPtr)&BrightGreen);
                writeColorReg(OFF_REG,  (RgbColorPtr)&DarkGreen);
            } else {
                TacticalSpr.currFrame = 1;

                // update button illuminations
                writeColorReg(STS_REG,  (RgbColorPtr)&DarkGreen);
                writeColorReg(HULL_REG, (RgbColorPtr)&DarkGreen);
                writeColorReg(OFF_REG,  (RgbColorPtr)&BrightGreen);
            }

            // now draw display
            TacticalSpr.x = TACTICAL_X + 8;
            TacticalSpr.y = TACTICAL_Y + 6;

            spriteDraw((SpritePtr)&TacticalSpr, VideoBuffer, 0);
        } break;

        case TACTICAL_UPDATE: {
            // based on tactical state update display accordingly
            switch (TacticalState) {
                case TACTICAL_MODE_STS: {
                    // draw velocity gauge
                    length = (ShipSpeed << 1) / 5;

                    // test for negative values
                    if (length < 0) {
                        // invert value
                        length = -length;

                        // show negative velocity with red
                        color = 32;
                    } else {
                        // show positive velocity with red
                        color = 144;
                    }

                    // do a little out of bounds check
                    if (length > 20) {
                        length = 20;
                    }

                    // draw visible portion of digital indicator
                    if (length > 0) {
                        lineH(TACTICAL_X + 8 + 16,
                              TACTICAL_X + 8 + 16 + length,
                              TACTICAL_Y + 6 + 5,
                              color);

                        lineH(TACTICAL_X + 8 + 16,
                              TACTICAL_X + 8 + 16 + length,
                              TACTICAL_Y + 6 + 5 + 1,
                              color);
                    }

                    // undraw old line (if any)
                    if (++length <= 20) {
                        // erase old line
                        lineH(TACTICAL_X + 8 + 16 + length,
                              TACTICAL_X + 8 + 16 + 20,
                              TACTICAL_Y + 6 + 5,
                              0);

                        lineH(TACTICAL_X + 8 + 16 + length,
                              TACTICAL_X + 8 + 16 + 20,
                              TACTICAL_Y + 6 + 5 + 1,
                              0);
                    }

                    // draw damage gauge
                    length = (ShipDamage << 1) / 5;

                    // check for negative values
                    if (length < 0) {
                        length = -length;
                    }

                    // draw visible portion of digital indicator
                    if (length > 0) {
                        lineH(TACTICAL_X + 8 + 16,
                              TACTICAL_X + 8 + 16 + length,
                              TACTICAL_Y + 6 + 5 + 8,
                              96);

                        lineH(TACTICAL_X + 8 + 16,
                              TACTICAL_X + 8 + 16 + length,
                              TACTICAL_Y + 6 + 5 + 8 + 1,
                              96);
                    }

                    if (++length <= 20) {
                        // erase old line
                        lineH(TACTICAL_X + 8 + 16 + length,
                              TACTICAL_X + 8 + 16 + 20,
                              TACTICAL_Y + 6 + 5 + 8,
                              0);

                        lineH(TACTICAL_X + 8 + 16 + length,
                              TACTICAL_X + 8 + 16 + 20,
                              TACTICAL_Y + 6 + 5 + 8 + 1,
                              0);
                    }

                    // draw energy gauge
                    length = (ShipEnergy << 1) / 5;

                    // check for negative values
                    if (length < 0) {
                        length = -length;
                    }

                    // draw visible portion of digital indicator
                    if (length > 0) {
                        lineH(TACTICAL_X + 8 + 16,
                              TACTICAL_X + 8 + 16 + length,
                              TACTICAL_Y + 6 + 5 + 16,
                              96);

                        lineH(TACTICAL_X + 8 + 16,
                              TACTICAL_X + 8 + 16 + length,
                              TACTICAL_Y + 6 + 5 + 16 + 1,
                              96);
                    }

                    // undraw remainder of gauge
                    if (++length <= 20) {
                        // erase old line
                        lineH(TACTICAL_X + 8 + 16 + length,
                              TACTICAL_X + 8 + 16 + 20,
                              TACTICAL_Y + 6 + 5 + 16,
                              0);

                        lineH(TACTICAL_X + 8 + 16 + length,
                              TACTICAL_X + 8 + 16 + 20,
                              TACTICAL_Y + 6 + 5 + 16 + 1,
                              0);
                    }
                } break;

                case TACTICAL_MODE_HULL: {
                    // do nothing for now
                } break;

                case TACTICAL_MODE_OFF: {
                    // do nothing for now
                } break;

                default: break;
            }
        } break;

        default: break;
    }
}

void loadTactical(void) {
    // this function loads various icons for the tactical displays

    int index;  // looping variable

    // load the imagery for the icons for display
    pcxInit((PcxPicturePtr)&ImagePcx);
    pcxLoad("krkdis.pcx", (PcxPicturePtr)&ImagePcx, 1);

    // initialize the tactical sprite
    spriteInit((SpritePtr)&TacticalSpr, 0, 0, 38, 28, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for heads up text
    for (index = 0; index < 5; index++) {
        pcxGetSprite((PcxPicturePtr)&ImagePcx,
                     (SpritePtr)&TacticalSpr, index, index, 0);
    }

    // delete pcx file
    pcxDelete((PcxPicturePtr)&ImagePcx);

    // load the imagery for the control buttons
    pcxInit((PcxPicturePtr)&ImagePcx);
    pcxLoad("krkbutt.pcx", (PcxPicturePtr)&ImagePcx, 1);

    // initialize the tactical sprite
    spriteInit((SpritePtr)&ButtonsSpr, 0, 0, 42, 12, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for heads up text
    for (index = 0; index < 8; index++) {
        pcxGetSprite((PcxPicturePtr)&ImagePcx,
                     (SpritePtr)&ButtonsSpr, index, index % 4, index / 4);
    }

    // delete pcx file
    pcxDelete((PcxPicturePtr)&ImagePcx);
}

void drawScanner(int command) {
    // this function draws the scanner, it has three modes, clear, erase blips,
    // and draw blips

    int index,  // looping variable
        xb, yb; // temporary blip locations

    // holds the scanner clips
    static int blipX[24],       // these arrays hold the blips from call to call
               blipY[24],
               activeBlips = 0; // total number of active blips this frame

    // what is the command
    if (command == SCANNER_CLEAR) {
        // clear the scanner image
        drawRectangle(SCANNER_X,
                      SCANNER_Y,
                      SCANNER_X + SCANNER_WIDTH - 1,
                      SCANNER_Y + SCANNER_HEIGHT - 1,
                      0);

        // reset number of blips
        activeBlips = 0;

        // exit
        return;
    }

    if (command == SCANNER_LOGO) {
        // clear the scanner surface off
        TacticalSpr.currFrame = 1;
        TacticalSpr.x         = SCANNER_X + 8;
        TacticalSpr.y         = SCANNER_Y + 6;

        spriteDraw((SpritePtr)&TacticalSpr, VideoBuffer, 0);

        // reset number of blips
        activeBlips = 0;

        // exit
        return;
    }

    // now determine if scanner is being drawn or erased
    if (command == SCANNER_ERASE_BLIPS) {
        // erase all the scanner blips

        // simply loop thru blips and erase them
        for (index = 0; index < activeBlips; index++) {
            writePixel(blipX[index], blipY[index], 0);
        }

        return;
    } else {
        // reset number of active blips
        activeBlips = 0;

        // draw all the scanner blips

        // first barriers
        for (index = 0; index < NUM_BARRIERS; index++) {
            // compute blip position
            xb = SCANNER_X + (8000 + Barriers[index].x) / 282;
            yb = SCANNER_Y + (8000 - Barriers[index].z) / 382;

            // draw the blip
            writePixel(xb, yb, 7);

            // save the blip
            blipX[activeBlips] = xb;
            blipY[activeBlips] = yb;

            // increment number of blips
            activeBlips++;
        }

        // now telepods
        for (index = 0; index < NUM_TELEPODS; index++) {
            // compute blip position
            xb = SCANNER_X + (8000 + Telepods[index].x) / 282;
            yb = SCANNER_Y + (8000 - Telepods[index].z) / 382;

            // draw the blip
            writePixel(xb, yb, 5);

            // save the blip
            blipX[activeBlips] = xb;
            blipY[activeBlips] = yb;

            // increment number of blips
            activeBlips++;
        }

        // now power stations
        for (index = 0; index < NUM_STATIONS; index++) {
            // compute blip position
            xb = SCANNER_X + (8000 + Stations[index].x) / 282;
            yb = SCANNER_Y + (8000 - Stations[index].z) / 382;

            // draw the blip
            writePixel(xb, yb, 13);

            // save the blip
            blipX[activeBlips] = xb;
            blipY[activeBlips] = yb;

            // increment number of blips
            activeBlips++;
        }

        // now aliens
        for (index = 0; index < NUM_ALIENS; index++) {
            if (Aliens[index].state != ALIEN_DEAD) {
                // compute blip position
                xb = SCANNER_X + (8000 + Aliens[index].x) / 282;
                yb = SCANNER_Y + (8000 - Aliens[index].z) / 382;

                // draw the blip
                writePixel(xb, yb, 12);

                // save the blip
                blipX[activeBlips] = xb;
                blipY[activeBlips] = yb;

                // increment number of blips
                activeBlips++;
            }
        }

        // finally the player

        // compute blip position
        xb = SCANNER_X + (8000 + ViewPoint.x) / 282;
        yb = SCANNER_Y + (8000 - ViewPoint.z) / 382;

        // draw the blip
        writePixel(xb, yb, 9);

        // save the blip
        blipX[activeBlips] = xb;
        blipY[activeBlips] = yb;

        // increment number of blips
        activeBlips++;
    }
}

void initMissiles(void) {
    // this function resets the missile array and gets them ready for use

    int index;  // looping variable

    // initialize all missiles to a known state
    for (index = 0; index < NUM_MISSILES; index++) {
        Missiles[index].state       = MISSILE_INACTIVE;
        Missiles[index].owner       = NO_OWNER;
        Missiles[index].lifetime    = 0;

        Missiles[index].direction.x = 0;
        Missiles[index].direction.y = 0;
        Missiles[index].direction.z = 0;

        Missiles[index].x           = 0;
        Missiles[index].y           = 0;
        Missiles[index].z           = 0;
    }

    // reset number of active missiles
    ActivePlayerMissiles = 0;
    TotalActiveMissiles  = 0;
}

void moveMissiles(void) {
    // this function moves all the missiles

    int index,          // looping variables
        index2,
        radiusTallon,   // used to hold the radius of the ships
        radiusSlider,
        radiusPlayer,
        dx,             // used during distance calculations
        dz,
        dist,
        min;

    // pre-compute radii of alien ship types
    radiusTallon = (0.75 * DynamicObj[TALLONS_TEMPLATE].radius);
    radiusSlider = (0.75 * DynamicObj[SLIDERS_TEMPLATE].radius);

    // compute radius of players ship
    if (PlayersShipType == PLAYER_TALLON) {
        radiusPlayer = DynamicObj[TALLONS_TEMPLATE].radius;
    } else {
        radiusPlayer = DynamicObj[SLIDERS_TEMPLATE].radius;
    }

    // move all the missiles and test for collisions
    for (index = 0; index < NUM_MISSILES; index++) {
        // for each missile that is active, move it and test it against bounds
        // and lifetime

        if (Missiles[index].state == MISSILE_ACTIVE) {
            // process missile

            // first move missile
            Missiles[index].x += Missiles[index].direction.x;
            Missiles[index].y += Missiles[index].direction.y;
            Missiles[index].z += Missiles[index].direction.z;

            // test for collisions with aliens if player fired this missile
            if (Missiles[index].owner == PLAYER_OWNER) {
                for (index2 = 0; index2 < NUM_ALIENS; index2++) {
                    // test if alien is not dead or dying
                    if (Aliens[index2].state != ALIEN_DEAD &&
                        Aliens[index2].state != ALIEN_DYING) {

                        // test if missiles center is within bounding radius
                        // of alien ship

                        // compute distance based on taylor expansion about 12% error max

                        // first find |dx| and |dz|
                        if ((dx = ((int)Aliens[index2].x - (int)Missiles[index].x)) < 0) {
                            dx = -dx;
                        }

                        if ((dz = ((int)Aliens[index2].z - (int)Missiles[index].z)) < 0) {
                            dz = -dz;
                        }

                        // compute minimum delta
                        if (dx <= dz) {
                            min = dx;
                        } else {
                            min = dz;
                        }

                        // compute distance
                        dist = dx + dz - (min >> 1);

                        // test distance against average radius of alien ship
                        if (Aliens[index2].type == ALIEN_TALLON) {
                            if (dist <= radiusTallon) {
                                // kill missile
                                Missiles[index].lifetime = -1;

                                // send message to aliens that this one is dying
                                Aliens[index2].state      = ALIEN_DYING;
                                Aliens[index2].counter1   = 0;
                                Aliens[index2].threshold1 = 20;

                                Aliens[index2].speed       = 0;

                                Aliens[index2].color.red   = 0;
                                Aliens[index2].color.green = 0;
                                Aliens[index2].color.blue  = 0;

                                // lets hear him fry
                                digitalFxPlay(KRKEX1_VOC, 0);

                                // add one more notch to my 6 shooter!
                                ShipKills++;

                                // move to next missile
                                continue;
                            }
                        } else if (Aliens[index2].type == ALIEN_SLIDER) {
                            if (dist <= radiusSlider) {
                                // kill missile
                                Missiles[index].lifetime = -1;

                                // send message to aliens that this one is dying
                                Aliens[index2].state      = ALIEN_DYING;
                                Aliens[index2].counter1   = 0;
                                Aliens[index2].threshold1 = 20;

                                Aliens[index2].speed       = 0;

                                Aliens[index2].color.red   = 0;
                                Aliens[index2].color.green = 0;
                                Aliens[index2].color.blue  = 0;

                                // lets hear him fry
                                digitalFxPlay(KRKEX1_VOC, 0);

                                // add one more notch to my 6 shooter!
                                ShipKills++;

                                // move to next missile
                                continue;
                            }
                        }
                    }
                }
            } else {
                // this missile must be an aliens, so test it against ship

                if ((dx = ((int)ViewPoint.x - (int)Missiles[index].x)) < 0) {
                    dx = -dx;
                }

                if ((dz = ((int)ViewPoint.z - (int)Missiles[index].z)) < 0) {
                    dz = -dz;
                }

                // compute minimum delta
                if (dx <= dz) {
                    min = dx;
                } else {
                    min = dz;
                }

                // compute distance
                dist = dx + dz - (min >> 1);

                // test for collision
                if (dist <= radiusPlayer) {
                    // send message to player
                    ShipMessage = SHIP_HIT;
                    ShipTimer   = 25;

                    // add some sound
                    digitalFxPlay(KRKEX2_VOC, 0);

                    // add some damage
                    if (++ShipDamage > 50) {
                        ShipDamage = 50;
                    }

                    // kill missile
                    Missiles[index].lifetime = -1;
                }
            }

            // test for out of bounds
            if (Missiles[index].x > GAME_MAX_WORLD_X + 500 ||
                Missiles[index].x < GAME_MIN_WORLD_X - 500 ||

                Missiles[index].z > GAME_MAX_WORLD_Z + 500 ||
                Missiles[index].z < GAME_MIN_WORLD_Z - 500) {

                // de-activate missile
                Missiles[index].state = MISSILE_INACTIVE;

                if (Missiles[index].owner == PLAYER_OWNER) {
                    ActivePlayerMissiles--;
                }

                // decrement total always
                TotalActiveMissiles--;
            } else if ((--Missiles[index].lifetime) < 0) {
                // missile has died out (ran out of energy)
                Missiles[index].state = MISSILE_INACTIVE;

                if (Missiles[index].owner == PLAYER_OWNER) {
                    ActivePlayerMissiles--;
                }

                // decrement total always
                TotalActiveMissiles--;
            }
        }
    }

    // only rotate if there are active missiles
    if (TotalActiveMissiles) {
        rotateObject(&DynamicObj[MISSILES_TEMPLATE], 0, 0, 30);
    }
}

void drawMissiles(void) {
    // this function draws all the missiles (in 3d)

    int index;  // looping variable

    for (index = 0; index < NUM_MISSILES; index++) {
        // test if missile is active before starting 3-D processing
        if (Missiles[index].state == MISSILE_ACTIVE) {
            // test if object is visible

            // now before we continue to process object, we must
            // move it to the proper world position
            DynamicObj[MISSILES_TEMPLATE].worldPos.x = Missiles[index].x;
            DynamicObj[MISSILES_TEMPLATE].worldPos.y = Missiles[index].y;
            DynamicObj[MISSILES_TEMPLATE].worldPos.z = Missiles[index].z;

            if (!removeObject(&DynamicObj[MISSILES_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
                // convert object local coordinates to world coordinate
                localToWorldObject(&DynamicObj[MISSILES_TEMPLATE]);

                // remove the backfaces and shade object
                removeBackfacesAndShade(&DynamicObj[MISSILES_TEMPLATE], -1);

                // convert world coordinates to camera coordinate
                worldToCameraObject(&DynamicObj[MISSILES_TEMPLATE]);

                // clip the objects polygons against viewing volume
                clipObject3D(&DynamicObj[MISSILES_TEMPLATE], CLIP_Z_MODE);

                // generate the final polygon list
                generatePolyList(&DynamicObj[MISSILES_TEMPLATE], ADD_TO_POLY_LIST);
            }
        }
    }
}

int startMissile(int owner,
                 Vector3DPtr pos,
                 Vector3DPtr dir,
                 int speed,
                 int lifetime) {
    // this function starts a missile up by hunting for an unused missile, initializing
    // it and then starting it with the proper parameters

    int index;  // looping variable

    // first test if this is a missile fired by player, if so, test if player
    // is getting greedy
    if (owner == PLAYER_OWNER && ActivePlayerMissiles >= MAX_PLAYER_MISSILES) {
        return 0;
    }

    // hunt for an inactive missile
    for (index = 0; index < NUM_MISSILES; index++) {
        // is this missile free?
        if (Missiles[index].state == MISSILE_INACTIVE) {
            // set this missile up
            Missiles[index].state    = MISSILE_ACTIVE;
            Missiles[index].owner    = owner;
            Missiles[index].lifetime = lifetime;

            Missiles[index].direction.x = speed * dir->x;
            Missiles[index].direction.y = speed * dir->y;
            Missiles[index].direction.z = speed * dir->z;

            // start missile at center of viewport plus one step out
            Missiles[index].x =              pos->x + dir->x;
            Missiles[index].y = GUN_HEIGHT + pos->y + dir->y;
            Missiles[index].z =              pos->z + dir->z;

            // test if player fired the missile and update active missiles
            if (owner == PLAYER_OWNER) {
                // there's now one more missile
                ActivePlayerMissiles++;

                // play sound fx
                digitalFxPlay(KRKMIS_VOC, 2);
            } else {
                // must be alien

                // play sound fx
                digitalFxPlay(KRKEMIS_VOC, 3);
            }

            // increment total active missiles
            TotalActiveMissiles++;

            // exit loop baby!
            return 1;
        }
    }

    // couldn't satisfy request, let caller know
    return 0;
}

void initAliens(void) {
    // this function initializes the alien structures

    int index;  // looping variable

    for (index = 0; index < NUM_ALIENS; index++) {
        Aliens[index].state          = ALIEN_NEW_STATE;
        Aliens[index].counter1       = 0;
        Aliens[index].counter2       = 0;
        Aliens[index].threshold1     = 0;
        Aliens[index].threshold2     = 0;
        Aliens[index].aux1           = 0;
        Aliens[index].aux2           = 0;
        Aliens[index].colorReg       = ALIEN_EXPL_BASE_REG + index;

        Aliens[index].color.red      = 0;
        Aliens[index].color.green    = 0;
        Aliens[index].color.blue     = 0;

        Aliens[index].type           = index % 2;
        Aliens[index].speed          = 0;

        Aliens[index].angularHeading = 0;

        Aliens[index].direction.x    = 0;
        Aliens[index].direction.y    = 0;
        Aliens[index].direction.z    = 0;

        Aliens[index].x              = -2000 + rand() % 4000;
        Aliens[index].y              = 0;
        Aliens[index].z              = -2000 + rand() % 4000;
    }
}

void processAliens(void) {
    // this function performs AI for the aliens and transforms them

    int index,      // looping variable
        whichPod;   // used to select which telepod to start a dead alien at

    float headX,    // used to compute final heading of alien
          headZ,
          targetX,  // used to compute desired target heading for alien
          targetZ,
          normalY,  // the y component of a normal vector
          distance; // distance between alien and player

    Vector3D alienPos,  // temp to hold alien position
             alienDir;  // temp to hold alien direction

    // this holds the aliens personality probability table.
    // here are the meanings of the values and the current distribution

    // 0 attack, 30%
    // 1 random, 20%
    // 2 evade,  10%
    // 3 stop,   10%
    // 4 turn,   20%

    static int alienPersonality[10] = { 0, 1, 4, 0, 1, 4, 3, 0, 2, 3 };

    // first process live aliens
    for (index = 0; index < NUM_ALIENS; index++) {
        // is this alien alive?
        if (Aliens[index].state != ALIEN_DEAD) {
            // based on state of alien, do the right thing
            switch (Aliens[index].state) {
                case ALIEN_DEAD: {
                    // process dead state
                    Aliens[index].state = ALIEN_NEW_STATE;
                } break;

                case ALIEN_DYING: {
                    // continue dying

                    // increase intensity of color
                    if ((Aliens[index].color.green += 4) > 63) {
                        Aliens[index].color.green = 63;
                    }

                    // set color register
                    writeColorReg(Aliens[index].colorReg, (RgbColorPtr)&Aliens[index].color);

                    // is death sequence complete?
                    if (++Aliens[index].counter1 > Aliens[index].threshold1) {
                        // tell state machine to select a new state
                        Aliens[index].state = ALIEN_DEAD;

                        writeColorReg(Aliens[index].colorReg, (RgbColorPtr)&Black);
                    }
                } break;

                case ALIEN_NEW_STATE: {
                    // a new state has been requested, select a new state based
                    // on probability and personality
                    switch (alienPersonality[rand() % 10]) {
                        case 0: {   // attack
                            // the alien is going to attack the player

                            // how long for total state
                            Aliens[index].counter1   = 0;
                            Aliens[index].threshold1 = 50 + rand() % 150;

                            // set time between heading adjustments
                            Aliens[index].counter2   = 0;
                            Aliens[index].threshold2 = 3 + rand() % 5;

                            // set speed
                            Aliens[index].speed = 45 + rand() % 5;

                            // set new state
                            Aliens[index].state = ALIEN_ATTACK;
                        } break;

                        case 1: {   // random
                            // how long for total state
                            Aliens[index].counter1   = 0;
                            Aliens[index].threshold1 = 50 + rand() % 150;

                            // how often for random direction changes
                            Aliens[index].counter2   = 0;
                            Aliens[index].threshold2 = 10 + rand() % 50;

                            // set speed
                            Aliens[index].speed = 40 + rand() % 25;

                            // and set new state
                            Aliens[index].state = ALIEN_RANDOM;
                        } break;

                        case 2: {   // evade
                            // how long for total state
                            Aliens[index].counter1   = 0;
                            Aliens[index].threshold1 = 50 + rand() % 150;

                            // set time between heading adjustments
                            Aliens[index].counter2   = 0;
                            Aliens[index].threshold2 = 3 + rand() % 5;

                            // set speed
                            Aliens[index].speed = 50 + rand() % 5;

                            // set new state
                            Aliens[index].state = ALIEN_EVADE;
                        } break;

                        case 3: {   // stop
                            // set number of frames for alien to stop
                            Aliens[index].counter1   = 0;
                            Aliens[index].threshold1 = 5 + rand() % 10;

                            // set speed to 0
                            Aliens[index].speed = 0;

                            // and set new state
                            Aliens[index].state = ALIEN_STOP;
                        } break;

                        case 4: {   // turn
                            // set amount of time for turn
                            Aliens[index].counter1   = 0;
                            Aliens[index].threshold1 = 20 + rand() % 20;

                            // set angular turning rate
                            Aliens[index].aux1 = -5 + rand() % 11;

                            // set speed for turn
                            Aliens[index].speed = 35 + rand() % 30;

                            // and set new state
                            Aliens[index].state = ALIEN_TURN;
                        } break;

                        default: break;
                    }
                } break;

                case ALIEN_ATTACK: {
                    // continue tracking player

                    // test if it's time to adjust heading to track player
                    if (++Aliens[index].counter2 > Aliens[index].threshold2) {
                        // adjust heading toward player, use a heuristic approach
                        // that simply tries to keep turning the alien toward
                        // the player, later maybe we could add a bit of
                        // trajectory lookahead, so the alien could intercept
                        // the player???

                        // to determine which way the alien needs to turn we
                        // can use the following trick: based on the current
                        // trajectory of the alien and the vector from the
                        // alien to the player, we can compute a normal vector

                        // compute heading vector (happens to be a unit vector)
                        headX = SinLook[Aliens[index].angularHeading];
                        headZ = CosLook[Aliens[index].angularHeading];

                        // compute target trajectory vector, players position
                        // minus aliens position
                        targetX = ViewPoint.x - Aliens[index].x;
                        targetZ = ViewPoint.z - Aliens[index].z;

                        // now compute y component of normal
                        normalY = (headZ * targetX - headX * targetZ);

                        // based on the sign of the result we can determine if
                        // we should turn the alien right or left, but be careful
                        // we are in a LEFT HANDED system!
                        if (normalY >= 0) {
                            Aliens[index].angularHeading += (10 + rand() % 10);
                        } else {
                            Aliens[index].angularHeading -= (10 + rand() % 10);
                        }

                        // check angle for overflow/underflow
                        if (Aliens[index].angularHeading >= 360) {
                            Aliens[index].angularHeading -= 360;
                        } else if (Aliens[index].angularHeading < 0) {
                            Aliens[index].angularHeading += 360;
                        }

                        // reset counter
                        Aliens[index].counter2 = 0;
                    }

                    // test if attacking sequence is complete
                    if (++Aliens[index].counter1 > Aliens[index].threshold1) {
                        // tell state machine to select a new state
                        Aliens[index].state = ALIEN_NEW_STATE;
                    }

                    // try and fire a missile
                    distance = fabs(ViewPoint.x - Aliens[index].x) +
                               fabs(ViewPoint.z - Aliens[index].z);

                    if ((rand() % 15) == 1 && distance < 3500) {
                        // create local vectors

                        // first position
                        alienPos.x = Aliens[index].x;
                        alienPos.y = 45;    // alien y centerline
                        alienPos.z = Aliens[index].z;

                        // now direction
                        alienDir.x = SinLook[Aliens[index].angularHeading];
                        alienDir.y = 0;
                        alienDir.z = CosLook[Aliens[index].angularHeading];

                        // start the missile
                        startMissile(ALIEN_OWNER,
                                     &alienPos,
                                     &alienDir,
                                     Aliens[index].speed + 25,
                                     75);
                    }
                } break;

                case ALIEN_RANDOM: {
                    // continue moving in randomly selected direction

                    // test if it's time to select a new direction
                    if (++Aliens[index].counter2 > Aliens[index].threshold2) {
                        // select a new direction  +- 30 degrees
                        Aliens[index].angularHeading += (-30 + 10 * (rand() % 7));

                        // check angle for overflow/underflow
                        if (Aliens[index].angularHeading >= 360) {
                            Aliens[index].angularHeading -= 360;
                        } else if (Aliens[index].angularHeading < 0) {
                            Aliens[index].angularHeading += 360;
                        }

                        // reset counter
                        Aliens[index].counter2 = 0;
                    }

                    // test if entire random sequence is complete
                    if (++Aliens[index].counter1 > Aliens[index].threshold1) {
                        // tell state machine to select a new state
                        Aliens[index].state = ALIEN_NEW_STATE;
                    }
                } break;

                case ALIEN_EVADE: {
                    // continue evading player

                    // test if it's time to adjust heading to evade player
                    if (++Aliens[index].counter2 > Aliens[index].threshold2) {
                        // adjust heading away from player, use a heuristic approach
                        // that simply tries to keep turning the alien away from
                        // the player

                        // to determine which way the alien needs to turn we
                        // can use the following trick: based on the current
                        // trajectory of the alien and the vector from the
                        // alien to the player, we can compute a normal vector

                        // compute heading vector (happens to be a unit vector)
                        headX = SinLook[Aliens[index].angularHeading];
                        headZ = CosLook[Aliens[index].angularHeading];

                        // compute target trajectory vector, players position
                        // minus aliens position
                        targetX = ViewPoint.x - Aliens[index].x;
                        targetZ = ViewPoint.z - Aliens[index].z;

                        // now compute y component of normal
                        normalY = (headZ * targetX - headX * targetZ);

                        // based on the sign of the result we can determine if
                        // we should turn the alien right or left, but be careful
                        // we are in a LEFT HANDED system!
                        if (normalY >= 0) {
                            Aliens[index].angularHeading -= (10 + rand() % 10);
                        } else {
                            Aliens[index].angularHeading += (10 + rand() % 10);
                        }

                        // check angle for overflow/underflow
                        if (Aliens[index].angularHeading >= 360) {
                            Aliens[index].angularHeading -= 360;
                        } else if (Aliens[index].angularHeading < 0) {
                            Aliens[index].angularHeading += 360;
                        }

                        // reset counter
                        Aliens[index].counter2 = 0;
                    }

                    // test if attacking sequence is complete
                    if (++Aliens[index].counter1 > Aliens[index].threshold1) {
                        // tell state machine to select a new state
                        Aliens[index].state = ALIEN_NEW_STATE;
                    }
                } break;

                case ALIEN_STOP: {
                    // sit still and rotate

                    // test if stopping sequence is complete
                    if (++Aliens[index].counter1 > Aliens[index].threshold1) {
                        // tell state machine to select a new state
                        Aliens[index].state = ALIEN_NEW_STATE;
                    }
                } break;

                case ALIEN_TURN: {
                    // continue turn
                    Aliens[index].angularHeading += Aliens[index].aux1;

                    // check angle for overflow/underflow
                    if (Aliens[index].angularHeading >= 360) {
                        Aliens[index].angularHeading -= 360;
                    } else if (Aliens[index].angularHeading < 0) {
                        Aliens[index].angularHeading += 360;
                    }

                    // test if turning sequence is complete
                    if (++Aliens[index].counter1 > Aliens[index].threshold1) {
                        // tell state machine to select a new state
                        Aliens[index].state = ALIEN_NEW_STATE;
                    }
                } break;

                default: break;
            }

            // now move the alien based on direction
            Aliens[index].x += (Aliens[index].speed * SinLook[Aliens[index].angularHeading]);
            Aliens[index].z += (Aliens[index].speed * CosLook[Aliens[index].angularHeading]);

            // perform bounds checking, if an alien hits an edge warp to other side
            if (Aliens[index].x > (GAME_MAX_WORLD_X + 750)) {
                Aliens[index].x = (GAME_MIN_WORLD_X - 500);
            } else if (Aliens[index].x < (GAME_MIN_WORLD_X - 750)) {
                Aliens[index].x = (GAME_MAX_WORLD_X + 500);
            }

            if (Aliens[index].z > (GAME_MAX_WORLD_Z + 750)) {
                Aliens[index].z = (GAME_MIN_WORLD_Z - 500);
            } else if (Aliens[index].z < (GAME_MIN_WORLD_Z - 750)) {
                Aliens[index].z = (GAME_MAX_WORLD_Z + 500);
            }
        }
    }

    // try and turn on dead guys at teleporters 10% chance
    if ((rand() % 10) == 1) {
        for (index = 0; index < NUM_ALIENS; index++) {
            // test if this one is dead
            if (Aliens[index].state == ALIEN_DEAD) {
                // set state to new state
                Aliens[index].state = ALIEN_NEW_STATE;

                // select telepod position to start at
                whichPod = rand() % NUM_TELEPODS;

                Aliens[index].x = Telepods[whichPod].x;
                Aliens[index].z = Telepods[whichPod].z;

                // that's enough for now!
                break;
            }
        }
    }
}

void drawAliens(void) {
    // this function simply draws the aliens

    int index,      // looping variable
        diffAngle;  // used to track angular difference between virtual object
                    // and real object

    // draw all the aliens (ya all four of them!)
    for (index = 0; index < NUM_ALIENS; index++) {
        // test if missile is alive before starting 3-D processing
        if (Aliens[index].state != ALIEN_DEAD) {
            // which kind of alien are we dealing with?
            if (Aliens[index].type == ALIEN_TALLON) {
                DynamicObj[TALLONS_TEMPLATE].worldPos.x = Aliens[index].x;
                DynamicObj[TALLONS_TEMPLATE].worldPos.y = Aliens[index].y;
                DynamicObj[TALLONS_TEMPLATE].worldPos.z = Aliens[index].z;

                if (!removeObject(&DynamicObj[TALLONS_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
                    // rotate tallon model to proper direction for this copy of it

                    // look in state field of model to determine current angle and then
                    // compare it to the desired angle, compute the difference and
                    // use the result as the rotation angle to rotate the model
                    // into the proper orientation for this copy of it
                    diffAngle = Aliens[index].angularHeading -
                                DynamicObj[TALLONS_TEMPLATE].state;

                    // fix the sign of the angle
                    if (diffAngle < 0) {
                        diffAngle += 360;
                    }

                    // perform the rotation
                    rotateObject(&DynamicObj[TALLONS_TEMPLATE], 0, diffAngle, 0);

                    // update object template with new heading
                    DynamicObj[TALLONS_TEMPLATE].state = Aliens[index].angularHeading;

                    // convert object local coordinates to world coordinate
                    localToWorldObject(&DynamicObj[TALLONS_TEMPLATE]);

                    // remove the backfaces and shade object
                    if (Aliens[index].state == ALIEN_DYING) {
                        removeBackfacesAndShade(&DynamicObj[TALLONS_TEMPLATE], Aliens[index].colorReg);
                    } else {
                        removeBackfacesAndShade(&DynamicObj[TALLONS_TEMPLATE], -1);
                    }

                    // convert world coordinates to camera coordinate
                    worldToCameraObject(&DynamicObj[TALLONS_TEMPLATE]);

                    // clip the objects polygons against viewing volume
                    clipObject3D(&DynamicObj[TALLONS_TEMPLATE], CLIP_Z_MODE);

                    // generate the final polygon list
                    generatePolyList(&DynamicObj[TALLONS_TEMPLATE], ADD_TO_POLY_LIST);
                }
            } else {
                DynamicObj[SLIDERS_TEMPLATE].worldPos.x = Aliens[index].x;
                DynamicObj[SLIDERS_TEMPLATE].worldPos.y = Aliens[index].y;
                DynamicObj[SLIDERS_TEMPLATE].worldPos.z = Aliens[index].z;

                if (!removeObject(&DynamicObj[SLIDERS_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
                    // rotate slider model to proper direction for this copy of it

                    // look in state field of model to determine current angle and then
                    // compare it to the desired angle, compute the difference and
                    // use the result as the rotation angle to rotate the model
                    // into the proper orientation for this copy of it
                    diffAngle = Aliens[index].angularHeading -
                                DynamicObj[SLIDERS_TEMPLATE].state;

                    // fix the sign of the angle
                    if (diffAngle < 0) {
                        diffAngle += 360;
                    }

                    // perform the rotation
                    rotateObject(&DynamicObj[SLIDERS_TEMPLATE], 0, diffAngle, 0);

                    // update object template with new heading
                    DynamicObj[SLIDERS_TEMPLATE].state = Aliens[index].angularHeading;

                    // convert object local coordinates to world coordinate
                    localToWorldObject(&DynamicObj[SLIDERS_TEMPLATE]);

                    // remove the backfaces and shade object
                    if (Aliens[index].state == ALIEN_DYING) {
                        removeBackfacesAndShade(&DynamicObj[SLIDERS_TEMPLATE], Aliens[index].colorReg);
                    } else {
                        removeBackfacesAndShade(&DynamicObj[SLIDERS_TEMPLATE], -1);
                    }

                    // convert world coordinates to camera coordinate
                    worldToCameraObject(&DynamicObj[SLIDERS_TEMPLATE]);

                    // clip the objects polygons against viewing volume
                    clipObject3D(&DynamicObj[SLIDERS_TEMPLATE], CLIP_Z_MODE);

                    // generate the final polygon list
                    generatePolyList(&DynamicObj[SLIDERS_TEMPLATE], ADD_TO_POLY_LIST);
                }
            }
        }
    }
}

void drawBackground(int mountainPos) {
    // this function draws the gradient sky and ground for the 3-D view

    long color; // used to build up a 4 byte color

    // this function draws the background and mountains for the foreground
    // 3-D image

    // the sky has three layers

    // draw layer 1
    color = SKY_COLOR_1;

    color = color | (color << 8);
    color = color | (color << 16);

    fquadset(DoubleBuffer, color, 4 * 320 / 4);

    // draw layer 2
    color = SKY_COLOR_2;

    color = color | (color << 8);
    color = color | (color << 16);

    fquadset(DoubleBuffer + 320 * 4, color, 8 * 320 / 4);

    // draw layer 3
    color = SKY_COLOR_3;

    color = color | (color << 8);
    color = color | (color << 16);

    fquadset(DoubleBuffer + 320 * 12, color, 44 * 320 / 4);

    // now draw the scrolling mountainscape
    layerDraw((LayerPtr)&Mountains, mountainPos, 0,
              DoubleBuffer, MOUNTAIN_Y_POS, MOUNTAIN_HEIGHT, 0);

    // now draw the ground

    // layer 1
    color = GND_COLOR_1;

    color = color | (color << 8);
    color = color | (color << 16);

    fquadset(DoubleBuffer + 320 * 99, color, 3 * 320 / 4);

    // layer 2
    color = GND_COLOR_2;

    color = color | (color << 8);
    color = color | (color << 16);

    fquadset(DoubleBuffer + 320 * 102, color, 6 * 320 / 4);

    color = GND_COLOR_3;

    color = color | (color << 8);
    color = color | (color << 16);

    fquadset(DoubleBuffer + 320 * 108, color, 21 * 320 / 4);
}

void drawBox(int x1, int y1, int x2, int y2, int color) {
    // this function draws a hollow rectangle
    lineH(x1, x2, y1, color);
    lineH(x1, x2, y2, color);
    lineV(y1, y2, x1, color);
    lineV(y1, y2, x2, color);
}

void techPrint(int x, int y, char* string, unsigned char FAR* destination) {
    // this function is used to print text out like a teletypewriter, it looks
    // cool, trust me!

    int length, // length of input string
        index,  // looping variable
        counter;// used to time process

    char buffer[3]; // a little string used to call font engine with

    // compute length of input string
    length = strlen(string);

    // print the string out a character at a time
    for (index = 0; index < length; index++) {
        // the first character is the actual printable character
        buffer[0] = string[index];

        // null terminate
        buffer[1] = 0;

        // print the string
        fontEngine1(x, y, 0, 0, buffer, destination);

        // move to next position
        x += (TECH_FONT_WIDTH + 1);

        // wait a bit  1/70th of a second
        waitForVerticalRetrace();

        // clear the cursor
    }

    // done!
}

void fontEngine1(int x, int y,
                 int font, int color,
                 char* string, unsigned char FAR* destination) {
    // this function prints a string out using one of the graphics fonts that
    // we have drawn, note this first version doesn't use the font field, but
    // we'll throw it in to keep the interface open for a future version

    static int fontLoaded = 0;  // this is used to track the first time the
                                // function is loaded

    int index,      // loop index
        cIndex,     // character index
        length;     // used to compute lengths of strings

    // test if this is the first time this function is called, if so load the font
    if (!fontLoaded) {
        // load the 4x7 tech font
        pcxInit((PcxPicturePtr)&ImagePcx);
        pcxLoad("krkfnt.pcx", (PcxPicturePtr)&ImagePcx, 1);

        // allocate memory for each bitmap and load character
        for (index = 0; index < NUM_TECH_FONT; index++) {
            // allocate memory for character
            bitmapAllocate((BitmapPtr)&TechFont[index],
                           TECH_FONT_WIDTH, TECH_FONT_HEIGHT);

            // set size of character
            TechFont[index].width  = TECH_FONT_WIDTH;
            TechFont[index].height = TECH_FONT_HEIGHT;

            // extract bitmap from PCX buffer
            TechFont[index].x = 1 + (index % 16) * (TECH_FONT_WIDTH + 1);
            TechFont[index].y = 1 + (index / 16) * (TECH_FONT_HEIGHT + 1);

            bitmapGet((BitmapPtr)&TechFont[index],
                      (PcxPicturePtr)&ImagePcx);
        }

        // font is loaded, delete pcx file and set flag
        pcxDelete((PcxPicturePtr)&ImagePcx);

        fontLoaded = 1;
    } else {
        // print the sent string

        // pre-compute length of string
        length = strlen(string);

        // print the string character by character
        for (index = 0; index < length; index++) {
            // extract the character index from the space character
            cIndex = string[index] - ' ';

            // set bitmap position
            TechFont[cIndex].y = y;
            TechFont[cIndex].x = x;

            // display bitmap
            bitmapPut((BitmapPtr)&TechFont[cIndex],
                      (unsigned char FAR*)destination, 0);

            // move to next character position
            x += (TECH_FONT_WIDTH + 1);
        }
    }
}

void panelFx(int reset) {
    // this function performs all of the special effects for the control panel

    int index;  // looping variable

    static int panelCounter = 0;    // used to time the color rotation of the panel

    static int entered = 0;         // flags if function has been entered yet

    // test if this is the first time in function
    if (!entered || reset) {
        // set entrance flag
        entered = 1;

        // set up color registers
        for (index = START_PANEL_REG; index <= END_PANEL_REG; index++) {
            // generate the color
            ColorA.red   = 0;
            ColorA.green = 0;
            ColorA.blue  = 0;

            // write the data
            writeColorReg(index, (RgbColorPtr)&ColorA);
        }

        ColorA.red = 63;

        writeColorReg(START_PANEL_REG,     (RgbColorPtr)&ColorA);

        writeColorReg(START_PANEL_REG + 3, (RgbColorPtr)&ColorA);

        // set up selection indicator color
        ColorA.red   = 30;
        ColorA.green = 0;
        ColorA.blue  = 0;

        writeColorReg(SELECT_REG, (RgbColorPtr)&ColorA);
    }

    // is it time to update colors?
    if (++panelCounter > 2) {
        // reset counter
        panelCounter = 0;

        // do animation to colors
        readColorReg(END_PANEL_REG, (RgbColorPtr)&ColorA);

        for (index = END_PANEL_REG; index > START_PANEL_REG; index--) {
            // read the (i-1)th register
            readColorReg(index - 1, (RgbColorPtr)&ColorB);

            // assign it to the ith
            writeColorReg(index, (RgbColorPtr)&ColorB);
        }

        // place the value of the first color register into the last to
        // complete the rotation
        writeColorReg(START_PANEL_REG, (RgbColorPtr)&ColorA);
    }

    // update selection color
    readColorReg(SELECT_REG, (RgbColorPtr)&ColorA);

    if ((ColorA.red += 5) > 63) {
        ColorA.red = 25;
    }

    writeColorReg(SELECT_REG, (RgbColorPtr)&ColorA);
}

void introPlanet(void) {
    // this function does the introduction to centari alpha 3

    int index;  // looping variable

    // data output fields
    static char* template[] = {
        "PLANET:   ",
        "TYPE:     ",
        "MASS:     ",
        "TEMP:     ",
        "PERIOD:   ",
        "LIFEFORMS:",
        "STATUS:   ",
    };

    // the data for each field
    static char* data[] = {
        " CENTARI ALPHA 3",
        " F-CLASS, AMMONIUM ATMOSPHERE",
        " 20.6 KELA",
        " 150.2 DEG",
        " 3.2 TELGANS",
        " SILICA BASED, PHOSPOROUS METABOLIZERS",
        " BATTLE AREA - RESTRICTED"
    };

    // load in the KRK title screen
    pcxInit((PcxPicturePtr)&ImagePcx);
    pcxLoad("krkredp.pcx", (PcxPicturePtr)&ImagePcx, 1);

    // show the PCX buffer
    pcxShowBuffer((PcxPicturePtr)&ImagePcx);

    // done with data so delete it
    pcxDelete((PcxPicturePtr)&ImagePcx);

    // do special effects
    timeDelay(50);

    // draw out statistics
    for (index = 0; index < 7; index++) {
        // draw header field
        fontEngine1(START_MESS_X, START_MESS_Y + index * 10, 0, 0,
                    template[index],
                    VideoBuffer);

        // draw information for field
        techPrint(START_MESS_X + 80, START_MESS_Y + index * 10, data[index], VideoBuffer);

        timeDelay(20);
    }

    // wait for a sec
    timeDelay(50);
}

void closingScreen(void) {
    // this function prints the credits

    int index;  // looping variable

    static char* extraCredits[] = {
        "EXTRA CREDITS",
        "             ",
        "MUSICAL MASTERY BY",
        "DEAN HUDSON OF",
        "ECLIPSE PRODUCTIONS",
        "                   ",
        "MIDPAK INSTRUMENTATION CONSULTING BY",
        "ROB WALLACE OF",
        "WALLACE MUSIC & SOUND",
        "                     ",
        "TITLE SCREEN BY",
        "RICHARD BENSON"
    };

    // blank the screen
    fillScreen(0);

    // restore palette
    writePalette((RgbPalettePtr)&GamePalette);

    if (MusicEnabled) {
        musicStop();
        musicPlay((MusicPtr)&Song, 11);
    }

    // print out the credits
    for (index = 0; index <= 11; index++) {
        techPrint(160 - (TECH_FONT_WIDTH + 1) * (strlen(extraCredits[index]) / 2),
                  8 + index * (TECH_FONT_HEIGHT + 4),
                  extraCredits[index], VideoBuffer);

        if (KeysActive) {
            return;
        }

        timeDelay(10);
    }

    timeDelay(50);

    // scroll them away
    for (index = 0; index < 135; index++) {
        fquadcpy(VideoBuffer, VideoBuffer + 320, 16000 - 80);

        // test for exit
        if (KeysActive) {
            return;
        }
    }
}

void introWaite(void) {
    // load in the waite group title screen
    pcxInit((PcxPicturePtr)&ImagePcx);
    pcxLoad("waite.pcx", (PcxPicturePtr)&ImagePcx, 1);

    // done with data so delete it
    pcxDelete((PcxPicturePtr)&ImagePcx);

    // show the PCX buffer
    pcxShowBuffer((PcxPicturePtr)&ImagePcx);

    // do special effects

    // wait for a sec
    timeDelay(40);

    screenTransition(SCREEN_WHITENESS);

    // blank the screen
    fillScreen(0);
}

void introKrk(void) {
    // load in the main title screen group title screen
    pcxInit((PcxPicturePtr)&ImagePcx);
    pcxLoad("krkfirst.pcx", (PcxPicturePtr)&ImagePcx, 1);

    // done with data so delete it
    pcxDelete((PcxPicturePtr)&ImagePcx);

    // show the PCX buffer
    pcxShowBuffer((PcxPicturePtr)&ImagePcx);

    // do special effects

    // wait for a sec
    timeDelay(50);

    screenTransition(SCREEN_DARKNESS);

    // blank the screen
    fillScreen(0);
}

void introControls(void) {
    // this function displays the controls screen

    // load in the starblazer controls screen
    pcxInit((PcxPicturePtr)&ImageControls);
    pcxLoad("krkredpc.pcx", (PcxPicturePtr)&ImageControls, 1);

    // copy controls data to video buffer
    pcxShowBuffer((PcxPicturePtr)&ImageControls);

    // delete pcx file
    pcxDelete((PcxPicturePtr)&ImageControls);
}

void introBriefing(void) {
    // this function displays the briefing screen

    int done = 0,   // exit flag
        page = 0,   // current page user is reading
        index;      // looping variable

    // load in the starblazer controls screen
    pcxInit((PcxPicturePtr)&ImageControls);
    pcxLoad("krkins.pcx", (PcxPicturePtr)&ImageControls, 0);

    // copy controls data to video buffer
    pcxShowBuffer((PcxPicturePtr)&ImageControls);

    // delete pcx file
    pcxDelete((PcxPicturePtr)&ImageControls);

    // display the first page
    for (index = 0; index < NUM_LINES_PAGE; index++) {
        fontEngine1(78, 24 + index * 8, 0, 0, Instructions[index + page * 17], VideoBuffer);
    }

    // enter main event loop
    while (!done) {
        // has the user pressed a key
        if (KeysActive > 0) {
            if (KeyboardState[MAKE_UP]) {
                // page up
                if (--page < 0) {
                    page = 0;
                }

                // press button
                digitalFxPlay(KRKKEY_VOC, 3);

                timeDelay(2);
            }

            if (KeyboardState[MAKE_DOWN]) {
                // page down
                if (++page >= NUM_PAGES) {
                    page = NUM_PAGES - 1;
                }

                // press button
                digitalFxPlay(KRKKEY_VOC, 3);

                timeDelay(2);
            }

            if (KeyboardState[MAKE_ESC]) {
                digitalFxPlay(KRKKEY_VOC, 3);

                done = 1;
            }

            // refresh display
            for (index = 0; index < NUM_LINES_PAGE; index++) {
                fontEngine1(78, 24 + index * 8, 0, 0, Instructions[index + page * 17], VideoBuffer);
            }
        }

        // wait a sec
        timeDelay(1);

        // check on music
        if (MusicEnabled) {
            // test if piece is complete or has been stopped
            if (musicStatus() == 2 || musicStatus() == 0) {
                // advance to next sequence
                if (++IntroSeqIndex == 14) {
                    IntroSeqIndex = 0;
                }

                musicPlay((MusicPtr)&Song, IntroSequence[IntroSeqIndex]);
            }
        }
    }
}

void resetSystem(void) {
    // this function resets everything so the game can be ran again
    // I hope I didn't leave anything out?

    ScannerState  = 0;
    HudState      = 0;
    TacticalState = TACTICAL_MODE_OFF;

    ShipPitch   = 0;
    ShipYaw     = 0;
    ShipRoll    = 0;
    ShipSpeed   = 0;
    ShipEnergy  = 50;
    ShipDamage  = 0;
    ShipMessage = SHIP_STABLE;
    ShipTimer   = 0;
    ShipKills   = 0;
    ShipDeaths  = 0;

    // reset back to power station
    ViewPoint.x = 0;
    ViewPoint.z = 0;
}

void musicInit(void) {
    // this function loads the music and resets all the indexes

    static int loaded = 0;

    // has the music already been loaded
    if (!MusicEnabled) {
        return;
    }

    if (!loaded) {
        musicLoad("krkmus.xmi", (MusicPtr)&Song);
        loaded = 1;
    }

    // reset sequence counters
    GameSeqIndex  = 0;
    IntroSeqIndex = 0;
}

void musicClose(void) {
    // this function unloads the music files

    if (!MusicEnabled) {
        return;
    }

    // turn off music and unload song
    musicStop();
    musicUnload((MusicPtr)&Song);
}

void digitalFxInit(void) {
    // this function initializes the digital sound fx system

    static int loaded = 0;

    if (!DigitalEnabled) {
        return;
    }

    // have the sound fx been loaded?
    if (!loaded) {
        // load in sounds
        soundLoad("KRKMIS.VOC",   (SoundPtr)&DigitalFx[KRKMIS_VOC],  1);
        soundLoad("KRKEMIS2.VOC", (SoundPtr)&DigitalFx[KRKEMIS_VOC], 1);
        soundLoad("KRKTAC.VOC",   (SoundPtr)&DigitalFx[KRKTAC_VOC],  1);
        soundLoad("KRKSCN.VOC",   (SoundPtr)&DigitalFx[KRKSCN_VOC],  1);
        soundLoad("KRKHUD.VOC",   (SoundPtr)&DigitalFx[KRKHUD_VOC],  1);

        soundLoad("KRKKEY.VOC",   (SoundPtr)&DigitalFx[KRKKEY_VOC],  1);
        soundLoad("KRKEX1.VOC",   (SoundPtr)&DigitalFx[KRKEX1_VOC],  1);
        soundLoad("KRKEX2.VOC",   (SoundPtr)&DigitalFx[KRKEX2_VOC],  1);

        // set loaded flag
        loaded = 1;
    }
}

void digitalFxClose(void) {
    // this function unloads all the digital FX

    if (!DigitalEnabled) {
        return;
    }

    // unload all the sound fx from memory
    soundUnload((SoundPtr)&DigitalFx[KRKMIS_VOC]);
    soundUnload((SoundPtr)&DigitalFx[KRKEMIS_VOC]);
    soundUnload((SoundPtr)&DigitalFx[KRKTAC_VOC]);
    soundUnload((SoundPtr)&DigitalFx[KRKSCN_VOC]);
    soundUnload((SoundPtr)&DigitalFx[KRKHUD_VOC]);

    soundUnload((SoundPtr)&DigitalFx[KRKPOW_VOC]);
    soundUnload((SoundPtr)&DigitalFx[KRKKEY_VOC]);
    soundUnload((SoundPtr)&DigitalFx[KRKEX1_VOC]);
    soundUnload((SoundPtr)&DigitalFx[KRKEX2_VOC]);
}

int digitalFxPlay(int theEffect, int priority) {
    // this function is used to play a digital effect using a pre-emptive priority
    // scheme. The algorithm works like this: if a sound is playing then its
    // priority is compared to the sound that is being requested to be played
    // if the new sound has higher priority (a smaller number) then the currently
    // playing sound is pre-empted for the new sound and the global FX priority
    // is set to the new sound. If there is no sound playing then the new sound
    // is simply played and the global priority is set

    // is the digital fx system on-line?
    if (!DigitalEnabled) {
        return 0;
    }

    // is there a sound playing?
    if (!soundStatus() || (priority <= DigitalFxPriority)) {
        // start new sound
        soundStop();

        soundPlay((SoundPtr)&DigitalFx[theEffect]);

        // set the priority
        DigitalFxPriority = priority;

        return 1;
    } else {
        // the current sound is of higher priority
        return 0;
    }
}

void parseCommands(int argc, char** argv) {
    // this function is used to parse the command line parameters that are to be
    // used as switches to enable different modes of operation

    int index;  // looping variable

    for (index = 1; index < argc; index++) {
        // get the first character from the string
        switch (argv[index][0]) {
            case 's': // enable sound effects
            case 'S': {
                DigitalEnabled = 1;
            } break;

            case 'm': // enable music
            case 'M': {
                MusicEnabled = 1;
            } break;

            // more commands would go here...

            default: break;
        }
    }
}

void drawStationaryObjects(void) {
    // this function draws all the stationary non active objects

    int index;  // looping index

    // phase 0: obstacle type one
    for (index = 0; index < NUM_OBSTACLES_1; index++) {
        // test if object is visible

        // now before we continue to process object, we must
        // move it to the proper world position
        StaticObj[OBSTACLES_1_TEMPLATE].worldPos.x = Obstacles1[index].x;
        StaticObj[OBSTACLES_1_TEMPLATE].worldPos.y = Obstacles1[index].y;
        StaticObj[OBSTACLES_1_TEMPLATE].worldPos.z = Obstacles1[index].z;

        if (!removeObject(&StaticObj[OBSTACLES_1_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
            // convert object local coordinates to world coordinate
            localToWorldObject(&StaticObj[OBSTACLES_1_TEMPLATE]);

            // remove the backfaces and shade object
            removeBackfacesAndShade(&StaticObj[OBSTACLES_1_TEMPLATE], -1);

            // convert world coordinates to camera coordinate
            worldToCameraObject(&StaticObj[OBSTACLES_1_TEMPLATE]);

            // clip the objects polygons against viewing volume
            clipObject3D(&StaticObj[OBSTACLES_1_TEMPLATE], CLIP_Z_MODE);

            // generate the final polygon list
            generatePolyList(&StaticObj[OBSTACLES_1_TEMPLATE], ADD_TO_POLY_LIST);
        }
    }

    // phase 1: obstacle type two
    for (index = 0; index < NUM_OBSTACLES_2; index++) {
        // test if object is visible

        // now before we continue to process object, we must
        // move it to the proper world position
        StaticObj[OBSTACLES_2_TEMPLATE].worldPos.x = Obstacles2[index].x;
        StaticObj[OBSTACLES_2_TEMPLATE].worldPos.y = Obstacles2[index].y;
        StaticObj[OBSTACLES_2_TEMPLATE].worldPos.z = Obstacles2[index].z;

        if (!removeObject(&StaticObj[OBSTACLES_2_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
            // convert object local coordinates to world coordinate
            localToWorldObject(&StaticObj[OBSTACLES_2_TEMPLATE]);

            // remove the backfaces and shade object
            removeBackfacesAndShade(&StaticObj[OBSTACLES_2_TEMPLATE], -1);

            // convert world coordinates to camera coordinate
            worldToCameraObject(&StaticObj[OBSTACLES_2_TEMPLATE]);

            // clip the objects polygons against viewing volume
            clipObject3D(&StaticObj[OBSTACLES_2_TEMPLATE], CLIP_Z_MODE);

            // generate the final polygon list
            generatePolyList(&StaticObj[OBSTACLES_2_TEMPLATE], ADD_TO_POLY_LIST);
        }
    }

    // phase 2: the towers
    for (index = 0; index < NUM_TOWERS; index++) {
        // test if object is visible

        // now before we continue to process object, we must
        // move it to the proper world position
        StaticObj[TOWERS_TEMPLATE].worldPos.x = Towers[index].x;
        StaticObj[TOWERS_TEMPLATE].worldPos.y = Towers[index].y;
        StaticObj[TOWERS_TEMPLATE].worldPos.z = Towers[index].z;

        if (!removeObject(&StaticObj[TOWERS_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
            // convert object local coordinates to world coordinate
            localToWorldObject(&StaticObj[TOWERS_TEMPLATE]);

            // remove the backfaces and shade object
            removeBackfacesAndShade(&StaticObj[TOWERS_TEMPLATE], -1);

            // convert world coordinates to camera coordinate
            worldToCameraObject(&StaticObj[TOWERS_TEMPLATE]);

            // clip the objects polygons against viewing volume
            clipObject3D(&StaticObj[TOWERS_TEMPLATE], CLIP_Z_MODE);

            // generate the final polygon list
            generatePolyList(&StaticObj[TOWERS_TEMPLATE], ADD_TO_POLY_LIST);
        }
    }

    // phase 3: the barriers
    for (index = 0; index < NUM_BARRIERS; index++) {
        // test if object is visible

        // now before we continue to process object, we must
        // move it to the proper world position
        StaticObj[BARRIERS_TEMPLATE].worldPos.x = Barriers[index].x;
        StaticObj[BARRIERS_TEMPLATE].worldPos.y = Barriers[index].y;
        StaticObj[BARRIERS_TEMPLATE].worldPos.z = Barriers[index].z;

        if (!removeObject(&StaticObj[BARRIERS_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
            // convert object local coordinates to world coordinate
            localToWorldObject(&StaticObj[BARRIERS_TEMPLATE]);

            // remove the backfaces and shade object
            removeBackfacesAndShade(&StaticObj[BARRIERS_TEMPLATE], -1);

            // convert world coordinates to camera coordinate
            worldToCameraObject(&StaticObj[BARRIERS_TEMPLATE]);

            // clip the objects polygons against viewing volume
            clipObject3D(&StaticObj[BARRIERS_TEMPLATE], CLIP_Z_MODE);

            // generate the final polygon list
            generatePolyList(&StaticObj[BARRIERS_TEMPLATE], ADD_TO_POLY_LIST);
        }
    }

    // phase 4: the main power station
    for (index = 0; index < NUM_STATIONS; index++) {
        // test if object is visible

        // now before we continue to process object, we must
        // move it to the proper world position
        StaticObj[STATIONS_TEMPLATE].worldPos.x = Stations[index].x;
        StaticObj[STATIONS_TEMPLATE].worldPos.y = Stations[index].y;
        StaticObj[STATIONS_TEMPLATE].worldPos.z = Stations[index].z;

        if (!removeObject(&StaticObj[STATIONS_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
            // convert object local coordinates to world coordinate
            localToWorldObject(&StaticObj[STATIONS_TEMPLATE]);

            // remove the backfaces and shade object
            removeBackfacesAndShade(&StaticObj[STATIONS_TEMPLATE], -1);

            // convert world coordinates to camera coordinate
            worldToCameraObject(&StaticObj[STATIONS_TEMPLATE]);

            // clip the objects polygons against viewing volume
            clipObject3D(&StaticObj[STATIONS_TEMPLATE], CLIP_Z_MODE);

            // generate the final polygon list
            generatePolyList(&StaticObj[STATIONS_TEMPLATE], ADD_TO_POLY_LIST);
        }
    }

    // phase 5: the telepods
    for (index = 0; index < NUM_TELEPODS; index++) {
        // test if object is visible

        // now before we continue to process object, we must
        // move it to the proper world position
        StaticObj[TELEPODS_TEMPLATE].worldPos.x = Telepods[index].x;
        StaticObj[TELEPODS_TEMPLATE].worldPos.y = Telepods[index].y;
        StaticObj[TELEPODS_TEMPLATE].worldPos.z = Telepods[index].z;

        if (!removeObject(&StaticObj[TELEPODS_TEMPLATE], OBJECT_CULL_XYZ_MODE)) {
            // convert object local coordinates to world coordinate
            localToWorldObject(&StaticObj[TELEPODS_TEMPLATE]);

            // remove the backfaces and shade object
            removeBackfacesAndShade(&StaticObj[TELEPODS_TEMPLATE], -1);

            // convert world coordinates to camera coordinate
            worldToCameraObject(&StaticObj[TELEPODS_TEMPLATE]);

            // clip the objects polygons against viewing volume
            clipObject3D(&StaticObj[TELEPODS_TEMPLATE], CLIP_Z_MODE);

            // generate the final polygon list
            generatePolyList(&StaticObj[TELEPODS_TEMPLATE], ADD_TO_POLY_LIST);
        }
    }
}

void set3DView(void) {
    // this function sets up the 3d viewing system for the game

    // set 2-D clipping region to take into consideration the instrument panels
    PolyClipMinY = 0;
    PolyClipMaxY = 128;

    // set up viewing and 3D clipping parameters
    ClipNearZ       = 125,
    ClipFarZ        = 6000,
    ViewingDistance = 250;

    // turn the damn light up a bit!
    AmbientLight = 8;

    LightSource.x =  0.918926;
    LightSource.y =  0.248436;
    LightSource.z = -0.306359;

    ViewPoint.x = 0;
    ViewPoint.y = 40;
    ViewPoint.z = 0;
}

void load3DObjects(void) {
    // this function loads the 3-D models

    int index;  // looping variable

    // load in dynamic game objects

    // load in missile template
    plgLoadObject(&DynamicObj[MISSILES_TEMPLATE], "missile.plg", 2);

    // first fix template object at (0,0,0)
    DynamicObj[MISSILES_TEMPLATE].worldPos.x = 0;
    DynamicObj[MISSILES_TEMPLATE].worldPos.y = 0;
    DynamicObj[MISSILES_TEMPLATE].worldPos.z = 0;

    // load in tallon alien template
    plgLoadObject(&DynamicObj[TALLONS_TEMPLATE], "tallon.plg", 1);

    // first fix template object at (0,0,0)
    DynamicObj[TALLONS_TEMPLATE].worldPos.x = 0;
    DynamicObj[TALLONS_TEMPLATE].worldPos.y = 0;
    DynamicObj[TALLONS_TEMPLATE].worldPos.z = 0;

    // the "state" field is going to track the current angle of the object
    DynamicObj[TALLONS_TEMPLATE].state = 0;     // pointing directly down the positive
                                                // Z-axis, this is the neutral position

    // load in slider alien template
    plgLoadObject(&DynamicObj[SLIDERS_TEMPLATE], "slider.plg", 1);

    // first fix template object at (0,0,0)
    DynamicObj[SLIDERS_TEMPLATE].worldPos.x = 0;
    DynamicObj[SLIDERS_TEMPLATE].worldPos.y = 0;
    DynamicObj[SLIDERS_TEMPLATE].worldPos.z = 0;

    // the "state" field is going to track the current angle of the object
    DynamicObj[SLIDERS_TEMPLATE].state = 0;     // pointing directly down the positive
                                                // Z-axis, this is the neutral position

    // load in static game objects, background, obstacles, etc.

    // load in obstacle one template
    plgLoadObject(&StaticObj[OBSTACLES_1_TEMPLATE], "pylons.plg", 1);

    // first fix template object at (0,0,0)
    StaticObj[OBSTACLES_1_TEMPLATE].worldPos.x = 0;
    StaticObj[OBSTACLES_1_TEMPLATE].worldPos.y = 0;
    StaticObj[OBSTACLES_1_TEMPLATE].worldPos.z = 0;

    // now position all obstacle copies
    for (index = 0; index < NUM_OBSTACLES_1; index++) {
        Obstacles1[index].state = 1;
        Obstacles1[index].rx    = 0;
        Obstacles1[index].ry    = 0;
        Obstacles1[index].rz    = 0;
        Obstacles1[index].x     = -8000 + rand() % 16000;
        Obstacles1[index].y     = 0;
        Obstacles1[index].z     = -8000 + rand() % 16000;
    }

    // load in obstacle two template
    plgLoadObject(&StaticObj[OBSTACLES_2_TEMPLATE], "rock.plg", 1);

    // first fix template object at (0,0,0)
    StaticObj[OBSTACLES_2_TEMPLATE].worldPos.x = 0;
    StaticObj[OBSTACLES_2_TEMPLATE].worldPos.y = 0;
    StaticObj[OBSTACLES_2_TEMPLATE].worldPos.z = 0;

    // now position all obstacle copies
    for (index = 0; index < NUM_OBSTACLES_2; index++) {
        Obstacles2[index].state = 1;
        Obstacles2[index].rx    = 0;
        Obstacles2[index].ry    = 0;
        Obstacles2[index].rz    = 0;
        Obstacles2[index].x     = -8000 + rand() % 16000;
        Obstacles2[index].y     = 0;
        Obstacles2[index].z     = -8000 + rand() % 16000;
    }

    // load in tower template
    plgLoadObject(&StaticObj[TOWERS_TEMPLATE], "tower.plg", 2);

    // first fix template object at (0,0,0)
    StaticObj[TOWERS_TEMPLATE].worldPos.x = 0;
    StaticObj[TOWERS_TEMPLATE].worldPos.y = 0;
    StaticObj[TOWERS_TEMPLATE].worldPos.z = 0;

    // now position all tower copies
    for (index = 0; index < NUM_TOWERS; index++) {
        Towers[index].state = 1;
        Towers[index].rx    = 0;
        Towers[index].ry    = 0;
        Towers[index].rz    = 0;
        Towers[index].x     = 0;
        Towers[index].y     = 0;
        Towers[index].z     = 0;
    }

    // position the towers
    Towers[0].x = -1500;
    Towers[0].z = 1500;

    Towers[1].x = -1500;
    Towers[1].z = -1500;

    Towers[2].x = 1500;
    Towers[2].z = -1500;

    Towers[3].x = 1500;
    Towers[3].z = 1500;

    // load in telepod template
    plgLoadObject(&StaticObj[TELEPODS_TEMPLATE], "tele.plg", 2);

    // first fix template object at (0,0,0)
    StaticObj[TELEPODS_TEMPLATE].worldPos.x = 0;
    StaticObj[TELEPODS_TEMPLATE].worldPos.y = 0;
    StaticObj[TELEPODS_TEMPLATE].worldPos.z = 0;

    // now position all tower copies
    for (index = 0; index < NUM_TELEPODS; index++) {
        Telepods[index].state = 1;
        Telepods[index].rx    = 0;
        Telepods[index].ry    = 0;
        Telepods[index].rz    = 0;
        Telepods[index].x     = 0;
        Telepods[index].y     = 0;
        Telepods[index].z     = 0;
    }

    // position the telepods
    Telepods[0].x = -6000;
    Telepods[0].z = 6000;

    Telepods[1].x = -6000;
    Telepods[1].z = -6000;

    Telepods[2].x = 6000;
    Telepods[2].z = -6000;

    Telepods[3].x = 6000;
    Telepods[3].z = 6000;

    // load in universe boundary template
    plgLoadObject(&StaticObj[BARRIERS_TEMPLATE], "barrier.plg", 2);

    // first fix template object at (0,0,0)
    StaticObj[BARRIERS_TEMPLATE].worldPos.x = 0;
    StaticObj[BARRIERS_TEMPLATE].worldPos.y = 0;
    StaticObj[BARRIERS_TEMPLATE].worldPos.z = 0;

    // now position all barrier copies
    for (index = 0; index < NUM_BARRIERS; index++) {
        Barriers[index].state = 1;
        Barriers[index].rx    = 0;
        Barriers[index].ry    = 15;
        Barriers[index].rz    = 0;
        Barriers[index].x     = 0;
        Barriers[index].y     = 0;
        Barriers[index].z     = 0;
    }

    // position the barriers
    Barriers[0].x = -8000;
    Barriers[0].z = 8000;

    Barriers[1].x = -8000;
    Barriers[1].z = 0;

    Barriers[2].x = -8000;
    Barriers[2].z = -8000;

    Barriers[3].x = 0;
    Barriers[3].z = -8000;

    Barriers[4].x = 8000;
    Barriers[4].z = -8000;

    Barriers[5].x = 8000;
    Barriers[5].z = 0;

    Barriers[6].x = 8000;
    Barriers[6].z = 8000;

    Barriers[7].x = 0;
    Barriers[7].z = 8000;

    // load in power station template
    plgLoadObject(&StaticObj[STATIONS_TEMPLATE], "station.plg", 2);

    // first fix template object at (0,0,0)
    StaticObj[STATIONS_TEMPLATE].worldPos.x = 0;
    StaticObj[STATIONS_TEMPLATE].worldPos.y = 0;
    StaticObj[STATIONS_TEMPLATE].worldPos.z = 0;

    // now position all power stations
    for (index = 0; index < NUM_STATIONS; index++) {
        Stations[index].state = 1;
        Stations[index].rx    = 0;
        Stations[index].ry    = 0;
        Stations[index].rz    = 0;
        Stations[index].x     = 0;
        Stations[index].y     = 0;
        Stations[index].z     = 0;
    }
}

void main(int argc, char** argv) {
    // the main controls most of the player and remote logic, normally we would
    // probably move most of the code into functions, but for instructional purposes
    // this is easier to follow, believe me there are already enough function calls
    // to make your head spin!

    int index,                  // looping variable
        sel,                    // used for input
        playersKeyState,        // state of players input
        currentSel       = 0,   // currently highlighted interface selection
        weaponsLitCount  = 0;   // times how long the light flash from a missile
                                // should be displayed when player fires

    unsigned char seed;         // a random number seed

    char buffer[64],            // general buffer
         number[32],            // used to print strings
         ch;                    // used for keyboard input

    // initialization section

    // set up viewing system
    set3DView();

    // build all look up tables
    buildLookUpTables();

    // load in all 3d models
    load3DObjects();

    // parse the command line and set up configuration
    parseCommands(argc, argv);

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // start up music system
    if (MusicEnabled) {
        musicInit();
        musicPlay((MusicPtr)&Song, 16);
    }

    // introduction section

    // put up Waite header
    introWaite();

    // put up my header
    introKrk();

    // seed the random number generator with time
    srand((unsigned int)timerQuery());

    // initialize font engine
    fontEngine1(0, 0, 0, 0, NULL, NULL);

    techPrint(START_MESS_X, START_MESS_Y, " KILL OR BE KILLED 1.0 STARTING UP...", VideoBuffer);
    timeDelay(5);

    techPrint(START_MESS_X, START_MESS_Y + 16, " LANGUAGE TRANSLATION ENABLED", VideoBuffer);

    // create the double buffer
    createDoubleBuffer(129);
    techPrint(START_MESS_X, START_MESS_Y + 26, " DOUBLE BUFFER CREATED", VideoBuffer);

    // install the keyboard driver
    keyboardInstallDriver();
    techPrint(START_MESS_X, START_MESS_Y + 36, " NEURAL INTERFACE ACTIVATED", VideoBuffer);

    // load and create background layer
    techPrint(START_MESS_X, START_MESS_Y + 46, " BACKGROUND ENABLED", VideoBuffer);

    pcxInit((PcxPicturePtr)&ImagePcx);
    pcxLoad("krkbak.pcx", (PcxPicturePtr)&ImagePcx, 1);
    pcxCopyToBuffer((PcxPicturePtr)&ImagePcx, DoubleBuffer);
    pcxDelete((PcxPicturePtr)&ImagePcx);

    layerCreate((LayerPtr)&Mountains, MOUNTAIN_WIDTH, MOUNTAIN_HEIGHT);
    layerBuild((LayerPtr)&Mountains, 0, 0,
               DoubleBuffer,
               0, 36, SCREEN_WIDTH, 43);

    loadTactical();
    techPrint(START_MESS_X, START_MESS_Y + 56, " TACTICAL ONLINE", VideoBuffer);

    // memory problem
    if (DigitalEnabled) {
        soundLoad("KRKPOW.VOC", (SoundPtr)&DigitalFx[KRKPOW_VOC], 1);
        digitalFxPlay(KRKPOW_VOC, 3);
        soundUnload((SoundPtr)&DigitalFx[KRKPOW_VOC]);
    }

    // end memory problem

    // all systems powered
    techPrint(START_MESS_X, START_MESS_Y + 108, " ALL SYSTEMS POWERED AND AVAILABLE", VideoBuffer);

    for (index = 0; index < 3; index++) {
        // draw the message and then erase the message
        fontEngine1(START_MESS_X, START_MESS_Y + 108, 0, 0, " ALL SYSTEMS POWERED AND AVAILABLE", VideoBuffer);
        timeDelay(8);

        fontEngine1(START_MESS_X, START_MESS_Y + 108, 0, 0, "                                  ", VideoBuffer);
        timeDelay(8);
    }

    // start up digital FX system
    if (DigitalEnabled) {
        digitalFxInit();
    }

    // do intro piece
    introPlanet();

    // save the system palette here because we are going to really thrash it!!!
    readPalette(0, 255, (RgbPalettePtr)&GamePalette);

    // main event loop
    while (GameState != GAME_OVER) {
        // test the overall game state

        // setup section
        if (GameState == GAME_SETUP) {
            // user is in the setup state
            introControls();

            drawBox(SELECT_BOX_SX,
                    SELECT_BOX_SY + currentSel * SELECT_BOX_DY,
                    SELECT_BOX_SX + SELECT_BOX_WIDTH,
                    SELECT_BOX_SY + SELECT_BOX_HEIGHT + currentSel * SELECT_BOX_DY,
                    254);

            drawRectangle(SELECT_LGT_SX,
                          SELECT_LGT_SY + currentSel * SELECT_LGT_DY,
                          SELECT_LGT_SX + SELECT_LGT_WIDTH,
                          SELECT_LGT_SY + SELECT_LGT_HEIGHT + currentSel * SELECT_LGT_DY,
                          254);

            // restore palette
            writePalette((RgbPalettePtr)&GamePalette);

            // enter setup event loop
            panelFx(1);

            while (GameState == GAME_SETUP) {
                // this event loop is for the setup phase

                // test for up or down
                if (KeyboardState[MAKE_UP]) {
                    // erase current cursor position
                    drawBox(SELECT_BOX_SX,
                            SELECT_BOX_SY + currentSel * SELECT_BOX_DY,
                            SELECT_BOX_SX + SELECT_BOX_WIDTH,
                            SELECT_BOX_SY + SELECT_BOX_HEIGHT + currentSel * SELECT_BOX_DY,
                            0);

                    drawRectangle(SELECT_LGT_SX,
                                  SELECT_LGT_SY + currentSel * SELECT_LGT_DY,
                                  SELECT_LGT_SX + SELECT_LGT_WIDTH,
                                  SELECT_LGT_SY + SELECT_LGT_HEIGHT + currentSel * SELECT_LGT_DY,
                                  0);

                    // move up one and draw cursor and box
                    if (--currentSel < 0) {
                        currentSel = MAX_SELECTION;
                    }

                    // draw the new selected selection
                    drawBox(SELECT_BOX_SX,
                            SELECT_BOX_SY + currentSel * SELECT_BOX_DY,
                            SELECT_BOX_SX + SELECT_BOX_WIDTH,
                            SELECT_BOX_SY + SELECT_BOX_HEIGHT + currentSel * SELECT_BOX_DY,
                            254);

                    drawRectangle(SELECT_LGT_SX,
                                  SELECT_LGT_SY + currentSel * SELECT_LGT_DY,
                                  SELECT_LGT_SX + SELECT_LGT_WIDTH,
                                  SELECT_LGT_SY + SELECT_LGT_HEIGHT + currentSel * SELECT_LGT_DY,
                                  254);

                    digitalFxPlay(KRKKEY_VOC, 3);

                    timeDelay(1);
                }

                if (KeyboardState[MAKE_DOWN]) {
                    // erase current cursor position
                    drawBox(SELECT_BOX_SX,
                            SELECT_BOX_SY + currentSel * SELECT_BOX_DY,
                            SELECT_BOX_SX + SELECT_BOX_WIDTH,
                            SELECT_BOX_SY + SELECT_BOX_HEIGHT + currentSel * SELECT_BOX_DY,
                            0);

                    drawRectangle(SELECT_LGT_SX,
                                  SELECT_LGT_SY + currentSel * SELECT_LGT_DY,
                                  SELECT_LGT_SX + SELECT_LGT_WIDTH,
                                  SELECT_LGT_SY + SELECT_LGT_HEIGHT + currentSel * SELECT_LGT_DY,
                                  0);

                    // move up one and draw cursor and box
                    if (++currentSel > MAX_SELECTION) {
                        currentSel = 0;
                    }

                    // draw the new selected selection
                    drawBox(SELECT_BOX_SX,
                            SELECT_BOX_SY + currentSel * SELECT_BOX_DY,
                            SELECT_BOX_SX + SELECT_BOX_WIDTH,
                            SELECT_BOX_SY + SELECT_BOX_HEIGHT + currentSel * SELECT_BOX_DY,
                            SELECT_REG);

                    drawRectangle(SELECT_LGT_SX,
                                  SELECT_LGT_SY + currentSel * SELECT_LGT_DY,
                                  SELECT_LGT_SX + SELECT_LGT_WIDTH,
                                  SELECT_LGT_SY + SELECT_LGT_HEIGHT + currentSel * SELECT_LGT_DY,
                                  SELECT_REG);

                    digitalFxPlay(KRKKEY_VOC, 3);

                    timeDelay(1);
                }

                // test for a selection via enter key
                if (KeyboardState[MAKE_ENTER]) {
                    digitalFxPlay(KRKKEY_VOC, 3);

                    // what is the selection?
                    switch (currentSel) {
                        case 0: {
                            GameState = GAME_RUNNING;
                        } break;

                        case 1: {
                            // call the mech selection function
                            selectMech();

                            // re-draw main menu
                            introControls();
                        } break;

                        case 2: {
                            // show user the rules and instructions
                            introBriefing();

                            // re-draw main menu
                            introControls();
                        } break;

                        case 3: {
                            GameState = GAME_OVER;
                        } break;

                        default: break;
                    }

                    drawBox(SELECT_BOX_SX,
                            SELECT_BOX_SY + currentSel * SELECT_BOX_DY,
                            SELECT_BOX_SX + SELECT_BOX_WIDTH,
                            SELECT_BOX_SY + SELECT_BOX_HEIGHT + currentSel * SELECT_BOX_DY,
                            254);

                    drawRectangle(SELECT_LGT_SX,
                                  SELECT_LGT_SY + currentSel * SELECT_LGT_DY,
                                  SELECT_LGT_SX + SELECT_LGT_WIDTH,
                                  SELECT_LGT_SY + SELECT_LGT_HEIGHT + currentSel * SELECT_LGT_DY,
                                  254);
                }

                // slow things down a bit
                timeDelay(1);

                // check on music
                if (MusicEnabled) {
                    // test if piece is complete or has been stopped
                    if (musicStatus() == 2 || musicStatus() == 0) {
                        // advance to next sequence
                        if (++IntroSeqIndex == 14) {
                            IntroSeqIndex = 0;
                        }

                        musicPlay((MusicPtr)&Song, IntroSequence[IntroSeqIndex]);
                    }
                }

                // do special fx
                panelFx(0);
            }
        }

        // game running section
        else if (GameState == GAME_RUNNING) {
            // restore palette
            writePalette((RgbPalettePtr)&GamePalette);

            // reset system variables
            resetSystem();

            // restart everything
            initMissiles();

            initAliens();

            // start music
            if (MusicEnabled) {
                musicStop();

                // start from beginning sequence
                GameSeqIndex = 0;

                musicPlay((MusicPtr)&Song, GameSequence[GameSeqIndex]);
            }

            // clear double buffer
            fillDoubleBuffer32(0);

            // load in instrument area
            pcxInit((PcxPicturePtr)&ImagePcx);
            pcxLoad("krkcp.pcx", (PcxPicturePtr)&ImagePcx, 1);
            pcxShowBuffer((PcxPicturePtr)&ImagePcx);
            pcxDelete((PcxPicturePtr)&ImagePcx);

            // set up displays
            drawScanner(SCANNER_CLEAR);
            drawScanner(SCANNER_LOGO);

            drawTactical(TACTICAL_CLEAR);
            drawTactical(TACTICAL_DRAW);

            // reset color animation registers
            miscColorInit();

            // enter into the main game loop
            while (GameState == GAME_RUNNING) {
                // compute starting time of this frame
                StartingTime = timerQuery();

                // keyboard input section

                // change ship velocity
                if (KeyboardState[MAKE_UP]) {
                    // speed up
                    if ((ShipSpeed += 5) > 55) {
                        ShipSpeed = 55;
                    }
                }

                if (KeyboardState[MAKE_DOWN]) {
                    // slow down
                    if ((ShipSpeed -= 5) < -55) {
                        ShipSpeed = -55;
                    }
                }

                // test for turns
                if (KeyboardState[MAKE_RIGHT]) {
                    // rotate ship to right
                    if ((ShipYaw += 4) >= 360) {
                        ShipYaw -= 360;
                    }
                }

                if (KeyboardState[MAKE_LEFT]) {
                    // rotate ship to left
                    if ((ShipYaw -= 4) < 0) {
                        ShipYaw += 360;
                    }
                }

                // test for weapons fire
                if (KeyboardState[MAKE_SPACE]) {
                    // fire a missile
                    startMissile(PLAYER_OWNER,
                                 &ViewPoint,
                                 &ShipDirection,
                                 ShipSpeed + 30,
                                 100);

                    // illuminate instrument panel to denote a shot has been fired
                    writeColorReg(PLAYERS_WEAPON_FIRE_REG, (RgbColorPtr)&BrightRed);

                    // set timer to turn off illumination
                    weaponsLitCount = 2;
                }

                // instrumentation checks

                // left hand scanner
                if (KeyboardState[MAKE_S]) {
                    if (ScannerState == 1) {
                        // turn the scanner off
                        ScannerState = 0;

                        // replace krk logo
                        drawScanner(SCANNER_CLEAR);

                        drawScanner(SCANNER_LOGO);
                    } else {
                        // turn the scanner on
                        ScannerState = 1;

                        // clear the scanner area for radar image
                        drawScanner(SCANNER_CLEAR);

                        digitalFxPlay(KRKSCN_VOC, 1);
                    }
                }

                // right half tactical display
                if (KeyboardState[MAKE_T]) {
                    // test if tactical was off
                    if (TacticalState == TACTICAL_MODE_OFF) {
                        // play sound
                        digitalFxPlay(KRKTAC_VOC, 1);
                    }

                    // toggle to next state of tactical display

                    // clear tactical display before next state
                    drawTactical(TACTICAL_CLEAR);

                    // move to next state
                    if (++TacticalState > TACTICAL_MODE_OFF) {
                        TacticalState = TACTICAL_MODE_STS;
                    }

                    // based on new tactical state draw proper display
                    drawTactical(TACTICAL_DRAW);
                }

                // heads up display
                if (KeyboardState[MAKE_H]) {
                    // toggle hud
                    if (HudState == 1) {
                        HudState = 0;

                        // set indicator to proper illumination
                        writeColorReg(HUD_REG, (RgbColorPtr)&DarkBlue);
                    } else {
                        HudState = 1;

                        // set indicator to proper illumination
                        writeColorReg(HUD_REG, (RgbColorPtr)&BrightBlue);

                        digitalFxPlay(KRKHUD_VOC, 1);
                    }
                }

                // test for exit
                if (KeyboardState[MAKE_ESC]) {
                    GameState = GAME_SETUP;
                }

                // motion and control section

                // create a trajectory vector aligned with view direction
                ShipDirection.x = SinLook[ShipYaw];
                ShipDirection.y = 0;
                ShipDirection.z = CosLook[ShipYaw];

                // move viewpoint based on ship trajectory
                ViewPoint.x += ShipDirection.x * ShipSpeed;
                ViewPoint.z += ShipDirection.z * ShipSpeed;

                // move objects here
                moveMissiles();

                // move and perform AI for aliens
                processAliens();

                // test ship against universe boundaries
                if (ViewPoint.x > GAME_MAX_WORLD_X) {
                    ViewPoint.x = GAME_MAX_WORLD_X;
                } else if (ViewPoint.x < GAME_MIN_WORLD_X) {
                    ViewPoint.x = GAME_MIN_WORLD_X;
                }

                if (ViewPoint.z > GAME_MAX_WORLD_Z) {
                    ViewPoint.z = GAME_MAX_WORLD_Z;
                } else if (ViewPoint.z < GAME_MIN_WORLD_Z) {
                    ViewPoint.z = GAME_MIN_WORLD_Z;
                }

                // add in vibrational noise due to terrain
                if (ShipSpeed) {
                    ViewPoint.y = 40 + rand() % (1 + abs(ShipSpeed) / 8);
                } else {
                    ViewPoint.y = 40;
                }

                // test for ship hit message
                if (ShipMessage == SHIP_HIT) {
                    // do screen shake
                    ViewPoint.y = 40 + 5 * (rand() % 10);

                    // perform color fx

                    // test if shake complete
                    if (--ShipTimer < 0) {
                        // reset ships state
                        ShipMessage = SHIP_STABLE;

                        ViewPoint.y = 40;

                        // reset colors
                    }
                }

                // transform stationary objects here
                rotateObject(&StaticObj[STATIONS_TEMPLATE], 0, 10, 0);

                rotateObject(&StaticObj[TELEPODS_TEMPLATE], 0, 15, 0);

                // set view angles based on trajectory of ship
                ViewAngle.angY = ShipYaw;

                // now that user has possibly moved viewpoint, create the global
                // world to camera transformation matrix
                createWorldToCamera();

                // reset the polygon list
                generatePolyList(NULL, RESET_POLY_LIST);

                // perform general 3-D pipeline for all 3-D objects

                // first draw stationary objects
                drawStationaryObjects();

                // draw dynamic objects
                drawAliens();

                // draw the missiles
                drawMissiles();

                // draw background
                drawBackground((int)(ShipYaw * .885));

                // sort the polygons
                sortPolyList();

                // draw the polygon list
                drawPolyList();

                // draw the instruments here
                if (ScannerState == 1) {
                    // erase old blips
                    drawScanner(SCANNER_ERASE_BLIPS);

                    // refresh scanner image
                    drawScanner(SCANNER_DRAW_BLIPS);
                }

                // update tactical display
                if (TacticalState != TACTICAL_MODE_OFF) {
                    drawTactical(TACTICAL_UPDATE);
                }

                if (HudState == 1) {
                    drawHud();
                }

                // do special color fx

                // flicker the engines of the aliens
                tallonColorFx();

                sliderColorFx();

                // strobe the perimeter barriers
                barrierColorFx();

                // take care of weapons flash
                if (weaponsLitCount > 0) {
                    // test if it's time to to off flash
                    if (--weaponsLitCount == 0) {
                        writeColorReg(PLAYERS_WEAPON_FIRE_REG, (RgbColorPtr)&Black);
                    }
                }

                // test if screen should be colored to simulate fire blast
                if (ShipMessage == SHIP_HIT) {
                    // test for time intervals
                    if (ShipTimer > 5 && (rand() % 3) == 1) {
                        fillDoubleBuffer32(SHIP_FLAME_COLOR + rand() % 16);
                    }
                }

                // display double buffer
                displayDoubleBuffer32(DoubleBuffer, 0);

                // lock onto 18 frames per second max
                while ((timerQuery() - StartingTime) < 1);

                // check on music
                if (MusicEnabled) {
                    if (musicStatus() == 2 || musicStatus() == 0) {
                        // advance to next sequence
                        if (++GameSeqIndex == 21) {
                            GameSeqIndex = 0;
                        }

                        musicPlay((MusicPtr)&Song, GameSequence[GameSeqIndex]);
                    }
                }
            }

            // test if there is a winner or user just decided to exit
            screenTransition(SCREEN_DARKNESS);

            // restart intro music
            if (MusicEnabled) {
                // stop game music, start intro music again
                IntroSeqIndex = 0;
                musicStop();
                musicPlay((MusicPtr)&Song, IntroSequence[IntroSeqIndex]);
            }
        }
    }

    // game over section

    // exit in a very cool way
    screenTransition(SCREEN_SWIPE_X);

    // free up all resources
    deleteDoubleBuffer();

    // close down FX
    digitalFxClose();

    // show the credits
    closingScreen();

    // close down music
    musicClose();

    // remove the keyboard handler
    keyboardRemoveDriver();

    setGraphicsMode(TEXT_MODE);

    // see ya!
    printf("\nKILL OR BE KILLED:Normal Shutdown.\n");
}
