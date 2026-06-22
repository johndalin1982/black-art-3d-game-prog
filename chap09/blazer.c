// blazer.c - Starblazer: 320x200 mode 13h, 640x480x256 SVGA with
// -dVBE_SUPPORT, or 1024x768 true-colour with IPX LAN play and AI via
// -dBLAZERX -dVBE_SUPPORT (blazerx.tgt sets both)

#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <dos.h>
#include <bios.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <math.h>
#ifdef VBE_SUPPORT
#include <stdlib.h>
#endif
#include <string.h>

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#ifdef BLAZERX
#include "ipx.h"
#endif

// digital sound system
#define BLZCLK_VOC      0   // engaging the cloaking device
#define BLZEXP1_VOC     1   // standard explosion
#define BLZEXP2_VOC     2   // the super nova
#define BLZLAS_VOC      3   // laser torpedo sound
#define BLZNRG_VOC      4   // energy up sound
#define BLZSHLD_VOC     5   // shields engaging
#define BLZTAC_VOC      6   // tactical heads up
#define BLZSCN_VOC      7   // scanners turning on
#define BLZMISS_VOC     8   // mission debriefing

#define BLZBIOS_VOC     9   // bios is ok
#define BLZENTR_VOC     10  // entering game arena
#define BLZABRT_VOC     11  // function aborted
#define BLZSEL_VOC      12  // a selection was made
#define BLZKEY_VOC      13  // a key has been pressed
#define BLZDIAL_VOC     14  // the computer is dialing

#define BLZLOS_VOC      15  // the loser sound
#define BLZWIN_VOC      16  // the loser sound


#define NUM_SOUND_FX    17  // the number of sound fx loaded in

// music system
#define NUM_INTRO_SEQUENCES 11  // the number of elements in intro music score
#define NUM_GAME_SEQUENCES  18  // the number of elements in game music score

// modem defines

// defines for briefing instructions
#define NUM_PAGES       12
#define NUM_LINES_PAGE  17

// defines for the super nova explosion
#define NUM_CINDERS         20      // maximum number of cinders per explosion

#ifdef BLAZERX
// a cinder's color steps through this 16-entry fire gradient (pale red to
// dark orange) over its lifetime - .color holds an index into it, 0..15
#define CINDER_START_COLOR  0       // starting cinder color index
#define CINDER_END_COLOR    15      // ending cinder color index
#else
#define CINDER_START_COLOR  48      // starting cinder color
#define CINDER_END_COLOR    (48+15) // ending cinder color
#endif

#define NUM_NOVAS           3       // maximum number of super novas in game

#define NOVA_ACTIVE         1       // states of the novas
#define NOVA_INACTIVE       0

// heads up display defines
#ifdef BLAZERX
//
// The scanner is UI, not a CAM_ZOOM'd world object: its size/position track
// screen resolution only. 640x480 and 1024x768 share the same 4:3 aspect (a
// clean uniform 1024/640 = 768/480 = 1.6x both axes, unlike the 320x200 ->
// 640x480 step - see vbe/chap09/blazer.c's SCANNER_Y comment), so these are
// simply 1.6x the corrected 640x480 values (256, 399, grid 128x76).
#define SCANNER_X           410     // 256 * 1.6
#define SCANNER_Y           638     // 399 * 1.6

// Grid lines thicken with resolution too - 1.6x of vbe/chap09/blazer.c's
// 2px (see SCANNER_LINE_THICK there), rounded to the nearest clean integer.
#define SCANNER_LINE_THICK  3
#else
#ifdef VBE_SUPPORT
//
// The scanner grid: 320x200 original is SCANNER_X=128 (40% of width),
// SCANNER_Y=166, grid 64x32 - bottom edge at 198, 2px from the 200px-tall
// screen (hugs the bottom). Grid is 20% of width, 16% of height. 640x480 and
// 320x200 do NOT share an aspect ratio (4:3 vs 8:5), so width scaled a clean
// 2x (128->256) but height must scale by the true 480/200=2.4x, not 2x, to
// keep the same hugs-the-bottom position and screen-proportion size:
// height 32 * 2.4 = 76.8 -> 76 (a clean 38px/row split); bottom margin
// 2 * 2.4 = 4.8 -> 5px; SCANNER_Y = 480 - 76 - 5 = 399.
#define SCANNER_X           256     // position of scanner on screen
#define SCANNER_Y           399

// Grid lines are the book's 1px, which reads as too thin at this
// resolution's larger scanner box - thickened to 2px (see lineH2Thick/
// lineV2Thick).
#define SCANNER_LINE_THICK  2
#else
#define SCANNER_X           128     // position of scanner on screen
#define SCANNER_Y           166
#endif
#endif

#define MAX_BLIPS           50      // maximum number of scanner blips

#ifdef VBE_SUPPORT
// The book's blips are a single pixel, which reads as an invisible speck at
// this resolution's larger scanner box - same rationale, same value, as
// SCANNER_LINE_THICK.
#define BLIP_SIZE           SCANNER_LINE_THICK
#endif

#ifdef BLAZERX
#define BLIP_ASTEROID       RGB32(85, 85,255)  // colors for scanner blips
#define BLIP_PLAYER         RGB32(85,255, 85)
#define BLIP_REMOTE         RGB32(255,85, 85)
#define BLIP_ALIEN          RGB32(255,85,255)
#else
#define BLIP_ASTEROID       9       // colors dor scanner blips
#define BLIP_PLAYER         10
#define BLIP_REMOTE         12
#define BLIP_ALIEN          13
#endif

#ifdef BLAZERX
#define MISSILE_COLOR       RGB32(85,255,85)   // photon missile color
#define SCANNER_GRID_COLOR  RGB32(85,255,85)   // scanner grid line color

#define RIGHT_HEADS_TEXT_X  768     // position of rightmost text for headsup
#define RIGHT_HEADS_TEXT_Y  384

#define LEFT_HEADS_TEXT_X   0       // position of leftmost text for headsup
#define LEFT_HEADS_TEXT_Y   384
#else
#ifdef VBE_SUPPORT
#define RIGHT_HEADS_TEXT_X  480     // position of rightmost text for headsup
#define RIGHT_HEADS_TEXT_Y  240

#define LEFT_HEADS_TEXT_X   0       // position of leftmost text for headsup
#define LEFT_HEADS_TEXT_Y   240
#else
#define RIGHT_HEADS_TEXT_X  240     // position of rightmost text for headsup
#define RIGHT_HEADS_TEXT_Y  120

#define LEFT_HEADS_TEXT_X   0       // position of leftmost text for headsup
#define LEFT_HEADS_TEXT_Y   120
#endif
#endif

#define HEADS_CLOAK         1       // bitmap indices for various headsup elements
#define HEADS_SCANNER       2
#define HEADS_COMM          3
#define HEADS_SHIPS         4
#define HEADS_ENERGY        5
#define HEADS_SHIELDS

#define HEADS_1             1
#define HEADS_2             2
#define HEADS_3             3

#define HEADS_GAUGE_1       1

// fuel cell defines
#define NUM_FUEL_CELLS      4   // number of fuel cells in game
#define NUM_FUEL_FRAMES     3   // number of animation frames for fuel cells

#define FUEL_CELL_ACTIVE    1   // states of a fuel cell
#define FUEL_CELL_ABSORBED  0

// alien defines

// number of alien animation frames
#define NUM_ALIEN_FRAMES    4
#define ALIEN_ODDS          50  // probability of alien coming out

#define ALIEN_INACTIVE      0   // states of the alien
#define ALIEN_RANDOM        1   // the three modes of intelligence
#define ALIEN_CHASE_PLAYER  2
#define ALIEN_CHASE_REMOTE  3

// worm hole defines
#define NUM_WORMHOLE_FRAMES 8       // number of wormhole animation frames

#define WORMHOLE_X          1250    // position of wormhole
#define WORMHOLE_Y          1250

// starting position of the intro startup sequence
#ifdef BLAZERX
#define START_MESS_X        6
#define START_MESS_Y        26
#else
#ifdef VBE_SUPPORT
#define START_MESS_X        4
#define START_MESS_Y        16
#else
#define START_MESS_X        2
#define START_MESS_Y        8
#endif
#endif

// general explosions
#define NUM_EXPLOSIONS          4   // number of explosions that can run at once
#define NUM_EXPLOSION_FRAMES    6   // number frames in explosion

#define EXPLOSION_INACTIVE      0   // state of an inactive explosion
#define EXPLOSION_ACTIVE        1   // state of a live explosion

// missile defines
#define GENERIC_MISSILE     0

#define PLAYER_MISSILE      1   // this missile was fired by the local player
#define REMOTE_MISSILE      2   // this missile was fired by remote player

#define MISS_ACTIVE         1   // flags that a missile is alive
#define MISS_INACTIVE       0   // flags that a missle is dead
#define NUM_MISSILES        12  // number of total missiles allowed in world

// state of the game itself
#define GAME_SETUP          0   // the game is in the setup mode
#define GAME_LINKING        1   // the communication link is being established
#define GAME_RUNNING        2   // the game is running
#define GAME_PAUSED         3   // the game is paused (not implemented)
#define GAME_OVER           4   // what do you think

// general object states
#define DEAD                0   // these are general states for any object
#define ALIVE               1
#define DYING               2

// defines for setup selection
#define SETUP_PLAY_SOLO             0
#define SETUP_MAKE_CONNECTION       1
#define SETUP_WAIT_FOR_CONNECTION   2
#define SETUP_SELECT_SHIP           3
#define SETUP_SET_COMM_PORT         4
#define SETUP_BRIEFING              5
#define SETUP_EXIT                  6

#define NUM_SETUP                   7   // number of setup choices

#ifdef BLAZERX
// the coordinates of the setup display window (486,163,146,67 * 1024/640,
// 768/480 - blazecon.pcx was rescaled by the same factor, so this stays
// aligned with the window drawn into that background)
#define DISPLAY_X                   778 // position of the small setup display box
#define DISPLAY_Y                   261
#define DISPLAY_WIDTH               234 // size of window
#define DISPLAY_HEIGHT              107
#else
// the coordinates of the setup display window
#ifdef VBE_SUPPORT
#define DISPLAY_X                   486 // position of the small setup display box
#define DISPLAY_Y                   163
#define DISPLAY_WIDTH               146 // size of window
#define DISPLAY_HEIGHT              67
#else
#define DISPLAY_X                   243 // position of the small setup display box
#define DISPLAY_Y                   68
#define DISPLAY_WIDTH               73  // size of window
#define DISPLAY_HEIGHT              28
#endif
#endif

#define DISPLAY_IMG_SHIPS           0   // commands to the display function
#define DISPLAY_IMG_PORTS           1   // indicating icons to display

#ifdef BLAZERX
// The camera: the game simulates in the book's 320x200-era units (one unit =
// one mode-13h pixel), and the camera scales that world onto the 1024x768
// render window at draw time - so speeds, distances and collision sizes keep
// the book's values. VIEW_* is how much of the universe is visible. The
// sprite bitmaps are pre-scaled by CAM_ZOOM, so only positions transform.
#define CAM_ZOOM            4                   // world units -> screen pixels
#define VIEW_WIDTH          (1024 / CAM_ZOOM)   // world units visible across
#define VIEW_HEIGHT         (768 / CAM_ZOOM)    // world units visible down
#else
#ifdef VBE_SUPPORT
// The camera: the game simulates in the book's 320x200-era units (one unit =
// one mode-13h pixel), and the camera scales that world onto the 640x480
// render window at draw time - so speeds, distances and collision sizes keep
// the book's values. VIEW_* is how much of the universe is visible. The
// sprite bitmaps are pre-scaled by CAM_ZOOM, so only positions transform.
#define CAM_ZOOM            2                   // world units -> screen pixels
#define VIEW_WIDTH          (640 / CAM_ZOOM)    // world units visible across
#define VIEW_HEIGHT         (480 / CAM_ZOOM)    // world units visible down
#endif
#endif

#ifdef BLAZERX
#define SPRITE_TRANSPARENT_COLOR   RGB32(255, 0, 255)   // magic magenta: the sprite-sheet key color

// Shield/engine tint-key colors baked into blazeshl.pcx/blazeshr.pcx (by
// regen_assets.py) at the pixel positions that used to be the reserved
// shield/engine palette registers. Index 0 = shield region, index 1 = engine
// region - matches spriteDrawTinted's tintColors ordering.
static const unsigned long PlayersTintKeys[2] = { RGB32(0,0,255), RGB32(43,199,139) };
static const unsigned long RemotesTintKeys[2] = { RGB32(255,0,0), RGB32(43,199,183) };

// size of the "tech" font used in intro (8x14 * 1024/640, 768/480 - blazefnt.pcx
// was rescaled by the same factor, since it's screen-space UI, not part of
// the CAM_ZOOM'd world)
#define TECH_FONT_WIDTH             13  // width of high tech font
#define TECH_FONT_HEIGHT            22  // height of high tech font
#else
// size of the "tech" font used in intro
#ifdef VBE_SUPPORT
#define TECH_FONT_WIDTH             8   // width of high tech font
#define TECH_FONT_HEIGHT            14  // height of high tech font
#else
#define TECH_FONT_WIDTH             4   // width of high tech font
#define TECH_FONT_HEIGHT            7   // height of high tech font
#endif
#endif
#define NUM_TECH_FONT               64  // number of characters in tech font

// asteroid defines
#define ASTEROID_SMALL              0   // different asteroid sizes
#define ASTEROID_MEDIUM             1
#define ASTEROID_LARGE              2

#define ASTEROID_INACTIVE           0   // asteroid states
#define ASTEROID_ACTIVE             1
#define ASTEROID_INOP               2

#define NUM_LARGE_ASTEROIDS         6   // max number of large asteroids in the game
#define NUM_MEDIUM_ASTEROIDS        10  // max number of medium asteroids in game
#define NUM_SMALL_ASTEROIDS         24  // max number of small asteroids in game

#define START_LARGE_ASTEROIDS       0   // starting index of large asteroids
#define START_MEDIUM_ASTEROIDS      6   // starting index of medium asteroids
#define START_SMALL_ASTEROIDS       16  // starting index of small asteroids

#define END_LARGE_ASTEROIDS         (0 + 6 - 1) // ending indices
#define END_MEDIUM_ASTEROIDS        (6 + 10 - 1)
#define END_SMALL_ASTEROIDS         (16 + 24 - 1)

#define ASTEROID_LARGE_WIDTH        34  // size of large asteroids
#define ASTEROID_LARGE_HEIGHT       28

#define ASTEROID_MEDIUM_WIDTH       16  // size of medium asteroids
#define ASTEROID_MEDIUM_HEIGHT      14

#define ASTEROID_SMALL_WIDTH        8   // size of small asteroids
#define ASTEROID_SMALL_HEIGHT       6

#define NUM_ASTEROID_FRAMES         8   // number of asteroid animation frames

#define NUM_ASTEROIDS               (6 + 10 + 24)   // total number of asteroids in universe

// defines for the star field
#define NUM_STARS                   50  // number of stars in universe

#define STAR_PLANE_0                0   // far plane
#define STAR_PLANE_1                1   // near plane
#define STAR_PLANE_2                2

#ifdef BLAZERX
#define STAR_COLOR_0                RGB32( 85, 85, 85)  // color of farthest star plane
#define STAR_COLOR_1                RGB32(170,170,170)
#define STAR_COLOR_2                RGB32(255,255,255) // color of nearest star plane
#else
#define STAR_COLOR_0                8   // color of farthest star plane
#define STAR_COLOR_1                7
#define STAR_COLOR_2                15  // color of nearest star plane
#endif

// dimensions of the universe
#define UNIVERSE_WIDTH              2500
#define UNIVERSE_HEIGHT             2500
#define UNIVERSE_BORDER             160 // the thickness of the border around the universe

// defines for ships
#define GRYFON_SHIP                 0
#define RAPTOR_SHIP                 1

#define SHIP_WIDTH                  22  // size of both ships
#define SHIP_HEIGHT                 18

#ifdef VBE_SUPPORT
// world sizes of the other in-world sprites (sprite bitmaps are pre-scaled
// x CAM_ZOOM, so spriteInit passes these x CAM_ZOOM, same as the ships
// and asteroids above)
#define EXPLOSION_WIDTH             28
#define EXPLOSION_HEIGHT            22

#define WORMHOLE_WIDTH              26
#define WORMHOLE_HEIGHT             22

#define FUEL_CELL_WIDTH             20
#define FUEL_CELL_HEIGHT            18

#define ALIEN_WIDTH                 14
#define ALIEN_HEIGHT                8
#endif

#ifndef BLAZERX
// shield defines
#define PLAYERS_SHIELD_REG          240 // color registers of players shield
#define REMOTES_SHIELD_REG          241 // color and remotes shield color
#endif

// shield timing (applies to both the player and the enemy AI). Once raised,
// shields stay up for SHIELD_ON_TIME frames and CANNOT be dropped early; when
// they expire they must recharge for SHIELD_COOLDOWN_TIME frames before they
// can be raised again. The game runs at ~18 fps, so 100 frames is ~5.5 seconds.
#define SHIELD_ON_TIME              100
#define SHIELD_COOLDOWN_TIME        100

#ifdef BLAZERX
// Which sprite pixels are the shield-glow/engine-flame regions is now
// identified by color (PlayersTintKeys/RemotesTintKeys, defined above) rather
// than a DAC register number - a ship's own sheet already distinguishes
// player/remote and shield/engine by which of its two tint keys a pixel
// matches, so a single shared tint-key pair per ship side is enough (no
// separate per-ship-part register numbers needed).
#else
// engine defines
#define PLAYERS_ENGINE_REG          242 // color registers for engine flicker
// BUG FIX: the book defined this as 241 — the SAME register as REMOTES_SHIELD_REG
// — so once the remote ship could thrust, its engine flicker wrote the (white)
// engine color into the shield register and lit up a phantom white shield outline.
// 243 is the distinct register the remote ship's art uses (240/241/242/243 =
// P-shield / R-shield / P-engine / R-engine).
#define REMOTES_ENGINE_REG          243
#endif

#ifdef BLAZERX
// the setup screen's scrolling light strip: a vertical band on blazecon.pcx,
// a horizontal band on blazeins.pcx (screen argument to panelFx selects
// which). Positions are the original art's light-strip bounding box * the
// same 1024/640, 768/480 factor used throughout the resolution port.
#define PANEL_FX_CONTROL            0
#define PANEL_FX_BRIEFING           1

#define NUM_PANEL_LIGHTS            10  // number of scrolling light segments

#define PANEL_LIGHTS_CONTROL_X1     272
#define PANEL_LIGHTS_CONTROL_X2     315
#define PANEL_LIGHTS_CONTROL_Y1     238
#define PANEL_LIGHTS_CONTROL_Y2     643

#define PANEL_LIGHTS_BRIEFING_X1    291
#define PANEL_LIGHTS_BRIEFING_X2    437
#define PANEL_LIGHTS_BRIEFING_Y1    46
#define PANEL_LIGHTS_BRIEFING_Y2    64
#else
// introduction panel colors
#define END_PANEL_REG               41  // the color register range for the
#define START_PANEL_REG             32  // scrolling red lights on setup screen
#endif

// general identifiers for the player and remote
#define THE_PLAYER                  0
#define THE_REMOTE                  1

#define WINNER_NONE                 0
#define WINNER_PLAYER               1
#define WINNER_REMOTE               2

// remote keyboard events
#define REMOTE_RIGHT                1   // each one of these bits is set or reset
#define REMOTE_LEFT                 2   // in a single byte and sent to the remote
#define REMOTE_THRUST               4   // to indicate what the local player did
#define REMOTE_SHIELDS              8
#define REMOTE_CLOAK                16
#define REMOTE_FIRE                 32
#define REMOTE_ESC                  64

#ifdef BLAZERX
// single-player AI difficulty knobs. Lower the fire cooldown / raise the range and
// missile count to make the enemy shoot more; do the opposite to ease off.
#define AI_FIRE_RANGE       230     // shoots at the player within this Manhattan distance
#define AI_FIRE_COOLDOWN    24      // base frames between shots (smaller = more shooting)
#define AI_FIRE_JITTER      12      // random extra frames added to the cooldown
#define AI_MAX_MISSILES     2       // most AI shots allowed in flight at once (sim cap 5)
#define AI_AIM_ERROR        1       // aim slip in heading steps: each shot lands within
                                    // +/- this many 22.5-degree steps of the true
                                    // intercept (0 = sniper, 2 = wild)
#define AI_SHIELD_RANGE     40      // raises shields only for missiles this close
#define AI_START_GRACE      36      // frames of "hold fire" when a solo game starts
#define AI_LOW_ENERGY       1500    // below this the enemy breaks off to conserve
#define AI_CLOAK_ENERGY     6000    // cloaks to reposition only above this energy
#define AI_AVOID_AHEAD      10      // frames of course lookahead for asteroid avoidance
#define AI_AVOID_RANGE      30      // clearance (Manhattan) kept around a rock's size

// Smooth movement. The enemy steers its *velocity* (not its position): each frame
// it aims for the player's velocity plus a small per-behavior maneuver, and only
// thrusts to correct the difference. Matching the player's velocity means it
// glides on the (player-locked) screen instead of lurching, and it never builds
// runaway momentum - which is what removes the spin / weave / jerk.
// AI_MANEUVER is the maneuver speed (x the ~3-unit heading vector => ~6 px/frame,
// under the ship's max of 8, so the player can still outrun it). AI_VEL_TOL is how
// close to matched counts as good enough: within it the ship stops correcting and
// faces the player to shoot, which is the common, calm-looking case.
#define AI_MANEUVER         2
#define AI_VEL_TOL          4

// engagement distances (Manhattan) the behaviors hold relative to the player
#define AI_HUNT_DIST        100     // HUNT closes to here, then paces the player and fires
#define AI_STRAFE_DIST      120     // STRAFE circles at this radius
#define AI_STRAFE_BAND      25      // STRAFE nudges in/out only beyond this band around
                                    // AI_STRAFE_DIST; inside it it just circles (no jitter)
#define AI_FLEE_DIST        140     // FLEE backs off to here

// Backstop so it never leaves the player's ~320x200 view: beyond the return
// distance, or past the per-axis leash, it drops everything and heads back in.
#define AI_RETURN_DIST      160     // Manhattan distance that forces a return
#define AI_LEASH_X          150     // max horizontal stray before forcing a return
#define AI_LEASH_Y          110     // max vertical stray before forcing a return

// enemy AI behavior states
#define AI_HUNT             0       // close in, pace the player, and shoot (most common)
#define AI_STRAFE           1       // circle the player at range
#define AI_FLEE             2       // back off to reposition (no firing)

#endif
// this is the structure for the asteroids
typedef struct AsteroidType {
    int xv;         // x velocity of asteroid
    int yv;         // y velocity of asteroid
    int x, y;       // universe coordinates
    int type;       // type of asteroid, big, medium, small
    Sprite rock;    // the asteroid sprite
} Asteroid, *AsteroidPtr;

// this is the structure for the stars
typedef struct StarType {
#ifdef VBE_SUPPORT
    int x, y;       // position of star, in view (pre-camera) units
#else
    int x, y;       // position of star
#endif
    int color;      // color of star
    int plane;      // plane that star is in
#ifdef BLAZERX
    unsigned long backColor[CAM_ZOOM * CAM_ZOOM];   // the pixels under the star
#else
#ifdef VBE_SUPPORT
    unsigned char backColor[CAM_ZOOM * CAM_ZOOM];   // the pixels under the star
#else
    int backColor;  // the color of the pixel under the star
#endif
#endif
} Star, *StarPtr;

// this is the structure used for weapons and explosion particles
typedef struct ParticleType {
    int x, y;       // universe position
    int sx, sy;     // screen coordinates
    int xv, yv;     // velocity
    int type;       // type of particle
    int state;      // state of particle
    int counter;    // general counter
    int threshold;  // threshold for counter
    int color;      // color of particle
#ifdef BLAZERX
    unsigned long backColor[CAM_ZOOM * CAM_ZOOM];   // the pixels under the particle
#else
#ifdef VBE_SUPPORT
    unsigned char backColor[CAM_ZOOM * CAM_ZOOM];   // the pixels under the particle
#else
    int backColor;  // the color of the pixel under the particle
#endif
#endif
    int visible;    // helps speed clipping
    int lifetime;   // the lifetime of the particle in frames
} Particle, *ParticlePtr;

// this is the structure for a super nova explosion
typedef struct NovaType {
    int state;                      // state of super nova explosion
    int x, y;                       // position of super nova explosion
    int lifetime;                   // the lifetime of the super nova
    Particle cinders[NUM_CINDERS];  // particles of the explosion
} Nova, *NovaPtr;

// this is the structure for all simple creatures
typedef struct MonsterType {
    int x, y;       // position in universe coordinates
    int xv, yv;     // velocity of monster
    int state;      // state of monster
    int type;       // type of monster
    Sprite body;    // the body sprite for monster
} Monster, *MonsterPtr;

// this is a scanner object
typedef struct BlipType {
    int x, y;       // position of blip
    int color;      // color of blip i.e. type of scanner object
} Blip, *BlipPtr;

void initStars(void);
void moveStars(void);
void drawStars(void);
void eraseStars(void);
void underStars(void);
void initAsteroids(int small, int medium, int large);
void startAsteroid(int x, int y, int type);
void eraseAsteroids(void);
void drawAsteroids(void);
void underAsteroids(void);
void moveAsteroids(void);
void fontEngine1(int x, int y, int font, int color, char* string, unsigned char FAR* destination);
void clearDisplay(int color);
void introTitle(void);
void introControls(void);
void loadIcons(void);
void loadShips(void);
void doStarburst(void);
int displaySelect(int current);
void copyFrames(SpritePtr dest, SpritePtr source);
void shieldControl(int ship, int on);
void resetSystem(void);
void startPlayersDeath(void);
void resetPlayer(void);
void resetRemote(void);
void startRemotesDeath(void);
#ifdef BLAZERX
int aiHeading(int vx, int vy);
void aiPickState(void);
int computeRemoteAi(void);
void panelFx(int screen);
#else
void panelFx(void);
#endif
void eraseMissiles(void);
void underMissiles(void);
void drawMissiles(void);
void initMissiles(void);
void moveMissiles(void);
int startMissile(int x, int y, int xv, int yv, int color, int type);
void startExplosion(int x, int y, int speed);
void underExplosions(void);
void eraseExplosions(void);
void drawExplosions(void);
void animateExplosions(void);
void initExplosions(void);
void techPrint(int x, int y, char* string, unsigned char FAR* destination);
void loadWormhole(void);
void initWormhole(void);
void underWormhole(void);
void drawWormhole(void);
void animateWormhole(void);
void loadAlien(void);
void initAlien(void);
void alienControl(void);
void underAlien(void);
void eraseAlien(void);
void drawAlien(void);
void moveAlien(void);
void loadFuelCells(void);
void initFuelCells(void);
void underFuelCells(void);
void eraseFuelCells(void);
void drawFuelCells(void);
void animateFuelCells(void);
void initScanner(void);
void eraseScanner(void);
void drawScanner(void);
void initNovas(void);
void eraseNovas(void);
void underNovas(void);
void drawNovas(void);
void moveNovas(void);
void startNova(int x, int y);
void musicInit(void);
void musicClose(void);
void digitalFxInit(void);
void digitalFxClose(void);
int digitalFxPlay(int effect, int priority);
void drawBlips(void);
void underBlips(void);
void eraseBlips(void);
#ifdef BLAZERX
void cameraPixelUnder(int sx, int sy, unsigned long* back);
void cameraPixelDraw(int sx, int sy, int color);
void cameraPixelErase(int sx, int sy, const unsigned long* back);
#else
#ifdef VBE_SUPPORT
void cameraPixelUnder(int sx, int sy, unsigned char* back);
void cameraPixelDraw(int sx, int sy, int color);
void cameraPixelErase(int sx, int sy, const unsigned char* back);
#endif
#endif

PcxPicture ImagePcx,        // general PCX image used to load background and imagery
           ImageControls;   // this holds the controls screen

#ifndef BLAZERX
RgbColor Color1, Color2;    // used for temporaries during color rotation

RgbPalette GamePalette;     // this will hold the startup system palette
                            // so we can restore it after screen FX

#endif
Sprite Button1,             // the setup buttons
       Button2,
       Button3,
       Displays,            // the display bitmaps
       Starburst,           // the starburst sprite
       GryfonR,             // this holds a generic gryfon ship with remote colors
       RaptorR,             // this holds a generic raptor ship with remote colors
       GryfonL,             // this holds a generic gryfon ship with local colors
       RaptorL,             // this holds a generic raptor ship with local colors
       PlayersShip,         // this is the player's ship
       RemotesShip,         // this is the remote's ship
       Wormhole,            // a stationary worm hole in the game
       FuelCells[NUM_FUEL_CELLS],   // the stationary fuel cells in the game
       HeadsText,           // heads up display sprites
       HeadsNumbers,
       HeadsGauge;

Monster Alien;              // the alien saucer that comes out of the wormhole

Sprite Explosions[NUM_EXPLOSIONS];  // the explosions in the game

Nova Novas[NUM_NOVAS];              // the super nova explosions!

int ActiveExplosions = 0;           // number of active explosions

Asteroid Asteroids[NUM_ASTEROIDS];  // the asteroid fields

Star Stars[NUM_STARS];              // the star fields

Particle Missiles[NUM_MISSILES];    // all the projectiles

Bitmap TechFont[NUM_TECH_FONT];     // the tech font bitmaps

int GameState = GAME_SETUP;         // the overall state of the game

int CommPort = 0,                   // currently selected comm port
    PlayersShipType = 0,            // currently selected local ship type
    RemotesShipType = 1;            // the remote ship's type

int CommPortToAddress[2] = { COM_1, COM_2 };    // converts the selected com port to an address

int Master = 1,                     // the player dials up a player
    Slave = 0,                      // then he is master else he is alive
    Linked = 0,                     // state of the model communications system
    Winner = WINNER_NONE;           // the winner of the game
#ifdef BLAZERX

// single-player support: when AiEnabled is set the remote ship is flown by the
// computer instead of a networked peer. The AI produces the same REMOTE_* input
// byte a human would send, so the whole remote simulation runs unchanged.
int AiEnabled    = 0,               // 1 = remote ship is AI controlled (solo play)
    AiFireDelay  = 0,               // cooldown counter between AI shots
    AiAimSlip    = 0,               // this shot's aim error, in heading steps
    AiState      = AI_HUNT,         // the enemy's current behavior mode
    AiStateTimer = 0,               // frames left before re-choosing a behavior
    AiOrbitDir   = 1,               // +1 / -1: which way STRAFE / WANDER circles
    AiLastX      = 0,               // where the enemy last saw the player - the
    AiLastY      = 0,               // ghost it tracks (fire held) while cloaked
    AiLastVx     = 0,               // the player's velocity when last seen - the
    AiLastVy     = 0;               // ghost coasts on it while he stays cloaked
#endif

// the start up arrays used to differentiate the player and remote

// master is index 1, slave is index 0
int GameStartX[] = { 1200, 1300 };
int GameStartY[] = { 1200, 1200 };

// the variables for the player
int PlayersLastX          = 0,      // the last position of player
    PlayersLastY          = 0,
    PlayersX              = 0,      // current player's position
    PlayersY              = 0,
    PlayersDx             = 0,      // player's position deltas since last frame
    PlayersDy             = 0,
    PlayersXv             = 0,      // velocity of ship
    PlayersYv             = 0,
    PlayersEngine         = 0,      // state of engines
    PlayersStability      = 8,      // how long it takes for gravity
    PlayersFlameCount     = 0,      // used for engine flicker
    PlayersFlameTime      = 1,
    PlayersGravity        = 0,      // current gravitron count
    PlayersShields        = 0,      // state of the shields
    PlayersShieldTime     = 0,      // frames left in the current shield-up period
    PlayersShieldCooldown = 0,      // frames left before shields can be raised again
    PlayersCloak          = -1,     // state of the cloak -1 off 1 on
    PlayersHeads          = -1,     // state of heads up display
    PlayersComm           = -1,     // state of comm link
    PlayersScanner        = -1,     // space scanner
    PlayersNumShips       = 3,      // number of player's ship
    PlayersShieldStrength = 22000,  // the amount of energy player's shields have
    PlayersEnergy         = 22000,  // the amount of ship energy
    PlayersScore          = 0,      // the score of the player
    PlayersActiveMissiles = 0,      // the number of missiles the player has fired
    PlayersState          = ALIVE,  // state of player
    PlayersDeathCount     = 0;      // how long death sequence will last

int DebounceHud           = 0,      // these are used to debounce the players
    DebounceScan          = 0,      // inputs one some keys
    DebounceCloak         = 0,
    DebounceThrust        = 0,
    DebounceFire          = 0,
    DebounceShields       = 0;

#ifdef VBE_SUPPORT
int RefreshHeads          = 0;      // used to track when HUD needs refreshing

int UnderPlayersBlip[BLIP_SIZE * BLIP_SIZE],  // these hold the pixels under the
    UnderRemotesBlip[BLIP_SIZE * BLIP_SIZE];  // scanner blip of the player and remote
#else
int RefreshHeads          = 0,      // used to track when HUD needs refreshing
    UnderPlayersBlip,               // these hold the pixels under the scanner
    UnderRemotesBlip;               // blip image of the player and remote
#endif

int RemotesLastX          = 0,      // the last position of player
    RemotesLastY          = 0,
    RemotesX              = 0,      // current remote's position
    RemotesY              = 0,
    RemotesDx             = 0,      // remote's position deltas since last frame
    RemotesDy             = 0,
    RemotesXv             = 0,      // velocity of ship
    RemotesYv             = 0,
    RemotesEngine         = 0,      // state of engines
    RemotesStability      = 8,      // how long it takes for gravity
    RemotesFlameCount     = 0,      // used for engine flicker
    RemotesFlameTime      = 1,
    RemotesGravity        = 0,      // current gravitron count
    RemotesShields        = 0,      // state of the shields
    RemotesShieldTime     = 0,      // frames left in the current shield-up period
    RemotesShieldCooldown = 0,      // frames left before shields can be raised again
    RemotesCloak          = -1,     // state of the cloak -1 off 1 on
    RemotesHeads          = -1,     // state of heads up display
    RemotesComm           = -1,     // comm link
    RemotesScanner        = -1,     // space scanner
    RemotesNumShips       = 3,      // number of remote's ship
    RemotesShieldStrength = 22000,  // the amount of energy remote's shields have
    RemotesEnergy         = 22000,  // the amount of ship energy
    RemotesScore          = 0,      // the remote's score
    RemotesActiveMissiles = 0,      // the number of missiles the remote has fired
    RemotesState          = ALIVE,  // state of remote
    RemotesDeathCount     = 0;      // how long death sequence will last

// unit motion look up tables
int MotionDx[16] = { 0, 1, 2, 2, 3, 2, 2, 1, 0, -1, -2, -2, -3, -2, -2, -1 };
int MotionDy[16] = { -3, -2, -2, -1, 0, 1, 2, 2, 3, 2, 2, 1, 0, -1, -2, -2 };

// musical sequence information
int MusicEnabled = 0,       // flags that enable music and sound FX
    DigitalEnabled = 0;

int DigitalFxPriority = 10; // the priority tracker of the current effect

int IntroSequence[] = { 0, 2, 3, 4, 3, 4, 2, 3, 4, 3, 3 };
int IntroSeqIndex = 0;

int GameSequence[] = {
    0 + 5,
    1 + 5,
    4 + 5,
    5 + 5,
    4 + 5,
    2 + 5,
    1 + 5,
    3 + 5,
    1 + 5,
    5 + 5,
    5 + 5,
    4 + 5,
    1 + 5,
    2 + 5,
    3 + 5,
    2 + 5,
    4 + 5,
    4 + 5
};

int GameSeqIndex = 0;

Music Song; //the music structure

// sound fx stuff
Sound DigitalFx[NUM_SOUND_FX];

// these colors are used for the shields on the ships
RgbColor PrimaryRed   = { 63, 0, 0 },   // pure red
         PrimaryBlue  = { 0, 0, 63 },   // pure blue
         PrimaryGreen = { 0, 63, 0 },   // pure green
         PrimaryBlack = { 0, 0, 0 },    // pure black
         PrimaryWhite = { 63, 63, 63 }, // pure white

         RemotesShieldColor = { 0, 0, 0 },  // the current color of the remotes shield
         PlayersShieldColor = { 0, 0, 0 },  // the current color of the players shields

         PlayersEngineColor = { 0, 0, 0 },  // the color of the players engine
         RemotesEngineColor = { 0, 0, 0 };  // the color of the remotes engine

#ifdef BLAZERX
// The live shield/engine colors above are still computed in RgbColor's
// 6-bit VGA range (the pulse/flicker tuning is unchanged), then converted
// here for spriteDrawTinted. PlayersTintColors[0]/RemotesTintColors[0]
// hold the current RGB32 shield color, [1] the current engine color - 0
// (black) means off. Passed straight to spriteDrawTinted, matching the
// region ordering of PlayersTintKeys/RemotesTintKeys.
unsigned long PlayersTintColors[2] = { 0, 0 };
unsigned long RemotesTintColors[2] = { 0, 0 };

unsigned long vgaColorToRGB32(RgbColor c) {
    return RGB32(c.red * 255 / 63, c.green * 255 / 63, c.blue * 255 / 63);
}
#endif

// the instruction pages
char* Instructions[] = {
               "STARBLAZER MISSION BRIEFING,       ",
               "STAR DATE: 2595.001                ",
               "                                   ",
               "LONG RANGE SCANS HAVE DETECTED THE ",
               "PRESENCE OF AN UNKNOWN VESSEL IN   ",
               "THE TALLEON ASTEROID BELT. YOUR    ",
               "MISSION IS SIMPLE: SEEK OUT THE    ",
               "AGGRESSOR AND DESTROY IT.          ",
               "                                   ",
               "THE ASTEROID BELT IS FAIRLY SPARSE ",
               "AND POSES MINIMAL THREAT TO YOUR   ",
               "SHIP, BUT THE ASTEROIDS ARE DEADLY ",
               "IF THEY BREACH YOUR SHIPS HULL.    ",
               "                                   ",
               "                                   ",
               "                                   ",
               "               1                   ",
               "THE ASTEROID BELT EXISTS AT        ",
               "COORDINATES (0.0) TO (2500.2500),  ",
               "(USING THE STANDARD CENTRAL        ",
               "NAVIGATION MULTIVARIATE VECTOR     ",
               "POSITIONING SYSTEM). IF YOU TRY TO ",
               "GO BEYOND THESE BOUNDS YOUR SHIP   ",
               "WILL AUTOMATICALLY HYPERWARP TO    ",
               "THE OPPOSITE EDGE OF THE SECTOR.   ",
               "THUS,YOU CAN NOT LEAVE UNTIL YOU   ",
               "HAVE COMPLETED YOUR MISSION!       ",
               "                                   ",
               "THERE HAVE ALSO BEEN REPORTS OF A  ",
               "STABLE WORMHOLE IN THIS SECTOR. THE",
               "WORMHOLE SEEMS TO BE A SINGLE POLE ",
               "TRANSVERSE DISTURBANCE AND EMITS NO",
               "GRAVITRONS.                        ",
               "               2                   ",
               "BUT REPORTS DO DETIAL MATTER AND   ",
               "ENERGY EMMISIONS FROM THE WORMHOLE,",
               "SO STAY CLEAR OF IT! IT IS LOCATED ",
               "AT COORDINATES (1200.1200).        ",
               "                                   ",
               "FINALLY, A SMALL SHIP HAS BEEN     ",
               "DETECTED,HOWEVER,IT SEEMS TO HAVE  ",
               "NO HOSTILE INTENTIONS. MOREOVER,IT ",
               "APPEARS THAT IT HAS A SYMETRICAL   ",
               "SUB-SPACE FIELD SHIELDING SYSTEM   ",
               "WHICH OUR BEST SCIENTISTS STILL DO ",
               "NOT FULLY UNDERSTAND. NEEDLESS TO  ",
               "SAY, YOUR WEAPONS ARE USELESS      ",
               "AGAINST IT...                      ",
               "                                   ",
               "                                   ",
               "               3                   ",
               "SPECIFICATIONS                     ",
               "                                   ",
               "YOUR SHIP IS EQUIPPED WITH ALL OF  ",
               "THE LATEST FEATURES INCLUDING:     ",
               "HEADS UP DISPLAYS, SECTOR SCANNER, ",
               "TWIN TACHION DRIVE, AUTOFIRE       ",
               "PARTICLE CANNONS, PLASMA-FIELD-    ",
               "EFFECT SUB-SPACE SHIELDS,AND A     ",
               "CLOAKING DEVICE BUILT IN THE       ",
               "ANTARUS SYSTEM SPECIFICALLY FOR    ",
               "YOUR SHIP.                         ",
               "                                   ",
               "FINALLY, YOUR SHIP HAS THE NEW     ",
               "PROTOTYPE SHAPE SHIFTING PLASMA    ",
               "ENERGY HULL SYSTEM.                ",
               "                                   ",
               "               4                   ",
               "ENTERING INTO COMBAT               ",
               "                                   ",
               "TO PLAY THE GAME YOU CAN EITHER    ",
               "PLAY SOLO AGAINST THE COMPUTER OR  ",
#ifdef BLAZERX
               "OVER A LAN (IPX). IN SOLO MODE AN  ",
               "AI-CONTROLLED ENEMY SHIP WILL HUNT ",
               "YOU THROUGH THE ASTEROID BELT. TO  ",
#else
               "MODEM-2-MODEM. AN ENEMY SHIP SITS  ",
               "AT ITS SPAWN IN THE ASTEROID BELT  ",
               "AS A STATIONARY TARGET. TO         ",
#endif
               "PLAY SOLO, PICK YOUR SHIP WITH THE ",
               "SELECT SHIP OPTION ON THE MAIN     ",
               "MENU, THEN USE THE PLAY SOLO       ",
               "OPTION AND YOU WILL BE WARPED TO   ",
               "THE TALLEON ASTEROID BELT TO       ",
               "ENGAGE THE ENEMY.                  ",
               "                                   ",
               "                                   ",
               "                                   ",
               "               5                   ",
#ifdef BLAZERX
               "TO PLAY OVER A LAN, THE GAME USES  ",
               "IPX - THE SAME PROTOCOL CLASSIC DOS",
               "GAMES USED. NO PHONE NUMBER, COMM  ",
               "PORT, OR MODEM SETUP IS NEEDED.    ",
#else
               "TO PLAY MODEM-2-MODEM, YOU CAN     ",
               "EITHER DIAL UP A COMPETITOR OR WAIT",
               "FOR A COMPETITOR TO CALL. HOWEVER, ",
               "BEFORE YOU CAN DO THIS, YOU MUST   ",
               "SELECT WHICH COMMUNICATIONS PORT   ",
               "YOUR MODEM IS ON. THIS CAN BE DONE ",
               "WITH THE SELECT COMM PORT MENU     ",
               "OPTION.                            ",
#endif
               "                                   ",
#ifdef BLAZERX
               "ONE PLAYER PICKS WAIT FOR          ",
               "CONNECTION; THE OTHER PICKS MAKE   ",
               "CONNECTION. THE NUMBER PROMPT IS   ",
               "IGNORED - JUST PRESS ENTER.        ",
               "                                   ",
               "THE TWO SHIPS FIND EACH OTHER      ",
               "AUTOMATICALLY ON THE LAN AND YOU   ",
               "ARE WARPED INTO BATTLE. THE SELECT ",
               "COMM PORT OPTION IS NOT USED IN    ",
               "THIS MODE.                         ",
               "                                   ",
#else
               "ONCE YOU HAVE SELECTED THE COMM    ",
               "PORT THEN EITHER DIAL UP A         ",
               "COMPETITOR WITH THE MAKE CONNECTION",
               "MENU ITEM OR WAIT FOR A CALL VIA   ",
               "THE WAIT FOR CONNECTION MENU ITEM. ",
               "ONCE A CONNECTION HAS BEEN MADE YOU",
               "WILL BE WARPED INTO BATTLE.        ",
#endif
               "               6                   ",
               "CONTROLING THE SHIP                ",
               "                                   ",
               "NAVIGATION:                        ",
               "                                   ",
               "TO NAVIAGATE THE SHIP USE THE RIGHT",
               "AND LEFT ARROWS TO TURN. TO THRUST ",
               "FOWARD USE THE UP ARROW.           ",
               "                                   ",
               "SHIELDS:                           ",
               "                                   ",
               "TO ENGAGE YOUR SHIELDS PRESS THE   ",
               "<ALT> KEY. THEY WILL LAST FOR      ",
               "APPROXIMATELY 5 SECS BEFORE        ",
               "SHUTTING DOWN.                     ",
               "                                   ",
               "                                   ",
               "               7                   ",
               "CLOAKING DEVICE:                   ",
               "                                   ",
               "THE CLOAKING DEVICE CAN BE TOGGLED ",
               "BY PRESSING THE <C> KEY. CLOAKING  ",
               "EXHIBITS HIGH POWER CONSUMPTION AND",
               "YOUR SHIELDS ARE USELESS WHEN      ",
               "CLOAKED.                           ",
               "                                   ",
               "HEADS UP DISPLAY:                  ",
               "                                   ",
               "THE HEADS UP DISPLAY CAN BE TOGGLED",
               "WITH THE <H> KEY. IT DISPLAYS      ",
               "PERTAINENT TACTICAL INFORMATION IN ",
               "REAL-TIME.                         ",
               "                                   ",
               "                                   ",
               "               8                   ",
               "LONG RANGE SCANNERS:               ",
               "                                   ",
               "YOUR SHIP IS EQUIPPED WITH A LONG  ",
               "RANGE SECTOR SCAN THAT WILL DISPLAY",
               "ALL OBJECTS IN THE SECTOR. YOUR    ",
               "SHIP IS MARKED GREEN, THE ENEMY IS ",
               "MARKED RED. ALL OTHER OBJECTS ARE  ",
               "BLUE. ENGAGE AND DISENGAGE THE     ",
               "SCANNER BY PRESSING THE <S> KEY.   ",
               "                                   ",
               "WEAPONS:                           ",
               "                                   ",
               "TO FIRE YOUR CANNONS PRESS THE     ",
               "<SPACE> KEY. THEY SUPPORT THE ATF  ",
               "OPTION(ALL SHIPS BUILT AFTER       ",
               "2491.300 SUPPORT RAPID FIRE).      ",
               "               9                   ",
               "EXITING THE GAME:                  ",
               "                                   ",
               "THE GAME CAN BE EXITED BY PRESSING ",
               "<ESC> AT ANY TIME. IF THERE IS A   ",
               "CONNECTIONM IT WILL BE TERMINATED. ",
               "AFTER EXITING THE GAME, YOU WILL BE",
               "SENT BACK TO THE MAIN MENU SYSTEM. ",
               "                                   ",
               "WARNINGS:                          ",
               "                                   ",
               "YOUR SHIP CONSUMES A GREAT DEAL OF ",
               "POWER SINCE IT IS A FIGHTER. YOU   ",
               "CAN MINIMIZE THIS POWER CONSUMPTION",
               "BY USING YOUR WEAPONS,THRUSTER,AND ",
               "CLOAKING DEVICE SPARINGLY.         ",
               "                                   ",
               "               10                  ",
               "YOUR SHIELDS OPERATE BY A FUSION   ",
               "REACTION OF TRI-RUBIDIUM, THUS ONCE",
               "YOUR SHIELD STRENGTH IS DEPLETED,  ",
               "YOUR SHIELDS ARE INOPERABLE!       ",
               "                                   ",
               "THE ORIGINAL REPORTS OF THE SECTOR ",
               "NOTE LARGE ENERGY FLUX SOURCES     ",
               "FLOATING AROUND FROM TIME TO TIME. ",
               "IT MAY BE POSSIBLE TO EXTRACT THE  ",
               "ENERGY FROM THESE ENERGY PODS.     ",
               "                                   ",
               "                                   ",
               "                                   ",
               "                                   ",
               "                                   ",
               "                                   ",
               "               11                  ",
               "TO HELP YOUR SHIP FACILITATE THIS, ",
               "AN EXTERNAL ENERGION ACCEPTOR      ",
               "MATRIX HAS BEEN FITTED TO THE HULL ",
               "OF YOUR SHIP. YOU MAY BE ABLE TO   ",
               "ABSORB THE ENERGY OF THE PODS BY   ",
               "COMING IN CONTACT WITH THEM, BUT   ",
               "THIS IS THEORETICAL AND YOU DO SO  ",
               "AT YOUR OWN RISK.                  ",
               "                                   ",
               "GOOD HUNTING                       ",
               "MAY THE WIND BE AT YOUR BACK...    ",
               "                                   ",
               "                                   ",
               "                                   ",
               "                                   ",
               "                                   ",
               "          END BREIFING             "
};

int getLine(char* buffer) {
    // this function implements a crude line editor, it's used to input strings from keyboard
    int index = 0;
    int ch;

    // get the input string
    while (1) {
        // has user hit a key?
        if (kbhit()) {
            // make a sound
            digitalFxPlay(BLZKEY_VOC, 2);

            // get the key
            ch = getch();

            // test for a numeric character
            if (ch >= '0' && ch <= '9') {
                buffer[index] = ch;
                buffer[index + 1] = 0;

#ifdef BLAZERX
                fontEngine1(
                    DISPLAY_X + 6, DISPLAY_Y + 6 + 26,
                    0,
                    0,
                    buffer, VideoBuffer);
#else
#ifdef VBE_SUPPORT
                fontEngine1(
                    DISPLAY_X + 4, DISPLAY_Y + 4 + 16,
                    0,
                    0,
                    buffer, VideoBuffer);
#else
                fontEngine1(
                    DISPLAY_X + 2,
                    DISPLAY_Y + 2 + 8,
                    0,
                    0,
                    buffer,
                    VideoBuffer);
#endif
#endif

                // test if end of line reached
                if (++index == 12) {
                    index = 11;
                }
            } else if (ch == 13) {  // test for enter
                // user is done, so exit

                // null terminate
                buffer[index] = 0;

                return 1;
            } else if (ch == 8 || ch == 127) {  // back space or delete
                if (--index < 0) {
                    index = 0;
                }

                buffer[index] = ' ';
                buffer[index + 1] = 0;

#ifdef BLAZERX
                fontEngine1(
                    DISPLAY_X + 6, DISPLAY_Y + 6 + 26,
                    0,
                    0,
                    buffer, VideoBuffer);
#else
#ifdef VBE_SUPPORT
                fontEngine1(
                    DISPLAY_X + 4, DISPLAY_Y + 4 + 16,
                    0,
                    0,
                    buffer, VideoBuffer);
#else
                fontEngine1(
                    DISPLAY_X + 2,
                    DISPLAY_Y + 2 + 8,
                    0,
                    0,
                    buffer,
                    VideoBuffer);
#endif
#endif

                buffer[index] = 0;

                // erase the character
            } else if (ch == 27) {
                return 0;
            }
        }
    }
}

void initNovas(void) {
    // this function initializes all the super novas
    int indexN;
    int indexC;

    // process each nova
    for (indexN = 0; indexN < NUM_NOVAS; indexN++) {
        // set all novas to inactive
        Novas[indexN].state = NOVA_INACTIVE;
        Novas[indexN].lifetime = 0;

        // initialize each cinder
        for (indexC = 0; indexC < NUM_CINDERS; indexC++) {
            // clear out all fields
            Novas[indexN].cinders[indexC].x = 0;
            Novas[indexN].cinders[indexC].y = 0;

            Novas[indexN].cinders[indexC].xv = 0;
            Novas[indexN].cinders[indexC].yv = 0;

            Novas[indexN].cinders[indexC].sx = 0;
            Novas[indexN].cinders[indexC].sy = 0;

            Novas[indexN].cinders[indexC].color = 0;
#ifdef VBE_SUPPORT
            memset(Novas[indexN].cinders[indexC].backColor, 0,
                   sizeof(Novas[indexN].cinders[indexC].backColor));
#else
            Novas[indexN].cinders[indexC].backColor = 0;
#endif

            // set timing fields
            Novas[indexN].cinders[indexC].lifetime = 0;

            Novas[indexN].cinders[indexC].counter = 0;
            Novas[indexN].cinders[indexC].threshold = 0;
        }
    }
}

#ifdef BLAZERX
void cameraPixelUnder(int sx, int sy, unsigned long* back) {
    // a world pixel covers CAM_ZOOM x CAM_ZOOM screen pixels: save the screen
    // block under a pixel-sized particle so it can be erased later
    int cx, cy;

    for (cy = 0; cy < CAM_ZOOM; cy++) {
        for (cx = 0; cx < CAM_ZOOM; cx++) {
            back[cy * CAM_ZOOM + cx] = readPixelDb(sx + cx, sy + cy);
        }
    }
}

void cameraPixelDraw(int sx, int sy, int color) {
    // draw a pixel-sized particle as its CAM_ZOOM x CAM_ZOOM screen block
    int cx, cy;

    for (cy = 0; cy < CAM_ZOOM; cy++) {
        for (cx = 0; cx < CAM_ZOOM; cx++) {
            writePixelDb(sx + cx, sy + cy, color);
        }
    }
}

void cameraPixelErase(int sx, int sy, const unsigned long* back) {
    // restore the screen block saved by cameraPixelUnder
    int cx, cy;

    for (cy = 0; cy < CAM_ZOOM; cy++) {
        for (cx = 0; cx < CAM_ZOOM; cx++) {
            writePixelDb(sx + cx, sy + cy, back[cy * CAM_ZOOM + cx]);
        }
    }
}
#else
#ifdef VBE_SUPPORT
void cameraPixelUnder(int sx, int sy, unsigned char* back) {
    // a world pixel covers CAM_ZOOM x CAM_ZOOM screen pixels: save the screen
    // block under a pixel-sized particle so it can be erased later
    int cx, cy;

    for (cy = 0; cy < CAM_ZOOM; cy++) {
        for (cx = 0; cx < CAM_ZOOM; cx++) {
            back[cy * CAM_ZOOM + cx] = readPixelDb(sx + cx, sy + cy);
        }
    }
}

void cameraPixelDraw(int sx, int sy, int color) {
    // draw a pixel-sized particle as its CAM_ZOOM x CAM_ZOOM screen block
    int cx, cy;

    for (cy = 0; cy < CAM_ZOOM; cy++) {
        for (cx = 0; cx < CAM_ZOOM; cx++) {
            writePixelDb(sx + cx, sy + cy, color);
        }
    }
}

void cameraPixelErase(int sx, int sy, const unsigned char* back) {
    // restore the screen block saved by cameraPixelUnder
    int cx, cy;

    for (cy = 0; cy < CAM_ZOOM; cy++) {
        for (cx = 0; cx < CAM_ZOOM; cx++) {
            writePixelDb(sx + cx, sy + cy, back[cy * CAM_ZOOM + cx]);
        }
    }
}
#endif
#endif

void eraseNovas(void) {
    // this function replaces the what was under the novas
    int indexN;
    int indexC;
    ParticlePtr bits;

    // process each nova
    for (indexN = 0; indexN < NUM_NOVAS; indexN++) {
        // is this nova active?
        if (Novas[indexN].state == NOVA_ACTIVE) {
            bits = Novas[indexN].cinders;

            // process each cinder
            for (indexC = 0; indexC < NUM_CINDERS; indexC++) {
                // is this cinder visible?
                if (bits[indexC].visible) {
#ifdef VBE_SUPPORT
                    cameraPixelErase(
                        bits[indexC].sx,
                        bits[indexC].sy,
                        bits[indexC].backColor);
#else
                    writePixelDb(
                        bits[indexC].sx,
                        bits[indexC].sy,
                        bits[indexC].backColor);
#endif
                }
            }
        }
    }
}

void underNovas(void) {
    // this function scans what's under a nova
    int indexN;
    int indexC;
    int pxWindow;   // the starting position of the players window
    int pyWindow;
    int cx;
    int cy;
    ParticlePtr bits;

    // compute starting position of players window so screen mapping can be done
#ifdef VBE_SUPPORT
    pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
    pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;
#else
    pxWindow = PlayersX - 160 + 11;
    pyWindow = PlayersY - 100 + 9;
#endif

    // process each nova
    for (indexN = 0; indexN < NUM_NOVAS; indexN++) {
        // is this nova active?
        if (Novas[indexN].state == NOVA_ACTIVE) {
            bits = Novas[indexN].cinders;

            // process each cinder
            for (indexC = 0; indexC < NUM_CINDERS; indexC++) {
#ifdef VBE_SUPPORT
                cx = bits[indexC].x - pxWindow;
                cy = bits[indexC].y - pyWindow;

                // test if cinder is visible on screen?
                if (cx >= VIEW_WIDTH || cx < 0 || cy >= VIEW_HEIGHT || cy < 0) {
                    // this cinder is invisible and has been clipped
                    bits[indexC].visible = 0;

                    // process next cinder
                    continue;
                }

                // remap to screen coordinates
                cx = bits[indexC].sx = cx * CAM_ZOOM;
                cy = bits[indexC].sy = cy * CAM_ZOOM;

                // scan under cinder
                cameraPixelUnder(cx, cy, bits[indexC].backColor);
#else
                cx = bits[indexC].sx = bits[indexC].x - pxWindow;
                cy = bits[indexC].sy = bits[indexC].y - pyWindow;

                // test if cinder is visible on screen?
                if (cx >= 320 || cx < 0 || cy >= 200 || cy < 0) {
                    // this cinder is invisible and has been clipped
                    bits[indexC].visible = 0;

                    // process next cinder
                    continue;
                }

                // scan under cinder
                bits[indexC].backColor = readPixelDb(cx, cy);
#endif

                // set visibility flag
                bits[indexC].visible = 1;
            }
        }
    }
}
#ifdef BLAZERX

// the cinder fire gradient CINDER_START_COLOR..CINDER_END_COLOR steps through,
// pale red fading to dark orange
static const unsigned long CinderColors[16] = {
    RGB32(255,218,218), RGB32(255,186,186), RGB32(255,159,159), RGB32(255,127,127),
    RGB32(255, 95, 95), RGB32(255, 64, 64), RGB32(255, 32, 32), RGB32(255,  0,  0),
    RGB32(252,168, 92), RGB32(252,152, 64), RGB32(252,136, 32), RGB32(252,120,  0),
    RGB32(228,108,  0), RGB32(204, 96,  0), RGB32(180, 84,  0), RGB32(156, 76,  0),
};
#endif

void drawNovas(void) {
    // this function draws the novas
    int indexN;
    int indexC;
    ParticlePtr bits;

    // process each nova
    for (indexN = 0; indexN < NUM_NOVAS; indexN++) {
        // is this nova active?
        if (Novas[indexN].state == NOVA_ACTIVE) {
            bits = Novas[indexN].cinders;

            // process each cinder
            for (indexC = 0; indexC < NUM_CINDERS; indexC++) {
                // is this cinder visible?
                if (bits[indexC].visible) {
#ifdef BLAZERX
                    cameraPixelDraw(
                        bits[indexC].sx,
                        bits[indexC].sy,
                        CinderColors[bits[indexC].color]);
#else
#ifdef VBE_SUPPORT
                    cameraPixelDraw(
                        bits[indexC].sx,
                        bits[indexC].sy,
                        bits[indexC].color);
#else
                    writePixelDb(
                        bits[indexC].sx,
                        bits[indexC].sy,
                        bits[indexC].color);
#endif
#endif
                }
            }
        }
    }
}

void moveNovas(void) {
    // this function draws the novas
    int indexN;
    int indexC;
    ParticlePtr bits;

    // process each nova
    for (indexN = 0; indexN < NUM_NOVAS; indexN++) {
        // is this nova active?
        if (Novas[indexN].state == NOVA_ACTIVE) {
            bits = Novas[indexN].cinders;

            // process each cinder
            for (indexC = 0; indexC < NUM_CINDERS; indexC++) {
                // move the cinder
                bits[indexC].x += bits[indexC].xv;
                bits[indexC].y += bits[indexC].yv;

                // animate the cinder color
                if (++bits[indexC].counter > bits[indexC].threshold) {
                    // decrement color
                    if (++bits[indexC].color >= CINDER_END_COLOR) {
                        bits[indexC].color = CINDER_END_COLOR;
                    }

                    // reset counter
                    bits[indexC].counter = 0;
                }
            }

            // age the nova
            if (--Novas[indexN].lifetime <= 0) {
                Novas[indexN].state = NOVA_INACTIVE;
            }
        }
    }
}

void startNova(int x, int y) {
    // this function starts a super nova explosion
    int indexC;
    int indexN;

    // hunt for an inactive nova
    for (indexN = 0; indexN < NUM_NOVAS; indexN++) {
        // test for inactive nova
        if (Novas[indexN].state == NOVA_INACTIVE) {
            // activate nova
            Novas[indexN].state = NOVA_ACTIVE;

            // set lifetime in frames
            Novas[indexN].lifetime = 50 + rand() % 20;

            // initialize each cinder or restart a single cinder
            for (indexC = 0; indexC < NUM_CINDERS; indexC++) {
                // fill in position, velocity and color of cinder
                Novas[indexN].cinders[indexC].x = x;
                Novas[indexN].cinders[indexC].y = y;
                Novas[indexN].cinders[indexC].sx = 0;
                Novas[indexN].cinders[indexC].sy = 0;
                Novas[indexN].cinders[indexC].xv = -8 + rand() % 16;
                Novas[indexN].cinders[indexC].yv = -8 + rand() % 16;

                Novas[indexN].cinders[indexC].color = CINDER_START_COLOR;
#ifdef VBE_SUPPORT
                memset(Novas[indexN].cinders[indexC].backColor, 0,
                       sizeof(Novas[indexN].cinders[indexC].backColor));
#else
                Novas[indexN].cinders[indexC].backColor = 0;
#endif

                // set timing fields
                Novas[indexN].cinders[indexC].counter = 0;
                Novas[indexN].cinders[indexC].threshold = 2 + rand() % 6;
            }

            // make sound
            digitalFxPlay(BLZEXP2_VOC, 0);

            // break out of for loop
            break;
        }
    }
}

void lineH2(int x1, int x2, int y, int color, unsigned char FAR* dest) {
#ifdef BLAZERX
    // draw a horizontal line into an arbitrary buffer (VideoBuffer or
    // DoubleBuffer). black3/black4 only provide lineH for VideoBuffer
    // itself, with no buffer-generic equivalent, so this writes pixels
    // directly using the current mode's pitch, same as lineH internally.
    int x, temp, bytespp = DisplayBpp / 8;
    unsigned char FAR* p;

    if (x1 > x2) {
        temp = x1;
        x1 = x2;
        x2 = temp;
    }
    p = dest + (unsigned long)y * DisplayPitch + (unsigned long)x1 * bytespp;

    for (x = x1; x <= x2; x++) {
        switch (DisplayBpp) {
            case 8:  *p = (unsigned char)color; break;
            case 16: *(unsigned short FAR*)p = (unsigned short)color; break;
            default: *(unsigned long FAR*)p  = (unsigned long)color; break;   // 32
        }
        p += bytespp;
    }
#else
#ifdef VBE_SUPPORT
    // draw a horizontal line to the destination buffer (always DoubleBuffer
    // in this file) - black3/black4 only expose a generic-buffer HLine at
    // the pixel level (writePixelDb), not a run-fill one, so this fills the
    // row directly the way the book's own lineH2 always has
    MEMSET(
        (char FAR*)(dest + (long)y * DisplayPitch + x1),
        (unsigned char)color,
        x2 - x1 + 1);
#else
    // draw a horizontal line to the destination buffer
    MEMSET(
        (char FAR*)(dest + ((y << 8) + (y << 6)) + x1),
        (unsigned char)color,
        x2 - x1 + 1);
#endif
#endif
}

void lineV2(int y1, int y2, int x, int color, unsigned char FAR* dest) {
#ifdef BLAZERX
    // draw a vertical line into an arbitrary buffer - see lineH2
    int y, temp, bytespp = DisplayBpp / 8;
    unsigned char FAR* p;
#else
    // draw a vertical line to destination buffer
    unsigned char FAR* startOffset; // starting memory offset of line
    int index;
#endif

#ifdef BLAZERX
    if (y1 > y2) {
        temp = y1;
        y1 = y2;
        y2 = temp;
    }
    p = dest + (unsigned long)y1 * DisplayPitch + (unsigned long)x * bytespp;
#else
    // compute starting position
#ifdef VBE_SUPPORT
    startOffset = dest + (long)y1 * DisplayPitch + x;
#else
    startOffset = dest + ((y1 << 8) + (y1 << 6)) + x;
#endif
#endif

#ifdef BLAZERX
    for (y = y1; y <= y2; y++) {
        switch (DisplayBpp) {
            case 8:  *p = (unsigned char)color; break;
            case 16: *(unsigned short FAR*)p = (unsigned short)color; break;
            default: *(unsigned long FAR*)p  = (unsigned long)color; break;   // 32
        }
        p += DisplayPitch;
#else
    for (index = 0; index <= y2 - y1; index++) {
        // set the pixel
        *startOffset = (unsigned char)color;

        // move downward to next line
#ifdef VBE_SUPPORT
        startOffset += DisplayPitch;
#else
        startOffset += 320;
#endif
#endif
    }
}

#ifdef VBE_SUPPORT
void lineH2Thick(int x1, int x2, int y, int thickness, int color, unsigned char FAR* dest) {
    // draw a horizontal line thickness rows tall, centered on y
    int i;
    for (i = 0; i < thickness; i++) {
        lineH2(x1, x2, y - thickness / 2 + i, color, dest);
    }
}

void lineV2Thick(int y1, int y2, int x, int thickness, int color, unsigned char FAR* dest) {
    // draw a vertical line thickness columns wide, centered on x
    int i;
    for (i = 0; i < thickness; i++) {
        lineV2(y1, y2, x - thickness / 2 + i, color, dest);
    }
}
#endif

void initStars(void) {
    // this function initializes all the stars in the star field
    int index;

    for (index = 0; index < NUM_STARS; index++) {
        // select plane that star will be in
        switch (rand() % 3) {
            case STAR_PLANE_0: {
                Stars[index].color = STAR_COLOR_0;
                Stars[index].plane = STAR_PLANE_0;
            } break;

            case STAR_PLANE_1: {
                Stars[index].color = STAR_COLOR_1;
                Stars[index].plane = STAR_PLANE_1;
            } break;

            case STAR_PLANE_2: {
                Stars[index].color = STAR_COLOR_2;
                Stars[index].plane = STAR_PLANE_2;
            } break;

            default:
                break;
        }

#ifdef VBE_SUPPORT
        // set fields that aren't plane specific (in view units, same scale as
        // VIEW_WIDTH/VIEW_HEIGHT - screen position is only computed at draw time)
        Stars[index].x = rand() % VIEW_WIDTH;  // change this latter to reflect clipping region
        Stars[index].y = rand() % VIEW_HEIGHT;
        memset(Stars[index].backColor, 0, sizeof(Stars[index].backColor));
#else
        // set fields that aren't plane specific
        Stars[index].x = rand() % 320;  // change this latter to reflect clipping region
        Stars[index].y = rand() % 200;
        Stars[index].backColor = 0;
#endif
    }
}

void moveStars(void) {
    // this function moves the star field, note that the star field is always
    // in screen coordinates, otherwise, we would need thousands of stars to
    // fill up the universe instead of 50!
    int index,
        starX,
        starY,
        plane0Dx,
        plane0Dy,
        plane1Dx,
        plane1Dy,
        plane2Dx,
        plane2Dy;

    // pre-compute plane translations
    plane0Dx = PlayersDx >> 2;
    plane0Dy = PlayersDy >> 2;

    plane1Dx = PlayersDx >> 1;
    plane1Dy = PlayersDy >> 1;

    plane2Dx = PlayersDx;
    plane2Dy = PlayersDy;

    // move all the stars based on the motion of the player
    for (index = 0; index < NUM_STARS; index++) {
        // locally cache star position to speed up calculations
        starX = Stars[index].x;
        starY = Stars[index].y;

        // test which star field star is in so it is translated with correct perspective
        switch (Stars[index].plane) {
            case STAR_PLANE_0: {
                // move the star based on differential motion of player
                // far plane is divided by 4
                starX += plane0Dx;
                starY += plane0Dy;
            } break;

            case STAR_PLANE_1: {
                // move the star based on differential motion of player
                // middle plane is divided by 2
                starX += plane1Dx;
                starY += plane1Dy;
            } break;

            case STAR_PLANE_2: {
                // move the star based on differential motion of player
                // near plane is divided by 1
                starX += plane2Dx;
                starY += plane2Dy;
            } break;
        }

        // test if a star has flown off an edge
#ifdef VBE_SUPPORT
        if (starX >= VIEW_WIDTH) {
            starX = starX - VIEW_WIDTH;
        } else if (starX < 0) {
            starX = VIEW_WIDTH + starX;
        }

        if (starY >= VIEW_HEIGHT) {
            starY = starY - VIEW_HEIGHT;
        } else if (starY < 0) {
            starY = VIEW_HEIGHT + starY;
        }
#else
        if (starX >= 320) {
            starX = starX - 320;
        } else if (starX < 0) {
            starX = 320 + starX;
        }

        if (starY >= 200) {
            starY = starY - 200;
        } else if (starY < 0) {
            starY = 200 + starY;
        }
#endif

        // reset stars position in structure
        Stars[index].x = starX;
        Stars[index].y = starY;
    }
}

void drawStars(void) {
    // this function draws all the stars
    int index;

    for (index = 0; index < NUM_STARS; index++) {
#ifdef VBE_SUPPORT
        cameraPixelDraw(Stars[index].x * CAM_ZOOM, Stars[index].y * CAM_ZOOM, Stars[index].color);
#else
        writePixelDb(Stars[index].x, Stars[index].y, Stars[index].color);
#endif
    }
}

void eraseStars(void) {
    // this function erases all the stars
    int index;

    for (index = 0; index < NUM_STARS; index++) {
#ifdef VBE_SUPPORT
        cameraPixelErase(Stars[index].x * CAM_ZOOM, Stars[index].y * CAM_ZOOM, Stars[index].backColor);
#else
        writePixelDb(Stars[index].x, Stars[index].y, Stars[index].backColor);
#endif
    }
}

void underStars(void) {
    // this function scans under each star
    int index;

    for (index = 0; index < NUM_STARS; index++) {
#ifdef VBE_SUPPORT
        cameraPixelUnder(Stars[index].x * CAM_ZOOM, Stars[index].y * CAM_ZOOM, Stars[index].backColor);
#else
        Stars[index].backColor = readPixelDb(Stars[index].x, Stars[index].y);
#endif
    }
}

void initAsteroids(int small, int medium, int large) {
    // this function loads in the imagery for the asteroids, allocates all the
    // memory for them, sets up all the fields and starts them at random positions
    int index;
    int frame;

    static int firstTime = 1;

    // initialize all the large asteroid sprites
    for (index = START_LARGE_ASTEROIDS; index <= END_LARGE_ASTEROIDS; index++) {
        if (firstTime) {
            spriteInit(
                &Asteroids[index].rock,
                0,
                0,
#ifdef VBE_SUPPORT
                ASTEROID_LARGE_WIDTH * CAM_ZOOM,
                ASTEROID_LARGE_HEIGHT * CAM_ZOOM,
#else
                ASTEROID_LARGE_WIDTH,
                ASTEROID_LARGE_HEIGHT,
#endif
                0,
                0,
                0,
                0,
                0,
                0);
#ifdef BLAZERX
            Asteroids[index].rock.transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif
        }

        // set position, velocity and type fields
        Asteroids[index].xv   = -8 + rand() % 16;
        Asteroids[index].yv   = -8 + rand() % 16;
        Asteroids[index].x    = rand() % UNIVERSE_WIDTH;
        Asteroids[index].y    = rand() % UNIVERSE_HEIGHT;
        Asteroids[index].type = ASTEROID_LARGE;
        Asteroids[index].rock.state = ASTEROID_INACTIVE;

        // use this to control rotation rate
        Asteroids[index].rock.threshold1 = 1 + rand() % 3;
    }

    // initialize all the medium asteroid sprites
    for (index = START_MEDIUM_ASTEROIDS; index <= END_MEDIUM_ASTEROIDS; index++) {
        if (firstTime) {
            spriteInit(
                &Asteroids[index].rock,
                0,
                0,
#ifdef VBE_SUPPORT
                ASTEROID_MEDIUM_WIDTH * CAM_ZOOM,
                ASTEROID_MEDIUM_HEIGHT * CAM_ZOOM,
#else
                ASTEROID_MEDIUM_WIDTH,
                ASTEROID_MEDIUM_HEIGHT,
#endif
                0,
                0,
                0,
                0,
                0,
                0);
#ifdef BLAZERX
            Asteroids[index].rock.transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif
        }

        // set velocity and type fields
        Asteroids[index].xv         = -6 + rand() % 12;
        Asteroids[index].yv         = -6 + rand() % 12;
        Asteroids[index].x          = rand() % UNIVERSE_WIDTH;
        Asteroids[index].y          = rand() % UNIVERSE_HEIGHT;
        Asteroids[index].type       = ASTEROID_MEDIUM;
        Asteroids[index].rock.state = ASTEROID_INACTIVE;

        // use this to control rotation rate
        Asteroids[index].rock.threshold1 = 1 + rand() % 3;
    }

    // initialize all the small asteroid sprites
    for (index = START_SMALL_ASTEROIDS; index <= END_SMALL_ASTEROIDS; index++) {
        if (firstTime) {
            spriteInit(
                &Asteroids[index].rock,
                0,
                0,
#ifdef VBE_SUPPORT
                ASTEROID_SMALL_WIDTH * CAM_ZOOM,
                ASTEROID_SMALL_HEIGHT * CAM_ZOOM,
#else
                ASTEROID_SMALL_WIDTH,
                ASTEROID_SMALL_HEIGHT,
#endif
                0,
                0,
                0,
                0,
                0,
                0);
#ifdef BLAZERX
            Asteroids[index].rock.transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif
        }

        // set velocity and type fields
        Asteroids[index].xv         = -4 + rand() % 8;
        Asteroids[index].yv         = -4 + rand() % 8;
        Asteroids[index].x          = rand() % UNIVERSE_WIDTH;
        Asteroids[index].y          = rand() % UNIVERSE_HEIGHT;
        Asteroids[index].type       = ASTEROID_SMALL;
        Asteroids[index].rock.state = ASTEROID_INACTIVE;

        // use this to control rotation rate
        Asteroids[index].rock.threshold1 = 1 + rand() % 3;
    }

    // now load the imagery for the large asteroids
    if (firstTime) {
        pcxInit(&ImagePcx);
        pcxLoad("blazelas.pcx", &ImagePcx, 1);

        // extract the bitmaps for the asteroid, there are 8 animation cells
        for (index = 0; index < NUM_ASTEROID_FRAMES; index++) {
            pcxGetSprite(
                &ImagePcx,
                &Asteroids[START_LARGE_ASTEROIDS].rock,
                index,
                index,
                0);
        }

        // now alias pointers of remaining asteroids sprites to same data that
        // was allocated for the first large asteroid, this saves a lot of memory!
        for (index = START_LARGE_ASTEROIDS + 1; index <= END_LARGE_ASTEROIDS; index++) {
            // alias all the frame image pointer of this asteroid to the frames
            // of the first asteroid, no need to repliate this data in memory!
            for (frame = 0; frame < NUM_ASTEROID_FRAMES; frame++) {
                // the image frames are within the sprite which is called rock which
                // is within the asteroid structure
                Asteroids[index].rock.frames[frame] =
                    Asteroids[START_LARGE_ASTEROIDS].rock.frames[frame];
            }

            // set number of frames field
            Asteroids[index].rock.numFrames =
                Asteroids[START_LARGE_ASTEROIDS].rock.numFrames;
        }

        // delete the pcx file
        pcxDelete(&ImagePcx);

        // now load the imagery for the medium asteroids
        pcxInit(&ImagePcx);
        pcxLoad("blazemas.pcx", &ImagePcx, 1);

        // extract the bitmaps for the asteroid, there are 8 animation cells
        for (index = 0; index < NUM_ASTEROID_FRAMES; index++) {
            pcxGetSprite(
                &ImagePcx,
                &Asteroids[START_MEDIUM_ASTEROIDS].rock,
                index,
                index,
                0);
        }

        // now alias pointers of remaining asteroids sprites to same data that
        // was allocated for the first medium asteroid, this saves a lot of memory!
        for (index = START_MEDIUM_ASTEROIDS + 1; index <= END_MEDIUM_ASTEROIDS; index++) {
            // alias all the frame image pointer of this asteroid to the frames
            // of the first asteroid, no need to replicate this data in memory!
            for (frame = 0; frame < NUM_ASTEROID_FRAMES; frame++) {
                // the image frames are within the sprite which is called rock which
                // is within the asteroid structure
                Asteroids[index].rock.frames[frame] =
                    Asteroids[START_MEDIUM_ASTEROIDS].rock.frames[frame];
            }

            // set number of frames field
            Asteroids[index].rock.numFrames =
                Asteroids[START_MEDIUM_ASTEROIDS].rock.numFrames;
        }

        // delete the pcx file
        pcxDelete(&ImagePcx);

        // finally load the imagery for the small asteroids
        pcxInit(&ImagePcx);
        pcxLoad("blazesas.pcx", &ImagePcx, 1);

        // extract the bitmaps for the asteroid, there are 8 animation cells
        for (index = 0; index < NUM_ASTEROID_FRAMES; index++) {
            pcxGetSprite(
                &ImagePcx,
                &Asteroids[START_SMALL_ASTEROIDS].rock,
                index,
                index,
                0);
        }

        // now alias pointers of remaining asteroids sprites to same data that
        // was allocated for the first small asteroid, this saves a lot of memory!
        for (index = START_SMALL_ASTEROIDS + 1; index <= END_SMALL_ASTEROIDS; index++) {
            // alias all the frame image pointer of this asteroid to the frames
            // of the first asteroid, no need to replicate this data in memory!
            for (frame = 0; frame < NUM_ASTEROID_FRAMES; frame++) {
                // the image frames are within the sprite which is called rock which
                // is within the asteroid structure
                Asteroids[index].rock.frames[frame] =
                    Asteroids[START_SMALL_ASTEROIDS].rock.frames[frame];
            }

            // set number of frames field
            Asteroids[index].rock.numFrames =
                Asteroids[START_SMALL_ASTEROIDS].rock.numFrames;
        }

        // delete the pcx file
        pcxDelete(&ImagePcx);
    }

    // now start up the requested number of asteroids

    // first the large
    for (index = 0; index < large; index++) {
        // look for inactive asteroids to start up
        if (Asteroids[index + START_LARGE_ASTEROIDS].rock.state == ASTEROID_INACTIVE) {
            Asteroids[index + START_LARGE_ASTEROIDS].rock.state = ASTEROID_ACTIVE;
        }
    }

    // now the medium
    for (index = 0; index < medium; index++) {
        // look for inactive asteroids to start up
        if (Asteroids[index + START_MEDIUM_ASTEROIDS].rock.state == ASTEROID_INACTIVE) {
            Asteroids[index + START_MEDIUM_ASTEROIDS].rock.state = ASTEROID_ACTIVE;
        }
    }

    // finally the small
    for (index = 0; index < small; index++) {
        // look for inactive asteroids to start up
        if (Asteroids[index + START_SMALL_ASTEROIDS].rock.state == ASTEROID_INACTIVE) {
            Asteroids[index + START_SMALL_ASTEROIDS].rock.state = ASTEROID_ACTIVE;
        }
    }

    // test if this was the first time the function was called
    if (firstTime) {
        firstTime = 0;
    }

    // what a pain!
}

void startAsteroid(int x, int y, int type) {
    // this function is used to start up an asteroid at the sent position
    // later possible implement velocity?
    int index;

    // which kind of asteroid is being requested?
    switch (type) {
        case ASTEROID_LARGE: {
            // scan for inactive asteroid
            for (index = START_LARGE_ASTEROIDS; index <= END_LARGE_ASTEROIDS; index++) {
                // is this asteroid being used?
                if (Asteroids[index].rock.state == ASTEROID_INACTIVE) {
                    // set fields of asteroid and return
                    Asteroids[index].xv         = -8 + rand() % 16;
                    Asteroids[index].yv         = -8 + rand() % 16;
                    Asteroids[index].x          = x;
                    Asteroids[index].y          = y;
                    Asteroids[index].rock.state = ASTEROID_ACTIVE;
                    return;
                }
            }
        } break;

        case ASTEROID_MEDIUM: {
            // scan for inactive asteroid
            for (index = START_MEDIUM_ASTEROIDS; index <= END_MEDIUM_ASTEROIDS; index++) {
                // is this asteroid being used?
                if (Asteroids[index].rock.state == ASTEROID_INACTIVE) {
                    // set fields of asteroid and return
                    Asteroids[index].xv         = -6 + rand() % 12;
                    Asteroids[index].yv         = -6 + rand() % 12;
                    Asteroids[index].x          = x;
                    Asteroids[index].y          = y;
                    Asteroids[index].rock.state = ASTEROID_ACTIVE;
                    return;
                }
            }
        } break;

        case ASTEROID_SMALL: {
            // scan for inactive asteroid 
            for (index = START_SMALL_ASTEROIDS; index <= END_SMALL_ASTEROIDS; index++) {
                // is this asteroid being used?
                if (Asteroids[index].rock.state == ASTEROID_INACTIVE) {
                    // set fields of asteroid and return
                    Asteroids[index].xv         = -4 + rand() % 8;
                    Asteroids[index].yv         = -4 + rand() % 8;
                    Asteroids[index].x          = x;
                    Asteroids[index].y          = y;
                    Asteroids[index].rock.state = ASTEROID_ACTIVE;
                    return;
                }
            }
        } break;
    }
}

void eraseAsteroids(void) {
    // this function traverses the asteroid list and erases all asteroids that are active
    int index;

    // erase all asteroids that are within screen window
    for (index = 0; index < NUM_ASTEROIDS; index++) {
        // test if this asteroids is active
        if (Asteroids[index].rock.state == ASTEROID_ACTIVE) {
            spriteEraseClip(&Asteroids[index].rock, DoubleBuffer);
        }
    }
}

void drawAsteroids(void) {
    // this function traverses the asteroid list and draws all asteroids that are active
    int index;

    for (index = 0; index < NUM_ASTEROIDS; index++) {
        // test if this asteroid is active
        if (Asteroids[index].rock.state == ASTEROID_ACTIVE) {
            spriteDrawClip(&Asteroids[index].rock, DoubleBuffer, 1);
        }
    }
}

void underAsteroids(void) {
    // this function traverses the asteroid list and scans under all asteroids that
    // are active, note that this function is the only one that computes the screen
    // coordinates of the asteroid sprites, placing the computation in the other
    // functions would be redundant, hence the sprite coordinates at this function
    // are undefined, and after this function they have been remapped to the video
    // screen relative to the player's position.
    int index,
        pxWindow,   // the starting position of the player's window
        pyWindow;

    // compute starting position of player's window so screen mapping can be done
#ifdef VBE_SUPPORT
    pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
    pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;
#else
    pxWindow = PlayersX - 160 + 11;
    pyWindow = PlayersY - 100 + 9;
#endif

    // now scan under all asteroids
    for (index = 0; index < NUM_ASTEROIDS; index++) {
        // test if this asteroid is active
        if (Asteroids[index].rock.state == ASTEROID_ACTIVE) {
            // position asteroid correctly on view screen, note this is very similar
            // to what we will do in 3-D when we translate all the objects in the
            // universe to the viewer position
#ifdef VBE_SUPPORT
            Asteroids[index].rock.x = (Asteroids[index].x - pxWindow) * CAM_ZOOM;
            Asteroids[index].rock.y = (Asteroids[index].y - pyWindow) * CAM_ZOOM;
#else
            Asteroids[index].rock.x = Asteroids[index].x - pxWindow;
            Asteroids[index].rock.y = Asteroids[index].y - pyWindow;
#endif

            spriteUnderClip(&Asteroids[index].rock, DoubleBuffer);
        }
    }
}

void moveAsteroids(void) {
    // this function traverses the asteroid list and moves and tests for collisions
    // note that the sprite positions of the asteroids are not touched only the
    // universe or "world" coordinates are
    int index,
        astX,
        astY;

    // process each asteroid
    for (index = 0; index < NUM_ASTEROIDS; index++) {
        // test if this asteroid is active
        if (Asteroids[index].rock.state == ASTEROID_ACTIVE) {
            // move the asteroid
            astX = Asteroids[index].x;
            astY = Asteroids[index].y;

            astX += Asteroids[index].xv;
            astY += Asteroids[index].yv;

            // test if asteroid is off screen boundary
            if (astX > UNIVERSE_WIDTH + UNIVERSE_BORDER) {
                astX = -UNIVERSE_BORDER;
            } else if (astX < -UNIVERSE_BORDER) {
                astX = UNIVERSE_WIDTH + UNIVERSE_BORDER;
            }

            if (astY > UNIVERSE_HEIGHT + UNIVERSE_BORDER) {
                astY = -UNIVERSE_BORDER;
            } else if (astY < -UNIVERSE_BORDER) {
                astY = UNIVERSE_HEIGHT + UNIVERSE_BORDER;
            }

            // restore variables in structure
            Asteroids[index].x = astX;
            Asteroids[index].y = astY;

            // animate asteroid
            if (++Asteroids[index].rock.counter1 >= Asteroids[index].rock.threshold1) {
                // reset counter
                Asteroids[index].rock.counter1 = 0;

                if (++Asteroids[index].rock.currFrame >= NUM_ASTEROID_FRAMES) {
                    Asteroids[index].rock.currFrame = 0;
                }

                // test if asteroid has hit player

                // test for collision
                if (PlayersX + SHIP_WIDTH / 2  >= astX &&
                    PlayersY + SHIP_HEIGHT / 2 >= astY &&
#ifdef VBE_SUPPORT
                    PlayersX + SHIP_WIDTH / 2  <= astX + Asteroids[index].rock.width / CAM_ZOOM &&
                    PlayersY + SHIP_HEIGHT / 2 <= astY + Asteroids[index].rock.height / CAM_ZOOM) {
#else
                    PlayersX + SHIP_WIDTH / 2  <= astX + Asteroids[index].rock.width &&
                    PlayersY + SHIP_HEIGHT / 2 <= astY + Asteroids[index].rock.height) {
#endif

                    // kill the asteroid and the missile
                    Asteroids[index].rock.state = ASTEROID_INACTIVE;

                    // what kind of asteroid did we have?
                    switch (Asteroids[index].type) {
                        case ASTEROID_LARGE: {
                            // start one medium and one small asteroid (if possible)
                            startAsteroid(
                                Asteroids[index].x,
                                Asteroids[index].y,
                                ASTEROID_MEDIUM);

                            startAsteroid(
                                Asteroids[index].x + ASTEROID_LARGE_WIDTH / 2,
                                Asteroids[index].y + ASTEROID_LARGE_HEIGHT / 2,
                                ASTEROID_SMALL);
                        } break;

                        case ASTEROID_MEDIUM: {
                            // start two small asteroids (if possible)
                            startAsteroid(
                                Asteroids[index].x,
                                Asteroids[index].y,
                                ASTEROID_SMALL);

                            startAsteroid(
                                Asteroids[index].x + ASTEROID_MEDIUM_WIDTH / 2,
                                Asteroids[index].y + ASTEROID_MEDIUM_HEIGHT / 2,
                                ASTEROID_SMALL);
                        } break;

                        case ASTEROID_SMALL: {
                            // start a randomly positioned asteroid of any size at the worm hole
                            startAsteroid(WORMHOLE_X, WORMHOLE_Y, rand() % 3);
                        } break;

                        default:
                            break;
                    }

                    // were shields up?
                    if (PlayersShields) {
                        // start explosion
                        startExplosion(PlayersX, PlayersY, 2);
                    } else {
                        // say bye bye!
                        startExplosion(PlayersX, PlayersY, 2);
                        startNova(PlayersX + SHIP_WIDTH / 2, PlayersY + SHIP_HEIGHT / 2);

                        // start players death sequence
                        startPlayersDeath();
                    }
                }

                // test for collision
#ifdef BLAZERX
                if (Linked || AiEnabled) {
#else
                if (Linked) {
#endif
                    if (RemotesX + SHIP_WIDTH / 2  >= astX &&
                        RemotesY + SHIP_HEIGHT / 2 >= astY &&
#ifdef VBE_SUPPORT
                        RemotesX + SHIP_WIDTH / 2  <= astX + Asteroids[index].rock.width / CAM_ZOOM &&
                        RemotesY + SHIP_HEIGHT / 2 <= astY + Asteroids[index].rock.height / CAM_ZOOM) {
#else
                        RemotesX + SHIP_WIDTH / 2  <= astX + Asteroids[index].rock.width &&
                        RemotesY + SHIP_HEIGHT / 2 <= astY + Asteroids[index].rock.height) {
#endif

                        // kill the asteroid and the missile
                        Asteroids[index].rock.state = ASTEROID_INACTIVE;

                        // what kind of asteroid did we have?
                        switch (Asteroids[index].type) {
                            case ASTEROID_LARGE: {
                                // start one medium and one small asteroid (if possible)
                                startAsteroid(
                                    Asteroids[index].x,
                                    Asteroids[index].y,
                                    ASTEROID_MEDIUM);

                                startAsteroid(
                                    Asteroids[index].x + ASTEROID_LARGE_WIDTH / 2,
                                    Asteroids[index].y + ASTEROID_LARGE_HEIGHT / 2,
                                    ASTEROID_SMALL);
                            } break;

                            case ASTEROID_MEDIUM: {
                                // start two small asteroids (if possible)
                                startAsteroid(
                                    Asteroids[index].x,
                                    Asteroids[index].y,
                                    ASTEROID_SMALL);

                                startAsteroid(
                                    Asteroids[index].x + ASTEROID_MEDIUM_WIDTH / 2,
                                    Asteroids[index].y + ASTEROID_MEDIUM_HEIGHT / 2,
                                    ASTEROID_SMALL);
                            } break;

                            case ASTEROID_SMALL: {
                                // start a randomly positioned asteroid of any size at the worm hole
                                startAsteroid(WORMHOLE_X, WORMHOLE_Y, rand() % 3);
                            } break;

                            default:
                                break;
                        }

                        // were shields up?
                        if (RemotesShields) {
                            // start explosion
                            startExplosion(RemotesX, RemotesY, 2);
                        } else {
                            // say bye bye!
                            startExplosion(RemotesX, RemotesY, 2);
                            startNova(RemotesX + SHIP_WIDTH / 2, RemotesY + SHIP_HEIGHT / 2);

                            // start players death sequence
                            startRemotesDeath();
                        }
                    }
                }
            }
        }
    }
}

void techPrint(int x, int y, char* string, unsigned char FAR* destination) {
    // this function is used to print text out like a teletypwriter, it looks cool, trust me!
    int length,
        index;
    char buffer[3];

    // compute length of input string
    length = strlen(string);

    // print the string out a character at a time
    for (index = 0; index < length; index++) {
        // the first character is the actual printable character
        buffer[0] = string[index];

        // this is a little cursor kind of thing
        buffer[1] = '<';

        // null terminate
        buffer[2] = 0;

        // print the string
        fontEngine1(x, y, 0, 0, buffer, destination);

        // move to next position
#ifdef BLAZERX
        x += TECH_FONT_WIDTH + 3;
#else
#ifdef VBE_SUPPORT
        x += TECH_FONT_WIDTH + 2;
#else
        x += TECH_FONT_WIDTH + 1;
#endif
#endif

        // wait a bit 1/70th of a second
        waitForVerticalRetrace();

        // clear the cursor
    }

    // clear the cursor
    buffer[0] = ' ';
    buffer[1] = ' ';
    buffer[2] = 0;

    fontEngine1(x, y, 0, 0, buffer, destination);

    // done!
}

void fontEngine1(int x, int y, int font, int color, char* string, unsigned char FAR* destination) {
    // this function prints a string out using one of the graphics fonts that
    // we have drawn, note this first version doesn't use the font field, but
    // we'll throw it in to keep the interface open for future version
    static int fontLoaded = 0;  // this is used to track the first time the function is loaded

    int index,
        cIndex,
        length;

    if (!fontLoaded) {
        // load the 4x7 tech font
        pcxInit(&ImagePcx);
        pcxLoad("blazefnt.pcx", &ImagePcx, 1);

        // allocate memory for each bitmap and load character
        for (index = 0; index < NUM_TECH_FONT; index++) {
            // allocate memory for character
            bitmapAllocate(&TechFont[index], TECH_FONT_WIDTH, TECH_FONT_HEIGHT);

            // set size of character
            TechFont[index].width = TECH_FONT_WIDTH;
            TechFont[index].height = TECH_FONT_HEIGHT;

            // extract bitmap from PCX buffer
            TechFont[index].x = 1 + (index % 16) * (TECH_FONT_WIDTH + 1);
            TechFont[index].y = 1 + (index / 16) * (TECH_FONT_HEIGHT + 1);

            bitmapGet(&TechFont[index], &ImagePcx);
        }

        // font is loaded, delete pcx file and set flag
        pcxDelete(&ImagePcx);

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
            bitmapPut(&TechFont[cIndex], destination, 0);

            // move to next character position
#ifdef BLAZERX
            x += TECH_FONT_WIDTH + 3;
#else
#ifdef VBE_SUPPORT
            x += TECH_FONT_WIDTH + 2;
#else
            x += TECH_FONT_WIDTH + 1;
#endif
#endif
        }
    }
}

void clearDisplay(int color) {
    // this function fills the setup display screen with a color
    int y;

    // clear display with horizontal lines
    for (y = DISPLAY_Y; y < DISPLAY_Y + DISPLAY_HEIGHT; y++) {
        lineH(DISPLAY_X, DISPLAY_X + DISPLAY_WIDTH - 1, y, color);
    }
}

void introTitle(void) {
    // load in the starblazer title screen
    pcxInit(&ImagePcx);
    pcxLoad("blazeint.pcx", &ImagePcx, 1);

    // show the PCX buffer
    pcxShowBuffer(&ImagePcx);

    // done with data so delete it
    pcxDelete(&ImagePcx);

    // do special effects

    // wait for a sec
    timeDelay(10);

    doStarburst();

    timeDelay(50);
}

void closingScreen(void) {
    // this function prints the credits

    // blank screen
    fillScreen(0);

#ifndef BLAZERX
    // restore palette
    writePalette(&GamePalette);

#endif
    if (MusicEnabled) {
        musicStop();
        musicPlay(&Song, 10);
    }

    // draw the credits
#ifdef BLAZERX
    techPrint(32, 160, "MUSICAL MASTERY BY", VideoBuffer);
    timeDelay(20);
    techPrint(64, 192, "DEAN HUDSON OF", VideoBuffer);
    timeDelay(20);
    techPrint(96, 224, "ECLIPSE PRODUCTIONS", VideoBuffer);
    timeDelay(20);

    techPrint(32, 320, "MIDPAK INSTRUMENTATION CONSULTING BY", VideoBuffer);
    timeDelay(20);
    techPrint(64, 352, "ROB WALLACE OF", VideoBuffer);
    timeDelay(20);
    techPrint(96, 384, "WALLACE MUSIC & SOUND", VideoBuffer);
#else
#ifdef VBE_SUPPORT
    techPrint(20, 100, "MUSICAL MASTERY BY", VideoBuffer);
    timeDelay(20);
    techPrint(40, 120, "DEAN HUDSON OF", VideoBuffer);
    timeDelay(20);
    techPrint(60, 140, "ECLIPSE PRODUCTIONS", VideoBuffer);
    timeDelay(20);

    techPrint(20, 200, "MIDPAK INSTRUMENTATION CONSULTING BY", VideoBuffer);
    timeDelay(20);
    techPrint(40, 220, "ROB WALLACE OF", VideoBuffer);
    timeDelay(20);
    techPrint(60, 240, "WALLACE MUSIC & SOUND", VideoBuffer);
#else
    techPrint(10, 50, "MUSICAL MASTERY BY", VideoBuffer);
    timeDelay(20);
    techPrint(20, 60, "DEAN HUDSON OF", VideoBuffer);
    timeDelay(20);
    techPrint(30, 70, "ECLIPSE PRODUCTIONS", VideoBuffer);
    timeDelay(20);

    techPrint(10, 100, "MIDPAK INSTRUMENTATION CONSULTING BY", VideoBuffer);
    timeDelay(20);
    techPrint(20, 110, "ROB WALLACE OF", VideoBuffer);
    timeDelay(20);
    techPrint(30, 120, "WALLACE MUSIC & SOUND", VideoBuffer);
#endif
#endif

    // wait a sec
    timeDelay(125);

    // fade away
    screenTransition(SCREEN_DARKNESS);
}

void introWaite(void) {
    // load in the waite group title screen
    pcxInit(&ImagePcx);
    pcxLoad("waite.pcx", &ImagePcx, 1);

    // show the PCX buffer
    pcxShowBuffer(&ImagePcx);

    // done with data so delete it
    pcxDelete(&ImagePcx);

    // do special effects

    // wait for a sec
    timeDelay(40);

    screenTransition(SCREEN_WHITENESS);

    // blank the screen
    fillScreen(0);
}

void introControls(void) {
    // this function displays the controls screen

    // load in the starblazer controls screen
    pcxInit(&ImageControls);
    pcxLoad("blazecon.pcx", &ImageControls, 1);

    // copy controls data to video buffer
    pcxShowBuffer(&ImageControls);

    // scan under button sprite and draw
    spriteUnder(&Button1, VideoBuffer);
    spriteDraw(&Button1, VideoBuffer, 1);

    // delete pcx file
    pcxDelete(&ImageControls);
}

void introBriefing(void) {
    // this function displays the controls screen
    int done = 0,
        page = 0,
        index;

    // load in the starblazer control screen
    pcxInit(&ImageControls);
    pcxLoad("blazeins.pcx", &ImageControls, 1);

    // copy controls data to video buffer
    pcxShowBuffer(&ImageControls);

    // delete pcx file
    pcxDelete(&ImageControls);

    // display the first page (under BLAZERX, positions * 1024/640, 768/480 -
    // blazeins.pcx was rescaled by the same factor)
    for (index = 0; index < NUM_LINES_PAGE; index++) {
#ifdef BLAZERX
        fontEngine1(250, 93 + index * 30, 0, 0, Instructions[index + page * 17], VideoBuffer);
#else
#ifdef VBE_SUPPORT
        fontEngine1(156, 58 + index * 19, 0, 0, Instructions[index + page * 17], VideoBuffer);
#else
        fontEngine1(78, 24 + index * 8, 0, 0, Instructions[index + page * 17], VideoBuffer);
#endif
#endif
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
#ifdef BLAZERX
                Button3.x = 592;
                Button3.y = 653;
#else
#ifdef VBE_SUPPORT
                Button3.x = 370;
                Button3.y = 408;
#else
                Button3.x = 185;
                Button3.y = 170;
#endif
#endif
                Button3.currFrame = 3;

                spriteDraw(&Button3, VideoBuffer, 1);

                digitalFxPlay(BLZKEY_VOC, 2);

                timeDelay(2);

                Button3.currFrame = 2;

                spriteDraw(&Button3, VideoBuffer, 1);
            }

            if (KeyboardState[MAKE_DOWN]) {
                // page down
                if (++page >= NUM_PAGES) {
                    page = NUM_PAGES - 1;
                }

                // press button
#ifdef BLAZERX
                Button3.x = 339;
                Button3.y = 653;
#else
#ifdef VBE_SUPPORT
                Button3.x = 212;
                Button3.y = 408;
#else
                Button3.x = 106;
                Button3.y = 170;
#endif
#endif
                Button3.currFrame = 1;

                spriteDraw(&Button3, VideoBuffer, 1);

                digitalFxPlay(BLZKEY_VOC, 2);

                timeDelay(2);

                Button3.currFrame = 0;

                spriteDraw(&Button3, VideoBuffer, 1);
            }

            if (KeyboardState[MAKE_ESC]) {
                done = 1;
            }

            // refresh display
            for (index = 0; index < NUM_LINES_PAGE; index++) {
#ifdef BLAZERX
                fontEngine1(250, 93 + index * 30, 0, 0, Instructions[index + page * 17], VideoBuffer);
#else
#ifdef VBE_SUPPORT
                fontEngine1(156, 58 + index * 19, 0, 0, Instructions[index + page * 17], VideoBuffer);
#else
                fontEngine1(78, 24 + index * 8, 0, 0, Instructions[index + page * 17], VideoBuffer);
#endif
#endif
            }
        }

        // do the scrolling lite thing
#ifdef BLAZERX
        panelFx(PANEL_FX_BRIEFING);
#else
        panelFx();
#endif

        // wait a sec
        timeDelay(1);
    }
}

void loadExplosions(void) {
    // this function loads the bitmap explosions
    int index,
        frames;

    // load the imagery for the explosions
    pcxInit(&ImagePcx);
    pcxLoad("blazeexp.pcx", &ImagePcx, 1);

    // load each explosion in
    for (index = 0; index < NUM_EXPLOSIONS; index++) {
        // initialize each sprite
#ifdef VBE_SUPPORT
        spriteInit(&Explosions[index], 0, 0,
                       EXPLOSION_WIDTH * CAM_ZOOM, EXPLOSION_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
#else
        spriteInit(&Explosions[index], 0, 0, 28, 22, 0, 0, 0, 0, 0, 0);
#endif
#ifdef BLAZERX
        Explosions[index].transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif

        // extract the animation frames
        for (frames = 0; frames <= NUM_EXPLOSION_FRAMES; frames++) {
            pcxGetSprite(&ImagePcx, &Explosions[index], frames, frames, 0);
        }
    }

    // delete the pcx file
    pcxDelete(&ImagePcx);
}

void loadIcons(void) {
    // this function loads various icons for the game
    int index;

    // load the imagery for the control buttons on the setup screen
    pcxInit(&ImagePcx);
    pcxLoad("blazebt1.pcx", &ImagePcx, 1);

    // initialize the button sprite
#ifdef BLAZERX
    spriteInit(&Button1, 346, 242, 32, 26, 0, 0, 0, 0, 0, 0); // pos: (236-20, 151); size: (20,16) * 1024/640, 768/480
    Button1.transparentColor = SPRITE_TRANSPARENT_COLOR;
#else
#ifdef VBE_SUPPORT
    spriteInit(&Button1, 236 - 20, 151, 20, 16, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&Button1, 118 - 10, 63, 10, 8, 0, 0, 0, 0, 0, 0);
#endif
#endif

    Button1.counter1 = 0;   // button is on the 0th element in the list

    // extract the bitmaps for the button, there are 4 animation cells
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &Button1, index, index, 0);
    }

    // load in display selection buttons

    // initialize the button sprite
#ifdef BLAZERX
    spriteInit(&Button2, 0, DISPLAY_Y + DISPLAY_HEIGHT - 19, 32, 26, 0, 0, 0, 0, 0, 0);
    Button2.transparentColor = SPRITE_TRANSPARENT_COLOR;
#else
#ifdef VBE_SUPPORT
    spriteInit(&Button2, 0, DISPLAY_Y + DISPLAY_HEIGHT - 12, 20, 16, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&Button2, 0, DISPLAY_Y + DISPLAY_HEIGHT - 6, 10, 8, 0, 0, 0, 0, 0, 0);
#endif
#endif

    // extract the bitmaps for the button, there are 2 animation cells
    for (index = 0; index < 2; index++) {
        pcxGetSprite(&ImagePcx, &Button2, index, index, 1);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // load the imagery for the briefing control buttons
    pcxInit(&ImagePcx);
    pcxLoad("blazebt3.pcx", &ImagePcx, 1);

    // initialize the button sprite
#ifdef BLAZERX
    spriteInit(&Button3, 0, 0, 134, 38, 0, 0, 0, 0, 0, 0);
    Button3.transparentColor = SPRITE_TRANSPARENT_COLOR;
#else
#ifdef VBE_SUPPORT
    spriteInit(&Button3, 0, 0, 84, 24, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&Button3, 0, 0, 42, 12, 0, 0, 0, 0, 0, 0);
#endif
#endif

    // extract the bitmaps for the button, there are 4 animation cells
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &Button3, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // load the imagery for the display bitmaps
    pcxInit(&ImagePcx);
    pcxLoad("blazedis.pcx", &ImagePcx, 1);

    // initialize the display sprite
#ifdef BLAZERX
    spriteInit(&Displays, DISPLAY_X, DISPLAY_Y + 19, 230, 64, 0, 0, 0, 0, 0, 0);
    Displays.transparentColor = SPRITE_TRANSPARENT_COLOR;
#else
#ifdef VBE_SUPPORT
    spriteInit(&Displays, DISPLAY_X, DISPLAY_Y + 12, 144, 40, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&Displays, DISPLAY_X, DISPLAY_Y + 6, 72, 20, 0, 0, 0, 0, 0, 0);
#endif
#endif

    // extract the bitmaps for the display bitmaps, there are 2 images
    for (index = 0; index < 2; index++) {
        pcxGetSprite(&ImagePcx, &Displays, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);
}

void loadShips(void) {
    int index;

    // load the imagery for the local ships
    pcxInit(&ImagePcx);
    pcxLoad("blazeshl.pcx", &ImagePcx, 1);

    // load in the imagery for the local gryfon and raptor
#ifdef VBE_SUPPORT
    spriteInit(&GryfonL, 0, 0, SHIP_WIDTH * CAM_ZOOM, SHIP_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
    spriteInit(&RaptorL, 0, 0, SHIP_WIDTH * CAM_ZOOM, SHIP_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&GryfonL, 0, 0, 22, 18, 0, 0, 0, 0, 0, 0);
    spriteInit(&RaptorL, 0, 0, 22, 18, 0, 0, 0, 0, 0, 0);
#endif
#ifdef BLAZERX
    GryfonL.transparentColor = SPRITE_TRANSPARENT_COLOR;
    RaptorL.transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif

    // there are 32 animation cells per ship
    for (index = 0; index < 16; index++) {
#ifdef BLAZERX
        pcxGetSpriteTinted(&ImagePcx, &GryfonL, index, index % 12, index / 12, PlayersTintKeys, 2);
        pcxGetSpriteTinted(&ImagePcx, &RaptorL, index, index % 12, 2 + index / 12, PlayersTintKeys, 2);
#else
        pcxGetSprite(&ImagePcx, &GryfonL, index, index % 12, index / 12);
        pcxGetSprite(&ImagePcx, &RaptorL, index, index % 12, 2 + index / 12);
#endif

        // these frames are with engines on
#ifdef BLAZERX
        pcxGetSpriteTinted(&ImagePcx, &GryfonL, index + 16, index % 12, 4 + index / 12, PlayersTintKeys, 2);
        pcxGetSpriteTinted(&ImagePcx, &RaptorL, index + 16, index % 12, 4 + 2 + index / 12, PlayersTintKeys, 2);
#else
        pcxGetSprite(&ImagePcx, &GryfonL, index + 16, index % 12, 4 + index / 12);
        pcxGetSprite(&ImagePcx, &RaptorL, index + 16, index % 12, 4 + 2 + index / 12);
#endif
    }

    pcxDelete(&ImagePcx);

    // now the remote gryfon and raptor
    pcxInit(&ImagePcx);
    pcxLoad("blazeshr.pcx", &ImagePcx, 1);

#ifdef VBE_SUPPORT
    spriteInit(&GryfonR, 0, 0, SHIP_WIDTH * CAM_ZOOM, SHIP_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
    spriteInit(&RaptorR, 0, 0, SHIP_WIDTH * CAM_ZOOM, SHIP_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&GryfonR, 0, 0, 22, 18, 0, 0, 0, 0, 0, 0);
    spriteInit(&RaptorR, 0, 0, 22, 18, 0, 0, 0, 0, 0, 0);
#endif
#ifdef BLAZERX
    GryfonR.transparentColor = SPRITE_TRANSPARENT_COLOR;
    RaptorR.transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif

    // there are 32 animation cells per ship
    for (index = 0; index < 16; index++) {
#ifdef BLAZERX
        pcxGetSpriteTinted(&ImagePcx, &GryfonR, index, index % 12, index / 12, RemotesTintKeys, 2);
        pcxGetSpriteTinted(&ImagePcx, &RaptorR, index, index % 12, 2 + index / 12, RemotesTintKeys, 2);
#else
        pcxGetSprite(&ImagePcx, &GryfonR, index, index % 12, index / 12);
        pcxGetSprite(&ImagePcx, &RaptorR, index, index % 12, 2 + index / 12);
#endif

        // these frames are with engines on
#ifdef BLAZERX
        pcxGetSpriteTinted(&ImagePcx, &GryfonR, index + 16, index % 12, 4 + index / 12, RemotesTintKeys, 2);
        pcxGetSpriteTinted(&ImagePcx, &RaptorR, index + 16, index % 12, 4 + 2  + index / 12, RemotesTintKeys, 2);
#else
        pcxGetSprite(&ImagePcx, &GryfonR, index + 16, index % 12, 4 + index / 12);
        pcxGetSprite(&ImagePcx, &RaptorR, index + 16, index % 12, 4 + 2  + index / 12);
#endif
    }

    // initialize the player and remote sprites
#ifdef VBE_SUPPORT
    spriteInit(&PlayersShip,
                   (VIEW_WIDTH / 2 - SHIP_WIDTH / 2) * CAM_ZOOM,
                   (VIEW_HEIGHT / 2 - SHIP_HEIGHT / 2) * CAM_ZOOM,
                   SHIP_WIDTH * CAM_ZOOM, SHIP_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
    spriteInit(&RemotesShip, 0, 0, SHIP_WIDTH * CAM_ZOOM, SHIP_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);

    // initialize the starburst
    spriteInit(&Starburst, 0, 0, SHIP_WIDTH * CAM_ZOOM, SHIP_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&PlayersShip, 160 - 11, 100 - 9, 22, 18, 0, 0, 0, 0, 0, 0);
    spriteInit(&RemotesShip, 0, 0, 22, 18, 0, 0, 0, 0, 0, 0);

    // initialize the starburst
    spriteInit(&Starburst, 0, 0, 22, 18, 0, 0, 0, 0, 0, 0);
#endif
#ifdef BLAZERX
    PlayersShip.transparentColor = SPRITE_TRANSPARENT_COLOR;
    RemotesShip.transparentColor = SPRITE_TRANSPARENT_COLOR;
    Starburst.transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif

    // extract the bitmaps for the starburst, there are 6 animation cells
    for (index = 0; index < 6; index++) {
        pcxGetSprite(&ImagePcx, &Starburst, index, index, 8);
    }

    pcxDelete(&ImagePcx);
}

void doStarburst(void) {
    int number, index;

    // select a random number of starbursts
    number = 2 + rand() % 3;

    for (index = 0; index < number; index++) {
        // select position for starburst
#ifdef BLAZERX
        Starburst.x = 512 + rand() % 373;
        Starburst.y = 307 + rand() % 40;
#else
#ifdef VBE_SUPPORT
        Starburst.x = 320 + rand() % 244;
        Starburst.y = 192 + rand() % 34;
#else
        Starburst.x = 160 + rand() % 140;
        Starburst.y = 80 + rand() % 20;
#endif
#endif

        spriteUnder(&Starburst, VideoBuffer);

        // do starburst
        for (Starburst.currFrame = 0; Starburst.currFrame < 6; Starburst.currFrame++) {
            spriteErase(&Starburst, VideoBuffer);
            spriteUnder(&Starburst, VideoBuffer);
            spriteDraw(&Starburst, VideoBuffer, 1);

            timeDelay(1 + rand() % 2);
        }

        // erase the starburst
        spriteErase(&Starburst, VideoBuffer);
    }
}

int displaySelect(int current) {
    // this function is used to select between two choices in the display window
    // compute starting position of selection icon based on default selection
#ifdef BLAZERX
    Button2.x = DISPLAY_X + 45 + current * 128;
#else
#ifdef VBE_SUPPORT
    Button2.x = DISPLAY_X + 28 + current * 80;
#else
    Button2.x = DISPLAY_X + 14 + current * 40;
#endif
#endif

    // scan under selection icon
    spriteUnder(&Button2, VideoBuffer);
    spriteDraw(&Button2, VideoBuffer, 1);

    // until user exits process event loop
    while (1) {
        // get input
        if (KeysActive > 0) {
            // what is user trying to do?
            if (KeyboardState[MAKE_RIGHT]) {
                if (++current > 1) {
                    current = 0;
                }

                digitalFxPlay(BLZKEY_VOC, 2);

                timeDelay(1);
            } else if (KeyboardState[MAKE_LEFT]) {
                if (--current < 0) {
                    current = 1;
                }

                digitalFxPlay(BLZKEY_VOC, 2);

                timeDelay(1);
            } else if (KeyboardState[MAKE_ESC]) {
                // print that selection was aborted
                clearDisplay(0);

                fontEngine1(
#ifdef BLAZERX
                    DISPLAY_X + 3,
                    DISPLAY_Y + 3,
#else
                    DISPLAY_X + 2,
                    DISPLAY_Y + 2,
#endif
                    0,
                    0,
                    "ABORTED...",
                    VideoBuffer);

                digitalFxPlay(BLZABRT_VOC, 1);

                timeDelay(5);

                clearDisplay(0);

                // return selection aborted
                return -1;
            } else if (KeyboardState[MAKE_ENTER]) {
                // carriage return, making selection
                // illuminate button for a second

                // draw button down
                Button2.currFrame = 1;
                spriteDraw(&Button2, VideoBuffer, 1);

                digitalFxPlay(BLZKEY_VOC, 2);

                timeDelay(5);

                // now draw button up
                Button2.currFrame = 0;
                spriteDraw(&Button2, VideoBuffer, 1);

                clearDisplay(0);

                fontEngine1(
#ifdef BLAZERX
                    DISPLAY_X + 3,
#else
                    DISPLAY_X + 2,
#endif
                    DISPLAY_Y,
                    0,
                    0,
                    "SELECTION",
                    VideoBuffer);

#ifdef BLAZERX
                fontEngine1(
                    DISPLAY_X + 6, DISPLAY_Y + 6 + 26,
                    0,
                    0,
                    "RECORDED", VideoBuffer);
#else
#ifdef VBE_SUPPORT
                fontEngine1(
                    DISPLAY_X + 4, DISPLAY_Y + 4 + 16,
                    0,
                    0,
                    "RECORDED", VideoBuffer);
#else
                fontEngine1(
                    DISPLAY_X + 2,
                    DISPLAY_Y + 2 + 8,
                    0,
                    0,
                    "RECORDED",
                    VideoBuffer);
#endif
#endif

                digitalFxPlay(BLZSEL_VOC, 1);

                timeDelay(5);

                clearDisplay(0);

                // return the selection
                return current;
            }

            // erase selection icon
            spriteErase(&Button2, VideoBuffer);

            // compute x position
#ifdef BLAZERX
            Button2.x = DISPLAY_X + 45 + current * 128;
#else
#ifdef VBE_SUPPORT
            Button2.x = DISPLAY_X + 28 + current * 80;
#else
            Button2.x = DISPLAY_X + 14 + current * 40;
#endif
#endif

            // scan under and draw selection icon
            spriteUnder(&Button2, VideoBuffer);
            spriteDraw(&Button2, VideoBuffer, 1);
        }

        // perform special effects
#ifdef BLAZERX
        panelFx(PANEL_FX_CONTROL);
#else
        panelFx();
#endif

        // wait a bit
        timeDelay(1);
    }
}

void copyFrames(SpritePtr dest, SpritePtr source) {
    // this function is used to copy the image frames from one sprite to another
    int index;

    for (index = 0; index < source->numFrames; index++) {
        // assign next frame
        dest->frames[index] = source->frames[index];
#ifdef BLAZERX

        // carry over the shield/engine tint mask too, if this frame has one
        dest->tintMask[index] = source->tintMask[index];
#endif
    }

    // set up dest fields
    dest->numFrames = source->numFrames;
    dest->currFrame = 0;
}

void shieldControl(int ship, int on) {
    // this function is used to activate or de-activate the shields of the player or remote ship
    if (ship == THE_PLAYER) {
        // which ship does player have?
        if (PlayersShipType == GRYFON_SHIP) {
            // activating or de-activating shields?
            if (on) {
                PlayersShieldColor = PrimaryBlue;
            } else {
                PlayersShieldColor = PrimaryBlack;
            }
        } else {
            // activating or de-activating shields?
            if (on) {
                PlayersShieldColor = PrimaryRed;
            } else {
                PlayersShieldColor = PrimaryBlack;
            }
        }

        // set the color
#ifdef BLAZERX
        PlayersTintColors[0] = vgaColorToRGB32(PlayersShieldColor);
#else
        writeColorReg(PLAYERS_SHIELD_REG, &PlayersShieldColor);
#endif

        // record shield change
        PlayersShields = on;
    } else {
        // must be remote ship

        // which ship does remote have?
        if (RemotesShipType == GRYFON_SHIP) {
            // activating or de-activating shields?
            if (on) {
                RemotesShieldColor = PrimaryBlue;
            } else {
                RemotesShieldColor = PrimaryBlack;
            }
        } else {
            // activating or de-activating shields?
            if (on) {
                RemotesShieldColor = PrimaryRed;
            } else {
                RemotesShieldColor = PrimaryBlack;
            }
        }

        // set the color
#ifdef BLAZERX
        RemotesTintColors[0] = vgaColorToRGB32(RemotesShieldColor);
#else
        writeColorReg(REMOTES_SHIELD_REG, &RemotesShieldColor);
#endif

        // record shield change
        RemotesShields = on;
    }
}

void eraseMissiles(void) {
    // this function erases all the missiles
    int index;

    for (index = 0; index < NUM_MISSILES; index++) {
        // is this missile active and visible
        if (Missiles[index].state == MISS_ACTIVE && Missiles[index].visible) {
#ifdef VBE_SUPPORT
            cameraPixelErase(Missiles[index].sx, Missiles[index].sy, Missiles[index].backColor);
#else
            writePixelDb(Missiles[index].sx, Missiles[index].sy, Missiles[index].backColor);
#endif
        }
    }
}

void underMissiles(void) {
    // this function scans the background under the missiles
    int index,
        pxWindow,
        pyWindow,
        mx,
        my;

    // compute starting position of players window so screen mapping can be done
#ifdef VBE_SUPPORT
    pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
    pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;
#else
    pxWindow = PlayersX - 160 + 11;
    pyWindow = PlayersY - 100 + 9;
#endif

    for (index = 0; index < NUM_MISSILES; index++) {
        // is this missile active
        if (Missiles[index].state == MISS_ACTIVE) {
#ifdef VBE_SUPPORT
            mx = Missiles[index].x - pxWindow;
            my = Missiles[index].y - pyWindow;

            // test if missile is visible on screen?
            if (mx >= VIEW_WIDTH || mx < 0 || my >= VIEW_HEIGHT || my < 0) {
                // this missile is invisible and has been clipped
                Missiles[index].visible = 0;

                // process next missile
                continue;
            }

            // remap to screen coordinates
            mx = Missiles[index].sx = mx * CAM_ZOOM;
            my = Missiles[index].sy = my * CAM_ZOOM;

            // scan under missile
            cameraPixelUnder(mx, my, Missiles[index].backColor);
#else
            // remap to screen coordinates
            mx = Missiles[index].sx = Missiles[index].x - pxWindow;
            my = Missiles[index].sy = Missiles[index].y - pyWindow;

            // test if missile is visible on screen?
            if (mx >= 320 || mx < 0 || my >= 200 || my < 0) {
                // this missile is invisible and has been clipped
                Missiles[index].visible = 0;

                // process next missile
                continue;
            }

            // scan under missile
            Missiles[index].backColor = readPixelDb(mx, my);
#endif

            // set visibility flag
            Missiles[index].visible = 1;
        }
    }
}

void drawMissiles(void) {
    // this function draws all the missiles
    int index;

    for (index = 0; index < NUM_MISSILES; index++) {
        // is this missile active and visible
        if (Missiles[index].state == MISS_ACTIVE && Missiles[index].visible) {
#ifdef VBE_SUPPORT
            cameraPixelDraw(Missiles[index].sx, Missiles[index].sy, Missiles[index].color);
#else
            writePixelDb(Missiles[index].sx, Missiles[index].sy, Missiles[index].color);
#endif
        }
    }
}

void initMissiles(void) {
    // this function resets and initializes all missiles
    int index;

    for (index = 0; index < NUM_MISSILES; index++) {
        Missiles[index].state = MISS_INACTIVE;
    }
}

void moveMissiles(void) {
    // this function moves all the missiles and performs collision detection
    int index,
        aIndex,
        missX,
        missY;

    // process each missile
    for (index = 0; index < NUM_MISSILES; index++) {
        // is missile active
        if (Missiles[index].state == MISS_ACTIVE) {
            // move the missile
            missX = (Missiles[index].x += Missiles[index].xv);
            missY = (Missiles[index].y += Missiles[index].yv);

            // test if a missile has hit an asteroid
            for (aIndex = 0; aIndex < NUM_ASTEROIDS; aIndex++) {
                // test if asteroid is active
                if (Asteroids[aIndex].rock.state == ASTEROID_ACTIVE) {
                    // test for collision
                    if (missX >= Asteroids[aIndex].x &&
                        missY >= Asteroids[aIndex].y &&
#ifdef VBE_SUPPORT
                        missX <= Asteroids[aIndex].x + Asteroids[aIndex].rock.width / CAM_ZOOM &&
                        missY <= Asteroids[aIndex].y + Asteroids[aIndex].rock.height / CAM_ZOOM) {
#else
                        missX <= Asteroids[aIndex].x + Asteroids[aIndex].rock.width &&
                        missY <= Asteroids[aIndex].y + Asteroids[aIndex].rock.height) {
#endif

                        // kill the asteroid and the missile
                        Asteroids[aIndex].rock.state = ASTEROID_INACTIVE;

                        Missiles[index].state = MISS_INACTIVE;

                        // what kind of asteroid did we have?
                        switch (Asteroids[aIndex].type) {
                            case ASTEROID_LARGE: {
                                // start an explosion at proper place
                                startExplosion(
                                    Asteroids[aIndex].x,
                                    Asteroids[aIndex].y,
                                    2);

                                // start one medium and one small asteroid (if possible)
                                startAsteroid(
                                    Asteroids[aIndex].x,
                                    Asteroids[aIndex].y,
                                    ASTEROID_MEDIUM);

                                startAsteroid(
                                    Asteroids[aIndex].x + ASTEROID_LARGE_WIDTH / 2,
                                    Asteroids[aIndex].y + ASTEROID_LARGE_HEIGHT / 2,
                                    ASTEROID_SMALL);

                                if (Missiles[index].type == PLAYER_MISSILE) {
                                    PlayersScore += 100;
                                } else {
                                    RemotesScore += 100;
                                }
                            } break;

                            case ASTEROID_MEDIUM: {
                                // start an explosion at proper place
                                startExplosion(
                                    Asteroids[aIndex].x - 2,
                                    Asteroids[aIndex].y - 2,
                                    2);

                                // start two small asteroids (if possible)
                                startAsteroid(
                                    Asteroids[aIndex].x,
                                    Asteroids[aIndex].y,
                                    ASTEROID_SMALL);

                                startAsteroid(
                                    Asteroids[aIndex].x + ASTEROID_MEDIUM_WIDTH / 2,
                                    Asteroids[aIndex].y + ASTEROID_MEDIUM_HEIGHT / 2,
                                    ASTEROID_SMALL);

                                if (Missiles[index].type == PLAYER_MISSILE) {
                                    PlayersScore += 50;
                                } else {
                                    RemotesScore += 50;
                                }
                            } break;

                            case ASTEROID_SMALL: {
                                // start an explosion at proper place
                                startExplosion(
                                    Asteroids[aIndex].x - 8,
                                    Asteroids[aIndex].y - 8,
                                    2);

                                // start a randomly positioned asteroid of any size at the worm hole
                                startAsteroid(WORMHOLE_X, WORMHOLE_Y, rand() % 3);

                                if (Missiles[index].type == PLAYER_MISSILE) {
                                    PlayersScore += 25;
                                } else {
                                    RemotesScore += 25;
                                }
                            } break;

                            default:
                                break;
                        }

                        // break inner loop
                        break;
                    }
                }
            }

            // test if missiles hit local player
#ifdef BLAZERX
            if ((Linked || AiEnabled) && PlayersState == ALIVE && Missiles[index].type == REMOTE_MISSILE) {
#else
            if (Linked && PlayersState == ALIVE && Missiles[index].type == REMOTE_MISSILE) {
#endif
                if (missX > PlayersX && missX < PlayersX + SHIP_WIDTH &&
                    missY > PlayersY && missY < PlayersY + SHIP_HEIGHT) {

                    // de-activate missile
                    Missiles[index].state = MISS_INACTIVE;

                    // were shields up?
                    if (PlayersShields) {
                        // start explosion
                        startExplosion(PlayersX, PlayersY, 2);
                    } else {
                        // say bye bye!
                        startExplosion(PlayersX, PlayersY, 2);
                        startNova(PlayersX + SHIP_WIDTH / 2, PlayersY + SHIP_HEIGHT / 2);

                        // start local player death sequence
                        startPlayersDeath();
                    }
                }
            }

            // test if missile has hit remote player
#ifdef BLAZERX
            if ((Linked || AiEnabled) && RemotesState == ALIVE && Missiles[index].type == PLAYER_MISSILE) {
#else
            if (Linked && RemotesState == ALIVE && Missiles[index].type == PLAYER_MISSILE) {
#endif
                if (missX > RemotesX && missX < RemotesX + SHIP_WIDTH &&
                    missY > RemotesY && missY < RemotesY + SHIP_HEIGHT) {

                    // de-activate missile
                    Missiles[index].state = MISS_INACTIVE;

                    // were shields up?
                    if (RemotesShields) {
                        // start explosion
                        startExplosion(RemotesX, RemotesY, 2);
                    } else {
                        // say bye bye!
                        startExplosion(RemotesX, RemotesY, 2);
                        startNova(RemotesX + SHIP_WIDTH / 2, RemotesY + SHIP_HEIGHT / 2);

                        // start remote death sequence
                        startRemotesDeath();
                    }
                }
            }

            // test if it's hit the edge of the screen or a wall
            if (missX >= UNIVERSE_WIDTH + UNIVERSE_BORDER ||
                missX < -UNIVERSE_BORDER ||
                missY > UNIVERSE_HEIGHT + UNIVERSE_BORDER ||
                missY < -UNIVERSE_BORDER ||
                --Missiles[index].lifetime < 0) {

                // de-activate the missile
                Missiles[index].state = MISS_INACTIVE;
            }

            // test if this missiles has been terminated in some way, if so update
            // the active missile variable for the player or remote
            if (Missiles[index].state == MISS_INACTIVE) {
                // update number of active missiles
                if (Missiles[index].type == PLAYER_MISSILE) {
                    PlayersActiveMissiles--;
                } else if (Missiles[index].type == REMOTE_MISSILE) {
                    RemotesActiveMissiles--;
                }
            }
        }
    }
}

int startMissile(int x, int y, int xv, int yv, int color, int type) {
    // this function starts a photon missile with the given position and speed
    int index;

    // scan for an inactive
    for (index = 0; index < NUM_MISSILES; index++) {
        // is this missile free?
        if (Missiles[index].state == MISS_INACTIVE) {
            // set up fields
            Missiles[index].state = MISS_ACTIVE;
            Missiles[index].x = x;
            Missiles[index].y = y;
            Missiles[index].sx = 0;
            Missiles[index].sy = 0;
            Missiles[index].counter = 0;
            Missiles[index].threshold = 0;
            Missiles[index].xv = xv;
            Missiles[index].yv = yv;
            Missiles[index].color = color;
#ifdef VBE_SUPPORT
            memset(Missiles[index].backColor, 0, sizeof(Missiles[index].backColor));
#else
            Missiles[index].backColor = 0;
#endif
            Missiles[index].type = type;
            Missiles[index].visible = 0;
            Missiles[index].lifetime = 30 + rand() % 10;

            // make a sound
            digitalFxPlay(BLZLAS_VOC, 2);

            // return success
            return 1;
        }
    }

    // must not have found one
    return 0;
}

void startPlayersDeath() {
    // this function starts the players death sequence
    PlayersXv = 0;
    PlayersYv = 0;
    PlayersEngine = 0;
    PlayersFlameCount = 0;
    PlayersGravity = 0;
    PlayersShields = 0;
    PlayersShieldTime = 0;
    PlayersShieldCooldown = 0;
    PlayersCloak = -1;
    PlayersState = DYING;
    PlayersDeathCount = 48;

    DebounceHud = 0;
    DebounceScan = 0;
    DebounceCloak = 0;
    DebounceThrust = 0;
    DebounceFire = 0;
    DebounceShields = 0;
}

void resetPlayer(void) {
    // this function resets the player to his starting position
    PlayersLastX = GameStartX[Master];
    PlayersLastY = GameStartY[Master];
    PlayersX = GameStartX[Master];
    PlayersY = GameStartY[Master];
    PlayersState = ALIVE;
}

void resetRemote(void) {
    // this function resets the remote to his starting position
    RemotesLastX = GameStartX[Slave];
    RemotesLastY = GameStartY[Slave];
    RemotesX = GameStartX[Slave];
    RemotesY = GameStartY[Slave];
    RemotesState = ALIVE;
}

void startRemotesDeath(void) {
    // this function starts the remotes death sequence
    RemotesXv = 0;
    RemotesYv = 0;
    RemotesEngine = 0;
    RemotesFlameCount = 0;
    RemotesGravity = 0;
    RemotesShields = 0;
    RemotesShieldTime = 0;
    RemotesShieldCooldown = 0;
    RemotesCloak = -1;
    RemotesState = DYING;
    RemotesDeathCount = 48;
#ifdef BLAZERX
}

int aiHeading(int vx, int vy) {
    // returns the ship heading (0..15) whose motion vector best matches the
    // direction (vx,vy), by maximizing the dot product against each heading.
    // vx,vy stay within +/-1250 (half the universe) so the products fit an int.
    int f, bestF, bestDot, dot;

    bestF = 0;
    bestDot = -32767;

    for (f = 0; f < 16; f++) {
        dot = vx * MotionDx[f] + vy * MotionDy[f];

        if (dot > bestDot) {
            bestDot = dot;
            bestF = f;
        }
    }

    return bestF;
}

void aiPickState(void) {
    // choose the enemy's next behavior. HUNT (close in, pace the player, shoot)
    // is the most common so it stays aggressive; STRAFE circles for variety;
    // FLEE backs off to reposition, and is where it retreats when low on energy.
    int r;

    // our own energy is low: back off to stop bleeding power
    if (RemotesEnergy < AI_LOW_ENERGY) {
        AiState = AI_FLEE;
        AiStateTimer = 35 + rand() % 35;
        return;
    }

    r = rand() % 100;

    // mostly STRAFE (circle the player), some HUNT, an occasional FLEE
    if (r < 60) {
        AiState = AI_STRAFE;
        AiStateTimer = 50 + rand() % 50;
        AiOrbitDir = (rand() & 1) ? 1 : -1;
    } else if (r < 85) {
        AiState = AI_HUNT;
        AiStateTimer = 35 + rand() % 30;
    } else {
        AiState = AI_FLEE;
        AiStateTimer = 25 + rand() % 25;
    }
}

int computeRemoteAi(void) {
    // single-player brain for the remote ship. It returns the same REMOTE_*
    // input byte a human peer would have sent over the link, so the existing
    // remote simulation, collision, death and drawing code all run unchanged -
    // the AI just decides which "keys" the enemy ship presses each frame.
    //
    // It steers its VELOCITY, not its position: each frame it works out a target
    // velocity (the player's velocity plus a small per-behavior maneuver) and only
    // thrusts to correct the difference. Pacing the player's velocity makes it
    // glide on the player-locked screen and stops it building runaway momentum, so
    // it no longer spins / weaves / jerks. When its velocity already matches it
    // faces the firing solution and shoots - the common, calm case. The behavior
    // states (HUNT / STRAFE / FLEE) just choose which maneuver velocity to aim for.
    int dx, dy, dist, huntFrame, diff, desiredFrame, keys, m, mdx, mdy;
    int aiSees, returning, radial, tx, ty, mFrame;
    int relMx, relMy, targetVx, targetVy, errVx, errVy, errMag;
    int aimT, fireFrame, diffFire, adx, ady, aDist, aNear;

    keys = 0;

    // The enemy can only track the player while the player is visible. The
    // moment the player cloaks, the enemy loses the lock: it remembers the last
    // place it saw the player, steers toward that spot, and holds fire until the
    // player decloaks. So cloaking actually shakes the enemy's aim.
    aiSees = (PlayersCloak == -1);

    if (aiSees) {
        AiLastX = PlayersX;
        AiLastY = PlayersY;
        AiLastVx = PlayersXv;
        AiLastVy = PlayersYv;
    } else {
        // he's cloaked: coast a ghost along his last-seen velocity, so ducking
        // under cloak and side-stepping no longer guarantees a clean escape -
        // the enemy leads the spot instead of parking on it
        AiLastX += AiLastVx;
        AiLastY += AiLastVy;

        if (AiLastX > UNIVERSE_WIDTH) {
            AiLastX = AiLastX - UNIVERSE_WIDTH;
        } else if (AiLastX < 0) {
            AiLastX = AiLastX + UNIVERSE_WIDTH;
        }

        if (AiLastY > UNIVERSE_HEIGHT) {
            AiLastY = AiLastY - UNIVERSE_HEIGHT;
        } else if (AiLastY < 0) {
            AiLastY = AiLastY + UNIVERSE_HEIGHT;
        }
    }

    // vector from the remote to its target - the player if visible, otherwise
    // the last-seen position - taking the shorter way around the wrapped universe
    dx = AiLastX - RemotesX;
    dy = AiLastY - RemotesY;

    if (dx > (UNIVERSE_WIDTH / 2)) {
        dx -= UNIVERSE_WIDTH;
    } else if (dx < -(UNIVERSE_WIDTH / 2)) {
        dx += UNIVERSE_WIDTH;
    }

    if (dy > (UNIVERSE_HEIGHT / 2)) {
        dy -= UNIVERSE_HEIGHT;
    } else if (dy < -(UNIVERSE_HEIGHT / 2)) {
        dy += UNIVERSE_HEIGHT;
    }

    // rough distance (Manhattan, avoids a square root)
    dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);

    // heading that points straight at the player - used for closing in
    huntFrame = aiHeading(dx, dy);

    // The firing solution: aim where the player WILL be when the missile gets
    // there, not where he is now. Relative to us the missile flies at ~6 units
    // a frame (2x the ~3-unit heading vector), so flight time is about dist / 6;
    // offset the aim by his velocity relative to ours over that time, then slip
    // it by this shot's aim error so the enemy is dangerous, not surgical.
    aimT = dist / 6;
    fireFrame = aiHeading(dx + (AiLastVx - RemotesXv) * aimT,
                          dy + (AiLastVy - RemotesYv) * aimT);
    fireFrame = (fireFrame + AiAimSlip) & 15;
    diffFire = (fireFrame - RemotesShip.currFrame) & 15;

    // tick the behavior timer and choose a new behavior when it expires, or
    // immediately when our energy runs low (so it stops bleeding power)
    if (AiStateTimer > 0) {
        AiStateTimer--;
    }

    if (AiStateTimer <= 0 ||
        (AiState != AI_FLEE && RemotesEnergy < AI_LOW_ENERGY)) {

        aiPickState();
    }

    // Backstop so the enemy never leaves the player's view: beyond the return
    // distance or the per-axis leash, override the behavior and close straight in.
    returning = (dist > AI_RETURN_DIST) ||
                ((dx < 0 ? -dx : dx) > AI_LEASH_X) ||
                ((dy < 0 ? -dy : dy) > AI_LEASH_Y);

    // --- the maneuver: the velocity we want RELATIVE to the player -------------
    relMx = 0;
    relMy = 0;

    if (returning) {
        // head straight back toward the player
        relMx = MotionDx[huntFrame] * AI_MANEUVER;
        relMy = MotionDy[huntFrame] * AI_MANEUVER;
    } else if (AiState == AI_HUNT) {
        // close to hunt distance, then hold station (pace the player) and fire
        if (dist > AI_HUNT_DIST) {
            relMx = MotionDx[huntFrame] * AI_MANEUVER;
            relMy = MotionDy[huntFrame] * AI_MANEUVER;
        }
    } else if (AiState == AI_STRAFE) {
        // circle: head tangentially. Only nudge in/out once the distance has
        // drifted outside a dead band around the strafe radius - inside the band
        // it's pure circling, so a hovering distance can't flip the steering and
        // make the ship twitch every frame.
        if (dist > AI_STRAFE_DIST + AI_STRAFE_BAND) {
            radial = 1;
        } else if (dist < AI_STRAFE_DIST - AI_STRAFE_BAND) {
            radial = -1;
        } else {
            radial = 0;
        }
        tx = AiOrbitDir * (-dy) + radial * (dx / 2);
        ty = AiOrbitDir * ( dx) + radial * (dy / 2);
        mFrame = aiHeading(tx, ty);
        relMx = MotionDx[mFrame] * AI_MANEUVER;
        relMy = MotionDy[mFrame] * AI_MANEUVER;
    } else { // AI_FLEE
        // back away, but only out to flee distance (then just pace the player)
        if (dist < AI_FLEE_DIST) {
            mFrame = aiHeading(-dx, -dy);
            relMx = MotionDx[mFrame] * AI_MANEUVER;
            relMy = MotionDy[mFrame] * AI_MANEUVER;
        }
    }

    // Asteroid avoidance overrides the maneuver: project our course a few
    // frames ahead (and each rock's), and if the nearest rock falls inside its
    // size plus a clearance band, steer straight away from it.
    aNear = 32767;
    tx = 0;
    ty = 0;

    for (m = 0; m < NUM_ASTEROIDS; m++) {
        if (Asteroids[m].rock.state == ASTEROID_ACTIVE) {
            adx = (Asteroids[m].x + Asteroids[m].xv * AI_AVOID_AHEAD
                       + Asteroids[m].rock.width / (2 * CAM_ZOOM))
                - (RemotesX + RemotesXv * AI_AVOID_AHEAD + SHIP_WIDTH / 2);
            ady = (Asteroids[m].y + Asteroids[m].yv * AI_AVOID_AHEAD
                       + Asteroids[m].rock.height / (2 * CAM_ZOOM))
                - (RemotesY + RemotesYv * AI_AVOID_AHEAD + SHIP_HEIGHT / 2);

            aDist = (adx < 0 ? -adx : adx) + (ady < 0 ? -ady : ady);

            if (aDist < Asteroids[m].rock.width / CAM_ZOOM + AI_AVOID_RANGE &&
                aDist < aNear) {
                aNear = aDist;
                tx = -adx;
                ty = -ady;
            }
        }
    }

    if (aNear < 32767) {
        mFrame = aiHeading(tx, ty);
        relMx = MotionDx[mFrame] * AI_MANEUVER;
        relMy = MotionDy[mFrame] * AI_MANEUVER;
    }

    // target world velocity = the player's velocity (so we pace it - or the
    // ghost's, while he's cloaked) + the maneuver
    targetVx = AiLastVx + relMx;
    targetVy = AiLastVy + relMy;

    // how far our current velocity is from that target
    errVx = targetVx - RemotesXv;
    errVy = targetVy - RemotesYv;
    errMag = (errVx < 0 ? -errVx : errVx) + (errVy < 0 ? -errVy : errVy);

    // If we need to change velocity, face the correction and thrust toward it. If
    // our velocity already matches, face the firing solution so we can shoot -
    // that's the usual case, and is what makes the movement look calm.
    if (errMag > AI_VEL_TOL) {
        desiredFrame = aiHeading(errVx, errVy);
    } else {
        desiredFrame = fireFrame;
    }

    // rotate one step the short way toward the desired heading
    diff = (desiredFrame - RemotesShip.currFrame) & 15;

    if (diff != 0) {
        if (diff <= 8) {
            keys += REMOTE_RIGHT;
        } else {
            keys += REMOTE_LEFT;
        }
    }

    // thrust only to correct the velocity, and only once pointed at the correction
    if (errMag > AI_VEL_TOL && (diff == 0 || diff == 1 || diff == 15)) {
        keys += REMOTE_THRUST;
    }

    // Fire when it can see the player, isn't fleeing, is roughly facing the
    // firing solution (within one heading step), is in range, off cooldown, and
    // has a shot slot and the energy to spare. A cloaked player can't be targeted.
    if (AiFireDelay > 0) {
        AiFireDelay--;
    }

    if (aiSees &&
        AiState != AI_FLEE &&
        (diffFire == 0 || diffFire == 1 || diffFire == 15) &&
        dist < AI_FIRE_RANGE &&
        RemotesEnergy > 100 &&
        RemotesActiveMissiles < AI_MAX_MISSILES &&
        AiFireDelay == 0) {

        keys += REMOTE_FIRE;

        // don't turn on the frame we fire: the sim rotates before launching the
        // missile, so a same-frame turn would swing the shot off the player
        keys &= ~(REMOTE_LEFT | REMOTE_RIGHT);

        AiFireDelay = AI_FIRE_COOLDOWN + rand() % AI_FIRE_JITTER;

        // roll the next shot's aim slip
        AiAimSlip = rand() % (2 * AI_AIM_ERROR + 1) - AI_AIM_ERROR;
    }

    // Raise shields only for a player missile that is genuinely bearing down -
    // close AND moving toward us - so the shielding is a deliberate dodge rather
    // than a flicker on every shot that drifts past. (And we can afford it, and
    // aren't cloaked, which the sim treats as exclusive with shields.)
    if (RemotesCloak == -1 &&
        RemotesShields == 0 &&
        RemotesShieldStrength > 0 &&
        RemotesShieldTime == 0 &&
        RemotesShieldCooldown == 0) {

        for (m = 0; m < NUM_MISSILES; m++) {
            if (Missiles[m].state == MISS_ACTIVE &&
                Missiles[m].type == PLAYER_MISSILE) {

                mdx = Missiles[m].x - RemotesX;
                mdy = Missiles[m].y - RemotesY;

                // in range, and its velocity points back toward us (the dot
                // product of the missile's velocity with the missile->remote
                // vector (-mdx,-mdy) is positive only when it's closing in)
                if (mdx > -AI_SHIELD_RANGE && mdx < AI_SHIELD_RANGE &&
                    mdy > -AI_SHIELD_RANGE && mdy < AI_SHIELD_RANGE &&
                    (Missiles[m].xv * (-mdx) + Missiles[m].yv * (-mdy)) > 0) {
                    keys += REMOTE_SHIELDS;
                    break;
                }
            }
        }
    }

    // The cloaking device: while FLEEing with energy to spare, cloak to break
    // the player's lock and reposition; decloak once ready to fight again (or
    // when energy runs low - the cloak bleeds power, and the sim's toggle is
    // gated on energy > 0, so waiting too long would leave it stuck cloaked).
    // The sim toggles on REMOTE_CLOAK, so press only when the state is wrong.
    if (RemotesCloak == 1 &&
        (AiState != AI_FLEE || RemotesEnergy < AI_LOW_ENERGY)) {
        keys += REMOTE_CLOAK;
    } else if (AiState == AI_FLEE &&
               RemotesCloak == -1 &&
               RemotesShields == 0 &&
               RemotesEnergy > AI_CLOAK_ENERGY) {
        keys += REMOTE_CLOAK;
    }

    return keys;
#endif
}

void resetSystem(void) {
    // this function resets everything so the game can be ran again
    // I hope I didn't leave anything out?

    // player variables
    Winner = WINNER_NONE;   // the winner of the game

    PlayersLastX = GameStartX[Master];
    PlayersLastY = GameStartY[Master];
    PlayersX = GameStartX[Master];
    PlayersY = GameStartY[Master];
    PlayersDx = 0;
    PlayersDy = 0;
    PlayersXv = 0;
    PlayersYv = 0;
    PlayersEngine = 0;
    PlayersStability = 8;
    PlayersFlameCount = 0;
    PlayersFlameTime = 1;
    PlayersShip.currFrame = 0;
    PlayersGravity = 0;
    PlayersShields = 0;
    PlayersShieldTime = 0;
    PlayersShieldCooldown = 0;
    PlayersCloak = -1;
    PlayersHeads = -1;
    PlayersComm = -1;
    PlayersScanner = -1;
    PlayersNumShips = 3;
    PlayersShieldStrength = 22000;
    PlayersEnergy = 22000;
    PlayersScore = 0;
    PlayersActiveMissiles = 0;
    PlayersState = ALIVE;
    PlayersDeathCount = 0;

    DebounceHud = 0;
    DebounceScan = 0;
    DebounceCloak = 0;
    DebounceThrust = 0;
    DebounceFire = 0;
    DebounceShields = 0;

    RefreshHeads = 0;

    // remote variables
    RemotesLastX = GameStartX[Slave];
    RemotesLastY = GameStartY[Slave];
    RemotesX = GameStartX[Slave];
    RemotesY = GameStartY[Slave];
    RemotesDx = 0;
    RemotesDy = 0;
    RemotesXv = 0;
    RemotesYv = 0;
    RemotesEngine = 0;
    RemotesStability = 8;
    RemotesFlameCount = 0;
    RemotesFlameTime = 1;
    RemotesGravity = 0;
    RemotesShields = 0;
    RemotesShieldTime  = 0;
    RemotesShieldCooldown = 0;
    RemotesCloak = -1;
    RemotesHeads = -1;
    RemotesComm = -1;
    RemotesScanner = -1;
    RemotesNumShips = 3;
    RemotesShieldStrength = 22000;
    RemotesEnergy = 22000;
    RemotesScore = 0;
    RemotesActiveMissiles = 0;
    RemotesState = ALIVE;
    RemotesDeathCount = 0;
#ifdef BLAZERX

    // AI state: in solo play give the player a brief moment before the enemy
    // opens fire. Timer 0 makes it pick a behavior on the first frame.
    AiFireDelay = AiEnabled ? AI_START_GRACE : 0;
    AiAimSlip = 0;
    AiState = AI_HUNT;
    AiStateTimer = 0;
    AiOrbitDir = 1;
    AiLastX = PlayersX;
    AiLastY = PlayersY;
    AiLastVx = 0;
    AiLastVy = 0;
#endif
}

#ifdef BLAZERX
// the scrolling light strip's fire-red gradient (from blazecon.pcx/
// blazeins.pcx's original register colors 32..41)
static const unsigned long PanelLightGradient[NUM_PANEL_LIGHTS] = {
    RGB32(255,0,0), RGB32(239,0,0), RGB32(227,0,0), RGB32(215,0,0), RGB32(203,0,0),
    RGB32(191,0,0), RGB32(179,0,0), RGB32(167,0,0), RGB32(155,0,0), RGB32(139,0,0),
};
#else
void panelFx(void) {
    // this function performs all of the special effects for the control panel
    int index;
#endif

#ifdef BLAZERX
void panelFx(int screen) {
    // this function performs all of the special effects for the control panel:
    // redraws the light strip as NUM_PANEL_LIGHTS segments along its long
    // axis, each showing PanelLightGradient[(segment + rotation) % N] - every
    // rotation step this shifts which gradient step each segment shows,
    // producing the same scrolling effect the register rotation used to.
    static int panelCounter = 0; // used to time the rotation
    static int rotation = 0;
    int i;
    int x1, x2, y1, y2, vertical;
#else
    static int panelCounter = 0; // used to time the color rotation of the panel
#endif

    // is it time to update colors?
#ifdef BLAZERX
    if (++panelCounter <= 2) {
        return;
    }
    panelCounter = 0;
    rotation = (rotation + 1) % NUM_PANEL_LIGHTS;
#else
    if (++panelCounter > 2) {
        // reset counter
        panelCounter = 0;
#endif

#ifdef BLAZERX
    if (screen == PANEL_FX_CONTROL) {
        x1 = PANEL_LIGHTS_CONTROL_X1; x2 = PANEL_LIGHTS_CONTROL_X2;
        y1 = PANEL_LIGHTS_CONTROL_Y1; y2 = PANEL_LIGHTS_CONTROL_Y2;
        vertical = 1;
    } else {
        x1 = PANEL_LIGHTS_BRIEFING_X1; x2 = PANEL_LIGHTS_BRIEFING_X2;
        y1 = PANEL_LIGHTS_BRIEFING_Y1; y2 = PANEL_LIGHTS_BRIEFING_Y2;
        vertical = 0;
    }
#else
        // do animation to colors
        readColorReg(END_PANEL_REG, &Color1);
#endif

#ifdef BLAZERX
    if (vertical) {
        int stripHeight = (y2 - y1 + 1) / NUM_PANEL_LIGHTS;
        for (i = 0; i < NUM_PANEL_LIGHTS; i++) {
            int segY1 = y1 + i * stripHeight;
            int segY2 = (i == NUM_PANEL_LIGHTS - 1) ? y2 : segY1 + stripHeight - 1;
            unsigned long color = PanelLightGradient[(i + rotation) % NUM_PANEL_LIGHTS];
            int yy;
            for (yy = segY1; yy <= segY2; yy++) {
                lineH2(x1, x2, yy, color, VideoBuffer);
            }
#else
        for (index = END_PANEL_REG; index > START_PANEL_REG; index--) {
            // read the (i-1)th register
            readColorReg(index - 1, &Color2);

            // assign it to the ith
            writeColorReg(index, &Color2);
#endif
        }
#ifdef BLAZERX
    } else {
        int stripWidth = (x2 - x1 + 1) / NUM_PANEL_LIGHTS;
        for (i = 0; i < NUM_PANEL_LIGHTS; i++) {
            int segX1 = x1 + i * stripWidth;
            int segX2 = (i == NUM_PANEL_LIGHTS - 1) ? x2 : segX1 + stripWidth - 1;
            unsigned long color = PanelLightGradient[(i + rotation) % NUM_PANEL_LIGHTS];
            int xx;
            for (xx = segX1; xx <= segX2; xx++) {
                lineV2(y1, y2, xx, color, VideoBuffer);
            }
        }
#else

        // place the value of the first color register into the last to complete the rotation
        writeColorReg(START_PANEL_REG, &Color1);
#endif
    }
}

void startExplosion(int x, int y, int speed) {
    // this function starts a generic explosion
    int index;

    // scan for a useable explosion
    for (index = 0; index < NUM_EXPLOSIONS; index++) {
        if (Explosions[index].state == EXPLOSION_INACTIVE) {
            // set up fields
            Explosions[index].state = EXPLOSION_ACTIVE;
            Explosions[index].x = 0;    // screen coordinates
            Explosions[index].y = 0;
            Explosions[index].counter2 = x; // the counters will be used as universe coordinates
            Explosions[index].counter3 = y;

            Explosions[index].currFrame = 0;
            Explosions[index].threshold1 = speed;
            Explosions[index].counter1 = 0;

            // make some sound
            digitalFxPlay(BLZEXP1_VOC, 1);

            break;
        }
    }
}

void underExplosions(void) {
    // this function scans under the explosions
    int index,
        pxWindow,
        pyWindow;

    // compute starting position of players window so screen mapping can be done
#ifdef VBE_SUPPORT
    pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
    pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;
#else
    pxWindow = PlayersX - 160 + 11;
    pyWindow = PlayersY - 100 + 9;
#endif

    // scan for a running explosions
    for (index = 0; index < NUM_EXPLOSIONS; index++) {
        if (Explosions[index].state == EXPLOSION_ACTIVE) {
            // position explosion correctly on view screen, note this is very similar
            // to what we will do in 3-D when we translate all the objects in the
            // universe to the viewer position, note counter2 and counter3
            // in the sprite structure are used as universe or world x,y
#ifdef VBE_SUPPORT
            Explosions[index].x = (Explosions[index].counter2 - pxWindow) * CAM_ZOOM;
            Explosions[index].y = (Explosions[index].counter3 - pyWindow) * CAM_ZOOM;
#else
            Explosions[index].x = Explosions[index].counter2 - pxWindow;
            Explosions[index].y = Explosions[index].counter3 - pyWindow;
#endif

            spriteUnderClip(&Explosions[index], DoubleBuffer);
        }
    }
}

void eraseExplosions(void) {
    // this function erases all the current explosions
    int index;

    // scan for a useable explosion
    for (index = 0; index < NUM_EXPLOSIONS; index++) {
        if (Explosions[index].state == EXPLOSION_ACTIVE) {
            spriteEraseClip(&Explosions[index], DoubleBuffer);
        }
    }
}

void drawExplosions(void) {
    // this function draws the explosions
    int index;

    // scan for a useable explosion
    for (index = 0; index < NUM_EXPLOSIONS; index++) {
        // make sure this explosion is alive
        if (Explosions[index].state == EXPLOSION_ACTIVE) {
            spriteDrawClip(&Explosions[index], DoubleBuffer, 1);
        }
    }
}

void animateExplosions(void) {
    // this function animates the explosions
    int index;

    // scan for a useable explosion
    for (index = 0; index < NUM_EXPLOSIONS; index++) {
        // test if explosion is active
        if (Explosions[index].state == EXPLOSION_ACTIVE) {
            // test if it's time to change frames
            if (++Explosions[index].counter1 >= Explosions[index].threshold1) {
                // is the explosion over?
                if (++Explosions[index].currFrame == NUM_EXPLOSION_FRAMES) {
                    Explosions[index].state = EXPLOSION_INACTIVE;
                }

                // reset animation clock for future
                Explosions[index].counter1 = 0;
            }
        }
    }
}

void initExplosions(void) {
    // clear out the state of all explosions
    int index;

    for (index = 0; index < NUM_EXPLOSIONS; index++) {
        Explosions[index].state = EXPLOSION_INACTIVE;
    }
}

void loadWormhole(void) {
    // this function loads in the imagery for the wormhole
    int index;

    // load the imagery for the wormhole
    pcxInit(&ImagePcx);
    pcxLoad("blazewrm.pcx", &ImagePcx, 1);

    // initialize the wormhole sprite
#ifdef VBE_SUPPORT
    spriteInit(&Wormhole, 0, 0, WORMHOLE_WIDTH * CAM_ZOOM, WORMHOLE_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&Wormhole, 0, 0, 26, 22, 0, 0, 0, 0, 0, 0);
#endif
#ifdef BLAZERX
    Wormhole.transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif

    // extract the animation frames
    for (index = 0; index < NUM_WORMHOLE_FRAMES; index++) {
        pcxGetSprite(&ImagePcx, &Wormhole, index, index, 0);
    }

    pcxDelete(&ImagePcx);
}

void initWormhole(void) {
    // this resets all the wormhole parameters

    // set screen coordinates to 0
    Wormhole.x = 0;
    Wormhole.y = 0;

    // set universe coordinates to proper position
    Wormhole.counter2 = WORMHOLE_X; // note the counters are being used for universe positions
    Wormhole.counter3 = WORMHOLE_Y;

    // reset the frame counter
    Wormhole.currFrame = 0;

    // these will be used to time animation
    Wormhole.counter1 = 0;
    Wormhole.threshold1 = 2;
}

void underWormhole(void) {
    // this function scans under the wormhole
    int pxWindow,
        pyWindow;

    // compute starting position of players window so screen mapping can be done
#ifdef VBE_SUPPORT
    pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
    pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;
#else
    pxWindow = PlayersX - 160 + 11;
    pyWindow = PlayersY - 100 + 9;
#endif

    // translate wormhole to screen coordinates
#ifdef VBE_SUPPORT
    Wormhole.x = (Wormhole.counter2 - pxWindow) * CAM_ZOOM;
    Wormhole.y = (Wormhole.counter3 - pyWindow) * CAM_ZOOM;
#else
    Wormhole.x = Wormhole.counter2 - pxWindow;
    Wormhole.y = Wormhole.counter3 - pyWindow;
#endif

    spriteUnderClip(&Wormhole, DoubleBuffer);
}

void eraseWormhole(void) {
    // this function erases the wormhole
    spriteEraseClip(&Wormhole, DoubleBuffer);
}

void drawWormhole(void) {
    // this function draws the wormhole
    spriteDrawClip(&Wormhole, DoubleBuffer, 1);
}

void animateWormhole(void) {
    // this function animates the wormhole
    if (++Wormhole.counter1 >= Wormhole.threshold1) {
        // time to reset frame counter?
        if (++Wormhole.currFrame == NUM_WORMHOLE_FRAMES) {
            Wormhole.currFrame = 0;
        }

        // reset animation clock for future
        Wormhole.counter1 = 0;
    }
}

void loadFuelCells(void) {
    // this function loads in the imagery for the fuel cells
    int index,
        frames;

    // load the imagery for the fuel cells
    pcxInit(&ImagePcx);
    pcxLoad("blazeful.pcx", &ImagePcx, 1);

    // initialize the fuel cells sprite and load bitmaps
    for (index = 0; index < NUM_FUEL_CELLS; index++) {
#ifdef VBE_SUPPORT
        spriteInit(&FuelCells[index], 0, 0,
                       FUEL_CELL_WIDTH * CAM_ZOOM, FUEL_CELL_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
#else
        spriteInit(&FuelCells[index], 0, 0, 20, 18, 0, 0, 0, 0, 0, 0);
#endif
#ifdef BLAZERX
        FuelCells[index].transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif

        // extract the animation frames
        for (frames = 0; frames < NUM_FUEL_FRAMES; frames++) {
            pcxGetSprite(&ImagePcx, &FuelCells[index], frames, frames, 0);
        }
    }

    pcxDelete(&ImagePcx);
}

void initFuelCells(void) {
    // this resets all the fuel cell parameters
    int index;

    for (index = 0; index < NUM_FUEL_CELLS; index++) {
        // set state
        FuelCells[index].state = FUEL_CELL_ACTIVE;

        // set screen coordinates to 0
        FuelCells[index].x = 0;
        FuelCells[index].y = 0;

        // set universe coordinates to random position
        FuelCells[index].counter2 = 200 + rand() % (UNIVERSE_WIDTH - 400);
        FuelCells[index].counter3 = 200 + rand() % (UNIVERSE_HEIGHT - 400);

        // reset the frame counter
        FuelCells[index].currFrame = 0;

        // these will be used to time animation
        FuelCells[index].counter1 = 0;
        FuelCells[index].threshold1 = 2;
    }
}

void underFuelCells(void) {
    // this function scans under the fuel cells
    int pxWindow,
        pyWindow,
        index;

    // compute starting position of players window so screen mapping can be done
#ifdef VBE_SUPPORT
    pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
    pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;
#else
    pxWindow = PlayersX - 160 + 11;
    pyWindow = PlayersY - 100 + 9;
#endif

    // process each fuel cell
    for (index = 0; index < NUM_FUEL_CELLS; index++) {
        // test if fuel cell is active
        if (FuelCells[index].state == FUEL_CELL_ACTIVE) {
            // translate fuel cells to screen coordinates
#ifdef VBE_SUPPORT
            FuelCells[index].x = (FuelCells[index].counter2 - pxWindow) * CAM_ZOOM;
            FuelCells[index].y = (FuelCells[index].counter3 - pyWindow) * CAM_ZOOM;
#else
            FuelCells[index].x = FuelCells[index].counter2 - pxWindow;
            FuelCells[index].y = FuelCells[index].counter3 - pyWindow;
#endif

            spriteUnderClip(&FuelCells[index], DoubleBuffer);
        }
    }
}

void eraseFuelCells(void) {
    // this function erases the fuel cells
    int index;

    for (index = 0; index < NUM_FUEL_CELLS; index++) {
        // test if fuel cell is active
        if (FuelCells[index].state == FUEL_CELL_ACTIVE) {
            spriteEraseClip(&FuelCells[index], DoubleBuffer);
        }
    }
}

void drawFuelCells(void) {
    // this function draws the fuel cells
    int index;

    for (index = 0; index < NUM_FUEL_CELLS; index++) {
        // test if fuel cell is active
        if (FuelCells[index].state == FUEL_CELL_ACTIVE) {
            spriteDrawClip(&FuelCells[index], DoubleBuffer, 1);
        }
    }
}

void animateFuelCells(void) {
    // this function animates the fuel cells and tests for collision
    int index;

    for (index = 0; index < NUM_FUEL_CELLS; index++) {
        // test if fuel cell is active
        if (FuelCells[index].state == FUEL_CELL_ACTIVE) {
            // move cell to right
            if (++FuelCells[index].counter2 > UNIVERSE_WIDTH + UNIVERSE_BORDER) {
                FuelCells[index].counter2 = -UNIVERSE_BORDER;
            }

            // perform animation
            if (++FuelCells[index].counter1 >= FuelCells[index].threshold1) {
                // time to reset frame counter?
                if (++FuelCells[index].currFrame == NUM_FUEL_FRAMES) {
                    FuelCells[index].currFrame = 0;
                }

                // reset animation clock for future
                FuelCells[index].counter1 = 0;
            }
        }
    }
}

void loadAlien(void) {
    // this function loads in the imagery for the alien
    int index;

    // load the imagery for the alien
    pcxInit(&ImagePcx);
    pcxLoad("blazealn.pcx", &ImagePcx, 1);

    // initialize the alien sprite
#ifdef VBE_SUPPORT
    spriteInit(&Alien.body, 0, 0, ALIEN_WIDTH * CAM_ZOOM, ALIEN_HEIGHT * CAM_ZOOM, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&Alien.body, 0, 0, 14, 8, 0, 0, 0, 0, 0, 0);
#endif
#ifdef BLAZERX
    Alien.body.transparentColor = SPRITE_TRANSPARENT_COLOR;
#endif

    // extract the animation frames
    for (index = 0; index < NUM_ALIEN_FRAMES; index++) {
        pcxGetSprite(&ImagePcx, &Alien.body, index, index, 0);
    }

    pcxDelete(&ImagePcx);
}

void initAlien(void) {
    // this resets all the alien parameters

    // set screen coordinates to 0
    Alien.body.x =0;
    Alien.body.y = 0;

    // set universe coordinates to proper position
    Alien.x = 0;
    Alien.y = 0;

    // reset the frame counter
    Alien.body.currFrame = 0;

    // these will be used to time animation
    Alien.body.counter1 = 0;
    Alien.body.threshold1 = 2;

    // set state to active
    Alien.state = ALIEN_INACTIVE;
}

void alienControl(void) {
    // this function will be called every cycle and decide if an alien will start up

    // make sure alien is dead
    if (Alien.state == ALIEN_INACTIVE) {
        // throw a coin
        if (rand() % ALIEN_ODDS == 1) {
            // position alien and set appropriate fields
            Alien.body.x = 0;   // screen coords
            Alien.body.y = 0;

            // start alien from wormhole (universe coords)
            Alien.x = WORMHOLE_X;   // note the counters are being used for universe positions
            Alien.y = WORMHOLE_Y;

            // reset the frame counter
            Alien.body.currFrame = 0;

            // this will be used to time animation
            Alien.body.counter1 = 0;
            Alien.body.threshold1 = 2;

            // set state to random to start with
            Alien.state = ALIEN_RANDOM;

            // select a random direction
            Alien.xv = -4 + rand() % 8;
            Alien.yv = -4 + rand() % 8;

            // counter 2 will be used to track how long to stay in a state
            Alien.body.counter2 = 50;
        }
    }
}

void underAlien(void) {
    // this function scans under the alien
    int pxWindow,
        pyWindow;

    if (Alien.state != ALIEN_INACTIVE) {
        // compute starting position of players window so screen mapping can be done
#ifdef VBE_SUPPORT
        pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
        pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;
#else
        pxWindow = PlayersX - 160 + 11;
        pyWindow = PlayersY - 100 + 9;
#endif

        // translate alien to screen coordinates
#ifdef VBE_SUPPORT
        Alien.body.x = (Alien.x - pxWindow) * CAM_ZOOM;
        Alien.body.y = (Alien.y - pyWindow) * CAM_ZOOM;
#else
        Alien.body.x = Alien.x - pxWindow;
        Alien.body.y = Alien.y - pyWindow;
#endif

        // perform scan in screen coords
        spriteUnderClip(&Alien.body, DoubleBuffer);
    }
}

void eraseAlien(void) {
    // this function erases the alien
    if (Alien.state != ALIEN_INACTIVE) {
        spriteEraseClip(&Alien.body, DoubleBuffer);
    }
}

void drawAlien(void) {
    // this function draws the alien
    if (Alien.state != ALIEN_INACTIVE) {
        spriteDrawClip(&Alien.body, DoubleBuffer, 1);
    }
}

void moveAlien(void) {
    // this function moves the alien (if there is one)
    if (Alien.state != ALIEN_INACTIVE) {
        // what state is alien in?
        switch (Alien.state) {
            case ALIEN_RANDOM: {
                // move alien in direction
                Alien.x += Alien.xv;
                Alien.y += Alien.yv;
            } break;

            case ALIEN_CHASE_PLAYER: {
                
            } break;

            case ALIEN_CHASE_REMOTE: {

            } break;

            default:
                break;
        }

        // decrement state counter
        if (--Alien.body.counter2 <= 0) {
            // select a new state
            Alien.state = ALIEN_RANDOM; // change this line later

            // set up new state
            switch (Alien.state) {
                case ALIEN_RANDOM: {
                    // select new random direction and state time
                    Alien.xv = -4 + rand() % 8;
                    Alien.yv = -4 + rand() % 8;

                    // counter 2 will be used to track how long to stay in a state
                    Alien.body.counter2 = 25 + rand() % 75;
                } break;

                case ALIEN_CHASE_PLAYER: {
                    // this bud's for you!
                } break;

                case ALIEN_CHASE_REMOTE: {
                    // this is an exercise
                } break;

                default:
                    break;
            }
        }

        // animate alien
        if (++Alien.body.counter1 >= Alien.body.threshold1) {
            // change animation frames
            if (++Alien.body.currFrame == NUM_ALIEN_FRAMES) {
                Alien.body.currFrame = 0;
            }

            // reset animation counter
            Alien.body.counter1 = 0;
        }

        // do collision detection
        if (Alien.x > UNIVERSE_WIDTH + UNIVERSE_BORDER ||
            Alien.x < -UNIVERSE_BORDER ||
            Alien.y > UNIVERSE_HEIGHT + UNIVERSE_BORDER ||
            Alien.y < -UNIVERSE_BORDER) {

            // kill alien
            Alien.state = ALIEN_INACTIVE;
        }
    }
}

void loadHeads(void) {
    // this function loads various icons for the heads up display
    int index;

    // load imagery for the icons for display
    pcxInit(&ImagePcx);
    pcxLoad("blazehu1.pcx", &ImagePcx, 1);

    // initialize the button sprite
#ifdef BLAZERX
    spriteInit(&HeadsText, 0, 0, 109, 19, 0, 0, 0, 0, 0, 0);
    HeadsText.transparentColor = SPRITE_TRANSPARENT_COLOR;
#else
#ifdef VBE_SUPPORT
    spriteInit(&HeadsText, 0, 0, 68, 12, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&HeadsText, 0, 0, 34, 6, 0, 0, 0, 0, 0, 0);
#endif
#endif

    // extract the bitmaps for heads up text
    for (index = 0; index < 7; index++) {
        pcxGetSprite(&ImagePcx, &HeadsText, index, index, 0);
    }

    // delete pcx file
    pcxDelete(&ImagePcx);

    // load the imagery for the icons for display
    pcxInit(&ImagePcx);
    pcxLoad("blazehu2.pcx", &ImagePcx, 1);

    // initialize the button sprite
#ifdef BLAZERX
    spriteInit(&HeadsNumbers, 0, 0, 26, 19, 0, 0, 0, 0, 0, 0);
    HeadsNumbers.transparentColor = SPRITE_TRANSPARENT_COLOR;
#else
#ifdef VBE_SUPPORT
    spriteInit(&HeadsNumbers, 0, 0, 16, 12, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&HeadsNumbers, 0, 0, 8, 6, 0, 0, 0, 0, 0, 0);
#endif
#endif

    // extract the bitmaps for heads up text
    for (index = 0; index < 7; index++) {
        pcxGetSprite(&ImagePcx, &HeadsNumbers, index, index, 0);
    }

    // delete pcx file
    pcxDelete(&ImagePcx);

    // load the imagery for the icons for display
    pcxInit(&ImagePcx);
    pcxLoad("blazehu1.pcx", &ImagePcx, 1);

    // initialize the button sprite
#ifdef BLAZERX
    spriteInit(&HeadsGauge, 0, 0, 109, 19, 0, 0, 0, 0, 0, 0);
    HeadsGauge.transparentColor = SPRITE_TRANSPARENT_COLOR;
#else
#ifdef VBE_SUPPORT
    spriteInit(&HeadsGauge, 0, 0, 68, 12, 0, 0, 0, 0, 0, 0);
#else
    spriteInit(&HeadsGauge, 0, 0, 34, 6, 0, 0, 0, 0, 0, 0);
#endif
#endif

    // extract the bitmaps for heads up gauges
    for (index = 0; index < 23; index++) {
        pcxGetSprite(&ImagePcx, &HeadsGauge, index, index % 9, 2 + index / 9);
    }

    // delete pcx file
    pcxDelete(&ImagePcx);
}

void initHeads(void) {
    // do nothing for now
}

void drawHeads(void) {
    // this function draws the heads up display during the time when
    // the game background can be modified
    int index;

    HeadsText.x = LEFT_HEADS_TEXT_X;
    HeadsText.y = LEFT_HEADS_TEXT_Y;

    // draw the left most messages first
    HeadsText.currFrame = HEADS_CLOAK;
    
    for (index = 0; index < 4; index++) {
        spriteDraw(&HeadsText, DoubleBuffer, 0);

        // move down a row
#ifdef BLAZERX
        HeadsText.y += 26;
#else
#ifdef VBE_SUPPORT
        HeadsText.y += 16;
#else
        HeadsText.y += 8;
#endif
#endif

        // select next message
        HeadsText.currFrame++;
    }

    // now draw buttons and numbers
#ifdef BLAZERX
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 128;
#else
#ifdef VBE_SUPPORT
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 80;
#else
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 40;
#endif
#endif
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y;

    // draw cloaked button
    if (PlayersCloak == 1) {
        HeadsNumbers.currFrame = 2;
        spriteDraw(&HeadsNumbers, DoubleBuffer, 0);
    } else {
        HeadsNumbers.currFrame = 1;
        spriteDraw(&HeadsNumbers, DoubleBuffer, 0);
    }

    // draw scanner enable
#ifdef BLAZERX
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 128;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 26;
#else
#ifdef VBE_SUPPORT
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 80;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 16;
#else
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 40;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 8;
#endif
#endif

    if (PlayersScanner == 1) {
        HeadsNumbers.currFrame = 2;
        spriteDraw(&HeadsNumbers, DoubleBuffer, 0);
    } else {
        HeadsNumbers.currFrame = 1;
        spriteDraw(&HeadsNumbers, DoubleBuffer, 0);
    }

    // draw communications
#ifdef BLAZERX
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 128;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 51;
#else
#ifdef VBE_SUPPORT
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 80;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 32;
#else
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 40;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 16;
#endif
#endif

    if (Linked) {
        HeadsNumbers.currFrame = 2;
        spriteDraw(&HeadsNumbers, DoubleBuffer, 0);
    } else {
        HeadsNumbers.currFrame = 1;
        spriteDraw(&HeadsNumbers, DoubleBuffer, 0);
    }

    // draw number of ships
#ifdef BLAZERX
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 128;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 77;
#else
#ifdef VBE_SUPPORT
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 80;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 48;
#else
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 40;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 24;
#endif
#endif

    HeadsNumbers.currFrame = 3 + PlayersNumShips;
    spriteDraw(&HeadsNumbers, DoubleBuffer, 0);

    // now draw right most messages
    HeadsText.x = RIGHT_HEADS_TEXT_X;
    HeadsText.y = RIGHT_HEADS_TEXT_Y;

    HeadsText.currFrame = HEADS_ENERGY;

    for (index = 0; index < 2; index++) {
        spriteDraw(&HeadsText, DoubleBuffer, 0);

        // move down a row
#ifdef BLAZERX
        HeadsText.y += 26;
#else
#ifdef VBE_SUPPORT
        HeadsText.y += 16;
#else
        HeadsText.y += 8;
#endif
#endif

        // select next message
        HeadsText.currFrame++;
    }

    // draw gauges
#ifdef BLAZERX
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 128;
#else
#ifdef VBE_SUPPORT
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 80;
#else
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 40;
#endif
#endif
    HeadsGauge.y = RIGHT_HEADS_TEXT_Y;

    // compute proper frame
    HeadsGauge.currFrame = 22 - PlayersEnergy / 1000;

    // draw the energy level
    spriteDraw(&HeadsGauge, DoubleBuffer, 0);

#ifdef BLAZERX
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 128;
    HeadsGauge.y = RIGHT_HEADS_TEXT_Y + 26;
#else
#ifdef VBE_SUPPORT
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 80;
    HeadsGauge.y = RIGHT_HEADS_TEXT_Y + 16;
#else
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 40;
    HeadsGauge.y = RIGHT_HEADS_TEXT_Y + 8;
#endif
#endif

    // compute proper frame
    HeadsGauge.currFrame = 22 - PlayersShieldStrength / 1000;

    // draw the shield strength
    spriteDraw(&HeadsGauge, DoubleBuffer, 0);
}

void eraseHeads(void) {
    // this function erases the heads up display during the time when
    // the game background can be modified
    int index;

    HeadsText.x = LEFT_HEADS_TEXT_X;
    HeadsText.y = LEFT_HEADS_TEXT_Y;

    // draw the left most messages first
    HeadsText.currFrame = 0;

    for (index = 0; index < 4; index++) {
        spriteDraw(&HeadsText, DoubleBuffer, 0);

        // move down a row
#ifdef BLAZERX
        HeadsText.y += 26;
#else
#ifdef VBE_SUPPORT
        HeadsText.y += 16;
#else
        HeadsText.y += 8;
#endif
#endif
    }

    // now erase buttons and numbers
#ifdef BLAZERX
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 128;
#else
#ifdef VBE_SUPPORT
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 80;
#else
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 40;
#endif
#endif
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y;

    // erase cloaked button
    HeadsNumbers.currFrame = 0;
    spriteDraw(&HeadsNumbers, DoubleBuffer, 0);

    // erase scanner
#ifdef BLAZERX
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 128;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 26;
#else
#ifdef VBE_SUPPORT
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 80;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 16;
#else
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 40;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 8;
#endif
#endif

    spriteDraw(&HeadsNumbers, DoubleBuffer, 0);

    // erase communications
#ifdef BLAZERX
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 128;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 51;
#else
#ifdef VBE_SUPPORT
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 80;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 32;
#else
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 40;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 16;
#endif
#endif

    spriteDraw(&HeadsNumbers, DoubleBuffer, 0);

    // erase ships number
#ifdef BLAZERX
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 128;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 77;
#else
#ifdef VBE_SUPPORT
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 80;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 48;
#else
    HeadsNumbers.x = LEFT_HEADS_TEXT_X + 40;
    HeadsNumbers.y = LEFT_HEADS_TEXT_Y + 24;
#endif
#endif

    spriteDraw(&HeadsNumbers, DoubleBuffer, 0);

    // now draw right most messages
    HeadsText.x = RIGHT_HEADS_TEXT_X;
    HeadsText.y = RIGHT_HEADS_TEXT_Y;

    // draw the left most messages first
    for (index = 0; index < 2; index++) {
        spriteDraw(&HeadsText, DoubleBuffer, 0);

        // move down a row
#ifdef BLAZERX
        HeadsText.y += 26;
#else
#ifdef VBE_SUPPORT
        HeadsText.y += 16;
#else
        HeadsText.y += 8;
#endif
#endif
    }

    // erase gauges
#ifdef BLAZERX
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 128;
#else
#ifdef VBE_SUPPORT
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 80;
#else
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 40;
#endif
#endif
    HeadsGauge.y = RIGHT_HEADS_TEXT_Y;

    // compute proper frame
    HeadsGauge.currFrame = 22;

    // draw the energy level
    spriteDraw(&HeadsGauge, DoubleBuffer, 0);

    // draw gauges
#ifdef BLAZERX
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 128;
    HeadsGauge.y = RIGHT_HEADS_TEXT_Y + 26;
#else
#ifdef VBE_SUPPORT
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 80;
    HeadsGauge.y = RIGHT_HEADS_TEXT_Y + 16;
#else
    HeadsGauge.x = RIGHT_HEADS_TEXT_X + 40;
    HeadsGauge.y = RIGHT_HEADS_TEXT_Y + 8;
#endif
#endif

    // erase the shield strength
    spriteDraw(&HeadsGauge, DoubleBuffer, 0);
}

void initScanner(void) {
    // this function initializes the scanner

    // not much to do at this point!
}

void eraseScanner(void) {
    // this function erases the scanner and the blips

    // first erase scanner grid
#ifdef BLAZERX
    lineH2Thick(SCANNER_X, SCANNER_X + 205, SCANNER_Y,      SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineH2Thick(SCANNER_X, SCANNER_X + 205, SCANNER_Y + 61, SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineH2Thick(SCANNER_X, SCANNER_X + 205, SCANNER_Y + 122, SCANNER_LINE_THICK, 0, DoubleBuffer);

    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X,       SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X + 51,  SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X + 102,  SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X + 154,  SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X + 205, SCANNER_LINE_THICK, 0, DoubleBuffer);
#else
#ifdef VBE_SUPPORT
    lineH2Thick(SCANNER_X, SCANNER_X + 128, SCANNER_Y,      SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineH2Thick(SCANNER_X, SCANNER_X + 128, SCANNER_Y + 38, SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineH2Thick(SCANNER_X, SCANNER_X + 128, SCANNER_Y + 76, SCANNER_LINE_THICK, 0, DoubleBuffer);

    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X,       SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X + 32,  SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X + 64,  SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X + 96,  SCANNER_LINE_THICK, 0, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X + 128, SCANNER_LINE_THICK, 0, DoubleBuffer);
#else
    lineH2(SCANNER_X, SCANNER_X + 64, SCANNER_Y,      0, DoubleBuffer);
    lineH2(SCANNER_X, SCANNER_X + 64, SCANNER_Y + 16, 0, DoubleBuffer);
    lineH2(SCANNER_X, SCANNER_X + 64, SCANNER_Y + 32, 0, DoubleBuffer);

    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X,      0, DoubleBuffer);
    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X + 16, 0, DoubleBuffer);
    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X + 32, 0, DoubleBuffer);
    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X + 48, 0, DoubleBuffer);
    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X + 64, 0, DoubleBuffer);
#endif
#endif
}

void drawScanner(void) {
    // this function draws the scanner and the blips

    // first draw scanner grid
#ifdef BLAZERX
    lineH2Thick(SCANNER_X, SCANNER_X + 205, SCANNER_Y,      SCANNER_LINE_THICK, SCANNER_GRID_COLOR, DoubleBuffer);
    lineH2Thick(SCANNER_X, SCANNER_X + 205, SCANNER_Y + 61, SCANNER_LINE_THICK, SCANNER_GRID_COLOR, DoubleBuffer);
    lineH2Thick(SCANNER_X, SCANNER_X + 205, SCANNER_Y + 122, SCANNER_LINE_THICK, SCANNER_GRID_COLOR, DoubleBuffer);

    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X,       SCANNER_LINE_THICK, SCANNER_GRID_COLOR, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X + 51,  SCANNER_LINE_THICK, SCANNER_GRID_COLOR, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X + 102,  SCANNER_LINE_THICK, SCANNER_GRID_COLOR, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X + 154,  SCANNER_LINE_THICK, SCANNER_GRID_COLOR, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 122, SCANNER_X + 205, SCANNER_LINE_THICK, SCANNER_GRID_COLOR, DoubleBuffer);
#else
#ifdef VBE_SUPPORT
    lineH2Thick(SCANNER_X, SCANNER_X + 128, SCANNER_Y,      SCANNER_LINE_THICK, 10, DoubleBuffer);
    lineH2Thick(SCANNER_X, SCANNER_X + 128, SCANNER_Y + 38, SCANNER_LINE_THICK, 10, DoubleBuffer);
    lineH2Thick(SCANNER_X, SCANNER_X + 128, SCANNER_Y + 76, SCANNER_LINE_THICK, 10, DoubleBuffer);

    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X,       SCANNER_LINE_THICK, 10, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X + 32,  SCANNER_LINE_THICK, 10, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X + 64,  SCANNER_LINE_THICK, 10, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X + 96,  SCANNER_LINE_THICK, 10, DoubleBuffer);
    lineV2Thick(SCANNER_Y, SCANNER_Y + 76, SCANNER_X + 128, SCANNER_LINE_THICK, 10, DoubleBuffer);
#else
    lineH2(SCANNER_X, SCANNER_X + 64, SCANNER_Y,      10, DoubleBuffer);
    lineH2(SCANNER_X, SCANNER_X + 64, SCANNER_Y + 16, 10, DoubleBuffer);
    lineH2(SCANNER_X, SCANNER_X + 64, SCANNER_Y + 32, 10, DoubleBuffer);

    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X,      10, DoubleBuffer);
    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X + 16, 10, DoubleBuffer);
    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X + 32, 10, DoubleBuffer);
    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X + 48, 10, DoubleBuffer);
    lineV2(SCANNER_Y, SCANNER_Y + 32, SCANNER_X + 64, 10, DoubleBuffer);
#endif
#endif
}

#ifdef VBE_SUPPORT
void blipSave(int x, int y, int* under) {
    // save the BLIP_SIZE x BLIP_SIZE block of pixels under a blip position
    int dx, dy, i = 0;
    for (dy = 0; dy < BLIP_SIZE; dy++) {
        for (dx = 0; dx < BLIP_SIZE; dx++) {
            under[i++] = readPixelDb(x - BLIP_SIZE / 2 + dx, y - BLIP_SIZE / 2 + dy);
        }
    }
}

void blipDraw(int x, int y, int color) {
    // draw a BLIP_SIZE x BLIP_SIZE filled block, centered on (x,y)
    int dx, dy;
    for (dy = 0; dy < BLIP_SIZE; dy++) {
        for (dx = 0; dx < BLIP_SIZE; dx++) {
            writePixelDb(x - BLIP_SIZE / 2 + dx, y - BLIP_SIZE / 2 + dy, color);
        }
    }
}

void blipRestore(int x, int y, const int* under) {
    // restore a block saved by blipSave
    int dx, dy, i = 0;
    for (dy = 0; dy < BLIP_SIZE; dy++) {
        for (dx = 0; dx < BLIP_SIZE; dx++) {
            writePixelDb(x - BLIP_SIZE / 2 + dx, y - BLIP_SIZE / 2 + dy, under[i++]);
        }
    }
}
#endif

void underBlips(void) {
    // this function is used to draw all the scanner blips

    // draw blips, notice that the position of the remote and player is
    // scaled to fit into the scanner window
#ifdef BLAZERX
    blipSave(SCANNER_X + PlayersX / 13, SCANNER_Y + PlayersY / 21, UnderPlayersBlip);
    blipSave(SCANNER_X + RemotesX / 13, SCANNER_Y + RemotesY / 21, UnderRemotesBlip);
#else
#ifdef VBE_SUPPORT
    blipSave(SCANNER_X + PlayersX / 20, SCANNER_Y + PlayersY / 34, UnderPlayersBlip);
    blipSave(SCANNER_X + RemotesX / 20, SCANNER_Y + RemotesY / 34, UnderRemotesBlip);
#else
    UnderPlayersBlip = readPixelDb(SCANNER_X + PlayersX / 40, SCANNER_Y + PlayersY / 80);
    UnderRemotesBlip = readPixelDb(SCANNER_X + RemotesX / 40, SCANNER_Y + RemotesY / 80);
#endif
#endif
}

void drawBlips(void) {
    // this function is used to draw all the scanner blips

    // draw blips, notice that the position of the remote and player is
    // scaled to fit into the scanner window
    if (PlayersCloak == -1) {
#ifdef BLAZERX
        blipDraw(SCANNER_X + PlayersX / 13, SCANNER_Y + PlayersY / 21, RGB32(85,85,255));
#else
#ifdef VBE_SUPPORT
        blipDraw(SCANNER_X + PlayersX / 20, SCANNER_Y + PlayersY / 34, 9);
#else
        writePixelDb(SCANNER_X + PlayersX / 40, SCANNER_Y + PlayersY / 80, 9);
#endif
#endif
    }

    if (RemotesCloak == -1) {
#ifdef BLAZERX
        blipDraw(SCANNER_X + RemotesX / 13, SCANNER_Y + RemotesY / 21, BLIP_REMOTE);
#else
#ifdef VBE_SUPPORT
        blipDraw(SCANNER_X + RemotesX / 20, SCANNER_Y + RemotesY / 34, 12);
#else
        writePixelDb(SCANNER_X + RemotesX / 40, SCANNER_Y + RemotesY / 80, 12);
#endif
#endif
    }
}

void eraseBlips(void) {
    // this function is used to erase all the scanner blips

    // erase blips, notice that the position of the remote and player is
    // scaled to fit into the scanner window
#ifdef BLAZERX
    blipRestore(SCANNER_X + PlayersX / 13, SCANNER_Y + PlayersY / 21, UnderPlayersBlip);
    blipRestore(SCANNER_X + RemotesX / 13, SCANNER_Y + RemotesY / 21, UnderRemotesBlip);
#else
#ifdef VBE_SUPPORT
    blipRestore(SCANNER_X + PlayersX / 20, SCANNER_Y + PlayersY / 34, UnderPlayersBlip);
    blipRestore(SCANNER_X + RemotesX / 20, SCANNER_Y + RemotesY / 34, UnderRemotesBlip);
#else
    writePixelDb(SCANNER_X + PlayersX / 40, SCANNER_Y + PlayersY / 80, UnderPlayersBlip);
    writePixelDb(SCANNER_X + RemotesX / 40, SCANNER_Y + RemotesY / 80, UnderRemotesBlip);
#endif
#endif
}

void musicInit(void) {
    // this function loads the music and resets all the indexes
    static int loaded = 0;

    // has the music already been loaded
    if (!MusicEnabled) {
        return;
    }

    if (!loaded) {
        musicLoad("blazemus.xmi", &Song);
        loaded = 1;
    }

    // reset sequence counters
    GameSeqIndex = 0;
    IntroSeqIndex = 0;
}

void musicClose(void) {
    // this function unloads the music files
    if (!MusicEnabled) {
        return;
    }

    // turn off music and unload song
    musicStop();
    musicUnload(&Song);
}

void digitalFxInit(void) {
    // this function initializes the digital sound fx system
    static int loaded = 0;

    if (!DigitalEnabled) {
        return;
    }

    // have the sound fx been loaded?
    if (!loaded) {
        // load int sounds
        soundLoad("BLZCLK.VOC",  &DigitalFx[BLZCLK_VOC],  1);
        soundLoad("BLZEXP1.VOC", &DigitalFx[BLZEXP1_VOC], 1);
        soundLoad("BLZEXP2.VOC", &DigitalFx[BLZEXP2_VOC], 1);
        soundLoad("BLZLAS.VOC",  &DigitalFx[BLZLAS_VOC],  1);
        soundLoad("BLZNRG.VOC",  &DigitalFx[BLZNRG_VOC],  1);
        soundLoad("BLZSHLD.VOC", &DigitalFx[BLZSHLD_VOC], 1);
        soundLoad("BLZTAC.VOC",  &DigitalFx[BLZTAC_VOC],  1);
        soundLoad("BLZSCN.VOC",  &DigitalFx[BLZSCN_VOC],  1);
        soundLoad("BLZMISS.VOC", &DigitalFx[BLZMISS_VOC], 1);

        soundLoad("BLZBIOS.VOC", &DigitalFx[BLZBIOS_VOC], 1);
        soundLoad("BLZENTR.VOC", &DigitalFx[BLZENTR_VOC], 1);
        soundLoad("BLZABRT.VOC", &DigitalFx[BLZABRT_VOC], 1);
        soundLoad("BLZSEL.VOC",  &DigitalFx[BLZSEL_VOC],  1);
        soundLoad("BLZKEY.VOC",  &DigitalFx[BLZKEY_VOC],  1);
        soundLoad("BLZDIAL.VOC", &DigitalFx[BLZDIAL_VOC], 1);

        soundLoad("BLZLOS.VOC",  &DigitalFx[BLZLOS_VOC],  1);
        soundLoad("BLZWIN.VOC",  &DigitalFx[BLZWIN_VOC],  1);

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
    soundUnload(&DigitalFx[BLZCLK_VOC]);
    soundUnload(&DigitalFx[BLZEXP1_VOC]);
    soundUnload(&DigitalFx[BLZEXP2_VOC]);
    soundUnload(&DigitalFx[BLZLAS_VOC]);
    soundUnload(&DigitalFx[BLZNRG_VOC]);
    soundUnload(&DigitalFx[BLZSHLD_VOC]);
    soundUnload(&DigitalFx[BLZTAC_VOC]);
    soundUnload(&DigitalFx[BLZSCN_VOC]);
    soundUnload(&DigitalFx[BLZMISS_VOC]);

    soundUnload(&DigitalFx[BLZENTR_VOC]);
    soundUnload(&DigitalFx[BLZABRT_VOC]);
    soundUnload(&DigitalFx[BLZSEL_VOC]);
    soundUnload(&DigitalFx[BLZKEY_VOC]);
    soundUnload(&DigitalFx[BLZDIAL_VOC]);

    soundUnload(&DigitalFx[BLZLOS_VOC]);
    soundUnload(&DigitalFx[BLZWIN_VOC]);
}

int digitalFxPlay(int effect, int priority) {
    // this function is used to play a digital effect using a pre-emptive priority
    // scheme. The algorithm works like this: if a sound is playing then its
    // priority is compared to the sound that is being requested to be played
    // if the new sound has higher priority (a smaller number) then the currently
    // playing sound is pre-empted for the new sound and the global FX priority
    // is set to the new sound. If there is no sound playing then the new sound
    // is simple played and the global priority is set

    // is the digital fx system on-line?
    if (!DigitalEnabled) {
        return 0;
    }

    // is there a sound playing?
    if (!soundStatus() || priority <= DigitalFxPriority) {
        // start new sound
        soundStop();
        soundPlay(&DigitalFx[effect]);

        // set the priority
        DigitalFxPriority = priority;

        return 1;
    } else {
        // the curren sound is of higher priority
        return 0;
    }
}

void parseCommands(int argc, char** argv) {
    // this function is used to parse the command line parameters that are to be
    // switched to enable different modes of operation
    int index;

    for (index = 1; index < argc; index++) {
        // get the first character from the string
        switch (argv[index][0]) {
            case 's':   // enable sound effects
            case 'S': {
                DigitalEnabled = 1;
            } break;

            case 'm':   // enable music
            case 'M': {
                MusicEnabled = 1;
            } break;

            // more commands would go here...
            default:
                break;
        }
    }
}

int getModemString(char* buffer) {
    // this function opens up the modem initialization file named
    // blaze.mod, if the file exists then
    FILE* fp;

    // try and open the file
    if ((fp = fopen("blaze.mod", "r")) == NULL) {
        strcpy(buffer, "");
        return 0;
    }

    // else load in modem initialization string
    fscanf(fp, "%63s", buffer);

    // close the file
    fclose(fp);

    return 1;
}

#ifdef BLAZERX
// ---------------------------------------------------------------------------
// Serial-link compatibility shim
//
// StarBlazer's modem code (engine/black9) is a reliable, ordered byte stream.
// We leave every serial call site in the game verbatim — so the logic stays
// byte-for-byte identical to the book — and redirect those calls, at compile
// time, onto the IPX transport in engine/ipx. The game's lockstep protocol
// (write a byte, then a blocking read, every frame) maps to a "flush-on-read"
// scheme: each read first ships our pending writes as one datagram, then
// blocks (pumping netPoll) until the peer's datagram arrives.
//
// Roles: broadcast discovery decides Master/Slave (host = MASTER, joiner =
// SLAVE); GAME_LINKING below applies the result via netRole().
// ---------------------------------------------------------------------------

#define BLAZER_NET_PORT   7777      // IPX socket for StarBlazer LAN play
#define LINK_TX_MAX       64
#define LINK_RX_MAX       256
#define LINK_HDR          8         // datagram header: 32-bit seq + 32-bit ack

static int           NetReady   = 0;   // netInit() succeeded this session
static unsigned char LinkTx[LINK_TX_MAX];
static int           LinkTxLen  = 0;
static unsigned char LinkRx[LINK_RX_MAX];
static int           LinkRxHead = 0;
static int           LinkRxTail = 0;

// Reliable, in-order delivery over IPX. StarBlazer was written for a modem — a
// lossless, ordered byte stream — and keeps the two ships in sync by exchanging
// only key-presses each frame and simulating both ships from them. A single
// lost/duplicated/reordered datagram would desync that lockstep forever, so we
// layer a tiny stop-and-wait protocol on top of the datagrams: each carries a
// 32-bit sequence and a 32-bit ack (count of in-order datagrams received from
// the peer). The receiver accepts only the next expected sequence; the sender
// retransmits until the peer's ack confirms delivery and will not advance to the
// next datagram until the current one is acknowledged. A dropped packet then
// self-heals instead of diverging the simulation.
static unsigned long TxSeq      = 0;   // sequence of the next datagram we send
static unsigned long RxExpect   = 0;   // next datagram sequence expected from peer
static unsigned long LastTxSeq  = 0;   // sequence of LastTx
static unsigned char LastTx[LINK_HDR + LINK_TX_MAX];   // last datagram (for resend)
static int           LastTxLen  = 0;
static int           TxAcked    = 1;   // has the peer acknowledged LastTx?

static void putU32(unsigned char* p, unsigned long v) {
    p[0] = (unsigned char)(v);        p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);  p[3] = (unsigned char)(v >> 24);
}
static unsigned long getU32(const unsigned char* p) {
    return (unsigned long)p[0]        | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static void linkRxPush(const unsigned char* p, int n) {
    int i;
    for (i = 0; i < n; i++) {
        int nxt = (LinkRxHead + 1) % LINK_RX_MAX;
        if (nxt == LinkRxTail) {
            break;                     // ring full, drop the rest
        }
        LinkRx[LinkRxHead] = p[i];
        LinkRxHead = nxt;
    }
}

static int linkRxPop(void) {
    int b;
    if (LinkRxTail == LinkRxHead) {
        return -1;
    }
    b = LinkRx[LinkRxTail];
    LinkRxTail = (LinkRxTail + 1) % LINK_RX_MAX;
    return b;
}

// (Re)send our current datagram, stamping the latest ack (= datagrams received).
static void linkSendCur(void) {
    if (LastTxLen <= 0) {
        return;
    }
    putU32(LastTx + 4, RxExpect);
    netSendToPeer(LastTx, (unsigned int)LastTxLen);
}

// Drain inbound datagrams: track the peer's ack, accept in-order payload only.
static void linkPumpRx(void) {
    unsigned char tmp[NET_MAX_PACKET_BYTES];
    int n;
    netPoll();
    while ((n = netRecv(tmp, sizeof(tmp), 0)) > 0) {
        unsigned long seq, ack;
        if (n < LINK_HDR) {
            continue;                  // malformed (no header)
        }
        seq = getU32(tmp);
        ack = getU32(tmp + 4);
        if (LastTxLen > 0 && ack > LastTxSeq) {
            TxAcked = 1;               // peer has received our LastTx
        }
        if (seq == RxExpect) {
            linkRxPush(tmp + LINK_HDR, n - LINK_HDR);
            RxExpect++;
        }
        // else: duplicate / old / out-of-order -> ignore
    }
}

static void netLinkWrite(int ch) {
    if (LinkTxLen < LINK_TX_MAX) {
        LinkTx[LinkTxLen++] = (unsigned char)ch;
    }
}

// Block (resending) until the peer acknowledges our last datagram, so we never
// advance the sequence with an undelivered packet still outstanding.
static void linkWaitAck(void) {
    unsigned long start = timerQuery();
    unsigned long lastSend = start;
    while (!TxAcked && (timerQuery() - start) < 18UL * 5UL) {
        linkPumpRx();
        if (TxAcked) {
            return;
        }
        if ((timerQuery() - lastSend) >= 2) {
            linkSendCur();
            lastSend = timerQuery();
        }
    }
}

// Blocking, reliable read of one link byte (the modem-equivalent serialReadWait):
// flush our pending write as a sequenced datagram, then pump the network until
// the next in-order byte arrives, retransmitting so loss self-heals. Times out
// (returns 0) after ~5 s so a vanished peer degrades rather than hanging.
static int netLinkReadWait(void) {
    unsigned long start, lastSend;
    int           b;

    if (LinkTxLen > 0) {
        linkWaitAck();                 // ensure the previous datagram landed first
        LastTxSeq = TxSeq;
        putU32(LastTx, TxSeq);
        memcpy(LastTx + LINK_HDR, LinkTx, (unsigned int)LinkTxLen);
        LastTxLen = LINK_HDR + LinkTxLen;
        LinkTxLen = 0;
        TxSeq++;
        TxAcked = 0;
        linkSendCur();
    }

    b = linkRxPop();
    if (b >= 0) {
        return b;
    }

    start = lastSend = timerQuery();
    while ((timerQuery() - start) < 18UL * 5UL) {   // 18.2 ticks/s
        linkPumpRx();
        b = linkRxPop();
        if (b >= 0) {
            return b;
        }
        if ((timerQuery() - lastSend) >= 2) {       // ~9 retransmits/s
            linkSendCur();
            lastSend = timerQuery();
        }
    }
    return 0;
}

// Maps to serialFlush(): reset the link, including the sequence state. Called at
// connect time, so both ends start their sequence space at 0 and stay aligned.
static void netLinkFlush(void) {
    LinkRxHead = LinkRxTail = 0;
    LinkTxLen  = 0;
    TxSeq      = 0;
    RxExpect   = 0;
    LastTxSeq  = 0;
    LastTxLen  = 0;
    TxAcked    = 1;
}

// Zero-config IPX pairing by broadcast discovery. MAKE CONNECTION -> joiner
// (the "phone number" the menu prompts for is ignored); WAIT FOR CONNECTION ->
// host. Both return MODEM_CONNECT on success so the menu advances to
// GAME_LINKING, or 0 (-> "COMM PROBLEM") on failure/timeout. Roles: host =
// MASTER, joiner = SLAVE (GAME_LINKING reads netRole()).

// Maps to makeConnection(number): join a hosted game. Over IPX this is zero-
// config broadcast discovery, so the entered "number" is ignored — just beacon
// as a joiner and pair with whichever peer is hosting.
static int netLinkClient(const char* number) {
    unsigned long start;

    if (!NetReady) {
        return 0;
    }
    netLinkFlush();
    (void)number;                       // ignored under IPX (discovery is broadcast)
    netJoin();                          // broadcast as a joiner; pair with the host
    start = timerQuery();
    while (netDiscoveryStatus() != NET_DISC_PAIRED &&
           (timerQuery() - start) < 18UL * 30UL) {     // up to ~30 s
        netPoll();
    }
    return (netDiscoveryStatus() == NET_DISC_PAIRED) ? MODEM_CONNECT : 0;
}

// Maps to waitForConnection(): host and wait for a client to connect.
static int netLinkHost(void) {
    unsigned long start;

    if (!NetReady) {
        return 0;
    }
    netLinkFlush();
    netHost();
    start = timerQuery();
    while (netDiscoveryStatus() != NET_DISC_PAIRED &&
           (timerQuery() - start) < 18UL * 90UL) {     // up to ~90 s (waiting on a human)
        netPoll();
    }
    return (netDiscoveryStatus() == NET_DISC_PAIRED) ? MODEM_CONNECT : 0;
}

// Compile-time redirection of the serial/modem link onto the IPX transport.
#define modemControl(x)       ((void)0)
#define serialOpen(a,b,c)     ((void)0)
#define initializeModem(s)    ((void)0)
#define hangUp()              ((void)0)
#define serialClose()         ((void)0)
#define serialFlush()         netLinkFlush()
#define makeConnection(n)     netLinkClient(n)
#define waitForConnection()   netLinkHost()
#define serialWrite(ch)       netLinkWrite(ch)
#define serialReadWait()      netLinkReadWait()
#endif

void main(int argc, char** argv) {
    // the main controls of the player and remote logic, normally we would
    // probably move most of the code into functions, but for instructional purposes
    // this is easier to follow, believe me there are already enough function calls
    // to make your head spin!
    int index,
        sel,                // used for input
        pxWindow,           // starting upper left hand corner of players view port
        pyWindow,
        result,             // result from comm system
        response,           // response from comm system
        playersKeyState,    // state of players input
        remotesKeyState,    // state of remotes input

        sentRight = 0,      // diagnostic counters to track number of
        sentLeft  = 0,      // input instructions both sent and received
        sentUp    = 0,
        recRight  = 0,
        recLeft   = 0,
        recUp     = 0;

    unsigned char seed;

    char buffer[64],            // general buffer
         modemIniString[64],    // used for extra modem initialization string
         number[32],            // used to print strings
         ch;                    // used for keyboard input

    // the game begins by loading in all the imagery, sounds and so forth.

    // parse the command line and set up configuration
    parseCommands(argc, argv);

    // get the modem initialization string
    getModemString(modemIniString);
    printf("\nExtra modem initialization string = %s", modemIniString);

#ifdef BLAZERX
    // bring up the LAN transport before we leave text mode, so any diagnostics
    // are visible
    printf("\nStarBlazer LAN: initializing network...\n");
    NetReady = netInit(BLAZER_NET_PORT);
    if (NetReady) {
        printf("IPX network ready (DOSBox: IPXNET STARTSERVER / CONNECT).\n");
    } else {
        printf("IPX unavailable - LAN play disabled (solo only).\n");
    }
    timeDelay(20);

#endif
    timeDelay(25);

#ifdef BLAZERX
    // set the graphics mode to SVGA 1024x768, 32-bit true colour
    setGraphicsModeVesa(1024, 768, 32);
#else
#ifdef VBE_SUPPORT
    // set the graphics mode to SVGA 640x480x256
    setGraphicsModeVesa(640, 480, 8);
#else
    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);
#endif
#endif

    // start up music system
    if (MusicEnabled) {
        musicInit();
        musicPlay(&Song, 0);
    }

    // start up digital FX system
    if (DigitalEnabled) {
        digitalFxInit();
    }

    // put up Waite header
    introWaite();

#ifdef VBE_SUPPORT
    // apply the game's palette
    pcxInit(&ImagePcx);
    pcxLoad("blazeint.pcx", &ImagePcx, 1);
    pcxDelete(&ImagePcx);
    fillScreen(0);

#endif
    // seed the random number generator with time
    srand(timerQuery());

    // initialize font engine
    fontEngine1(0, 0, 0, 0, NULL, NULL);
    techPrint(START_MESS_X, START_MESS_Y, "STARBLAZER 1.0 STARTING UP...", VideoBuffer);

    timeDelay(5);

    // create the double buffer
#ifdef BLAZERX
    createDoubleBuffer(768);
    techPrint(START_MESS_X, START_MESS_Y + 51, "DOUBLE BUFFER CREATED", VideoBuffer);
    techPrint(START_MESS_X, START_MESS_Y + 77, "LANGUAGE TRANSLATION ENGAGED", VideoBuffer);

    // install the keyboard driver
    keyboardInstallDriver();

    techPrint(START_MESS_X, START_MESS_Y + 102, "NEURAL INTERFACE ACTIVATED", VideoBuffer);

    // load in all gadget icons
    loadIcons();
    techPrint(START_MESS_X, START_MESS_Y + 128, "VISUAL ICONS LOADED", VideoBuffer);

    // load in the ships
    loadShips();
    techPrint(START_MESS_X, START_MESS_Y + 154, "SHIPS LOADED", VideoBuffer);

    // assign ships to player and remote
    copyFrames(&PlayersShip, &GryfonL);
    copyFrames(&RemotesShip, &RaptorR);

    // start the asteroids up
    initAsteroids(16, 6, 4);
    techPrint(START_MESS_X, START_MESS_Y + 179, "ASTEROID TRAJECTORIES COMPUTED", VideoBuffer);

    // start the stars
    initStars();
    techPrint(START_MESS_X, START_MESS_Y + 205, "STARFIELD GENERATED", VideoBuffer);

    // start the missiles
    initMissiles();
    techPrint(START_MESS_X, START_MESS_Y + 230, "WEAPONS DAEMONS ONLINE", VideoBuffer);

    // start the explosions
    initExplosions();
    loadExplosions();
    initNovas();
    techPrint(START_MESS_X, START_MESS_Y + 256, "EXPLOSION ANIMATION SYSTEM LOADED", VideoBuffer);

    techPrint(START_MESS_X, START_MESS_Y + 282, "FONT ENGINE ENGAGED", VideoBuffer);

    // load the wormhole imagery
    loadWormhole();
    initWormhole();
    techPrint(START_MESS_X, START_MESS_Y + 307, "WORMHOLE CREATED", VideoBuffer);

    // load the aliens
    loadAlien();
    initAlien();
    techPrint(START_MESS_X, START_MESS_Y + 333, "ENEMY AI GENERATED", VideoBuffer);

    // load the fuel cells
    loadFuelCells();
    initFuelCells();
    techPrint(START_MESS_X, START_MESS_Y + 358, "FUEL CELLS PLACED", VideoBuffer);

    // load the heads up display
    loadHeads();
    initHeads();
    techPrint(START_MESS_X, START_MESS_Y + 384, "HEADS UP DISPLAY ACTIVE", VideoBuffer);

    // blink ok
    techPrint(START_MESS_X, START_MESS_Y + 435, "SYSTEM BIOS O.K.", VideoBuffer);
#else
#ifdef VBE_SUPPORT
    createDoubleBuffer(480);
    techPrint(START_MESS_X, START_MESS_Y + 32, "DOUBLE BUFFER CREATED", VideoBuffer);
    techPrint(START_MESS_X, START_MESS_Y + 48, "LANGUAGE TRANSLATION ENGAGED", VideoBuffer);

    // install the keyboard driver
    keyboardInstallDriver();

    techPrint(START_MESS_X, START_MESS_Y + 64, "NEURAL INTERFACE ACTIVATED", VideoBuffer);

    // load in all gadget icons
    loadIcons();
    techPrint(START_MESS_X, START_MESS_Y + 80, "VISUAL ICONS LOADED", VideoBuffer);

    // load in the ships
    loadShips();
    techPrint(START_MESS_X, START_MESS_Y + 96, "SHIPS LOADED", VideoBuffer);

    // assign ships to player and remote
    copyFrames(&PlayersShip, &GryfonL);
    copyFrames(&RemotesShip, &RaptorR);

    // start the asteroids up
    initAsteroids(16, 6, 4);
    techPrint(START_MESS_X, START_MESS_Y + 112, "ASTEROID TRAJECTORIES COMPUTED", VideoBuffer);

    // start the stars
    initStars();
    techPrint(START_MESS_X, START_MESS_Y + 128, "STARFIELD GENERATED", VideoBuffer);

    // start the missiles
    initMissiles();
    techPrint(START_MESS_X, START_MESS_Y + 144, "WEAPONS DAEMONS ONLINE", VideoBuffer);

    // start the explosions
    initExplosions();
    loadExplosions();
    initNovas();
    techPrint(START_MESS_X, START_MESS_Y + 160, "EXPLOSION ANIMATION SYSTEM LOADED", VideoBuffer);

    techPrint(START_MESS_X, START_MESS_Y + 176, "FONT ENGINE ENGAGED", VideoBuffer);

    // load the wormhole imagery
    loadWormhole();
    initWormhole();
    techPrint(START_MESS_X, START_MESS_Y + 192, "WORMHOLE CREATED", VideoBuffer);

    // load the aliens
    loadAlien();
    initAlien();
    techPrint(START_MESS_X, START_MESS_Y + 208, "ENEMY AI GENERATED", VideoBuffer);

    // load the fuel cells
    loadFuelCells();
    initFuelCells();
    techPrint(START_MESS_X, START_MESS_Y + 224, "FUEL CELLS PLACED", VideoBuffer);

    // load the heads up display
    loadHeads();
    initHeads();
    techPrint(START_MESS_X, START_MESS_Y + 240, "HEADS UP DISPLAY ACTIVE", VideoBuffer);

    // blink ok
    techPrint(START_MESS_X, START_MESS_Y + 272, "SYSTEM BIOS O.K.", VideoBuffer);
#else
    createDoubleBuffer(200);
    techPrint(START_MESS_X, START_MESS_Y + 16, "DOUBLE BUFFER CREATED", VideoBuffer);
    techPrint(START_MESS_X, START_MESS_Y + 24, "LANGUAGE TRANSLATION ENGAGED", VideoBuffer);

    // install the keyboard driver
    keyboardInstallDriver();

    techPrint(START_MESS_X, START_MESS_Y + 32, "NEURAL INTERFACE ACTIVATED", VideoBuffer);

    // load in all gadget icons
    loadIcons();
    techPrint(START_MESS_X, START_MESS_Y + 40, "VISUAL ICONS LOADED", VideoBuffer);

    // load in the ships
    loadShips();
    techPrint(START_MESS_X, START_MESS_Y + 48, "SHIPS LOADED", VideoBuffer);

    // assign ships to player and remote
    copyFrames(&PlayersShip, &GryfonL);
    copyFrames(&RemotesShip, &RaptorR);

    // start the asteroids up
    initAsteroids(16, 6, 4);
    techPrint(START_MESS_X, START_MESS_Y + 56, "ASTEROID TRAJECTORIES COMPUTED", VideoBuffer);

    // start the stars
    initStars();
    techPrint(START_MESS_X, START_MESS_Y + 64, "STARFIELD GENERATED", VideoBuffer);

    // start the missiles
    initMissiles();
    techPrint(START_MESS_X, START_MESS_Y + 72, "WEAPONS DAEMONS ONLINE", VideoBuffer);

    // start the explosions
    initExplosions();
    loadExplosions();
    initNovas();
    techPrint(START_MESS_X, START_MESS_Y + 80, "EXPLOSION ANIMATION SYSTEM LOADED", VideoBuffer);

    // initialize font engine
    fontEngine1(0, 0, 0, 0, NULL, NULL);
    techPrint(START_MESS_X, START_MESS_Y + 88, "FONT ENGINE ENGAGED", VideoBuffer);

    // load the wormhole imagery
    loadWormhole();
    initWormhole();
    techPrint(START_MESS_X, START_MESS_Y + 96, "WORMHOLE CREATED", VideoBuffer);

    // load the aliens
    loadAlien();
    initAlien();
    techPrint(START_MESS_X, START_MESS_Y + 104, "ENEMY AI GENERATED", VideoBuffer);

    // load the fuel cells
    loadFuelCells();
    initFuelCells();
    techPrint(START_MESS_X, START_MESS_Y + 112, "FUEL CELLS PLACED", VideoBuffer);

    // load the heads up display
    loadHeads();
    initHeads();
    techPrint(START_MESS_X, START_MESS_Y + 120, "HEADS UP DISPLAY ACTIVE", VideoBuffer);

    // blink ok
    techPrint(START_MESS_X, START_MESS_Y + 136, "SYSTEM BIOS O.K.", VideoBuffer);
#endif
#endif

    digitalFxPlay(BLZBIOS_VOC, 1);

    for (index = 0; index < 3; index++) {
        // draw the message and the erase the message
#ifdef BLAZERX
        fontEngine1(START_MESS_X, START_MESS_Y + 435, 0, 0, "SYSTEM BIOS O.K.", VideoBuffer);
#else
#ifdef VBE_SUPPORT
        fontEngine1(START_MESS_X, START_MESS_Y + 272, 0, 0, "SYSTEM BIOS O.K.", VideoBuffer);
#else
        fontEngine1(START_MESS_X, START_MESS_Y + 136, 0, 0, "SYSTEM BIOS O.K.", VideoBuffer);
#endif
#endif
        timeDelay(8);

#ifdef BLAZERX
        fontEngine1(START_MESS_X, START_MESS_Y + 435, 0, 0, "                ", VideoBuffer);
#else
#ifdef VBE_SUPPORT
        fontEngine1(START_MESS_X, START_MESS_Y + 272, 0, 0, "                ", VideoBuffer);
#else
        fontEngine1(START_MESS_X, START_MESS_Y + 136, 0, 0, "                ", VideoBuffer);
#endif
#endif
        timeDelay(8);
    }

    // get rid off this sound to save a little memory (we are running low!)
    soundUnload(&DigitalFx[BLZBIOS_VOC]);

    // do intro piece
    introTitle();
#ifndef BLAZERX

    // save the system palette here because we are going to really thrash it!!!
    readPalette(0, 255, &GamePalette);
#endif

    // main event loop
    while (GameState != GAME_OVER) {
        // test the overall game state
        if (GameState == GAME_SETUP) {
            // if machines were linked then break connection
            if (Linked) {
                // close down serial connection
                hangUp();
                serialFlush();
                serialClose();

                // reset linked flag
                Linked = 0;
            }

#ifdef BLAZERX
            // back at the menu: clear single-player AI until a mode is chosen
            AiEnabled = 0;

#endif
            // user in the setup state
            introControls();
#ifndef BLAZERX

            // restore palette
            writePalette(&GamePalette);
#endif

            // enter setup event loop
            while (GameState == GAME_SETUP) {
                // this event loop is for the setup phase
                if (KeysActive > 0) {
                    // what is user trying to do
                    if (KeyboardState[MAKE_UP]) {
                        // erase the button and move it up
                        spriteErase(&Button1, VideoBuffer);

#ifdef BLAZERX
                        Button1.y -= 46;

                        // test if we need to wrap around bottom
                        if (--Button1.counter1 < 0) {
                            Button1.counter1 = 6;
                            Button1.y = 242 + 6 * 46;
                        }
#else
#ifdef VBE_SUPPORT
                        Button1.y -= 29;

                        // test if we need to wrap around bottom
                        if (--Button1.counter1 < 0) {
                            Button1.counter1 = 6;
                            Button1.y = 151 + 6 * 29;
                        }
#else
                        Button1.y -= 12;

                        // test if we need to wrap around bottom
                        if (--Button1.counter1 < 0) {
                            Button1.counter1 = 6;
                            Button1.y = 63 + 6 * 12;
                        }
#endif
#endif

                        // scan and draw button
                        spriteUnder(&Button1, VideoBuffer);
                        spriteDraw(&Button1, VideoBuffer, 1);

                        digitalFxPlay(BLZKEY_VOC, 2);

                        timeDelay(1);
                    } else if (KeyboardState[MAKE_DOWN]) {
                        // erase the button and move it down
                        spriteErase(&Button1, VideoBuffer);

#ifdef BLAZERX
                        Button1.y += 46;

                        // test if we need to wrap around top
                        if (++Button1.counter1 > 6) {
                            Button1.counter1 = 0;
                            Button1.y = 242;
                        }
#else
#ifdef VBE_SUPPORT
                        Button1.y += 29;

                        // test if we need to wrap around top
                        if (++Button1.counter1 > 6) {
                            Button1.counter1 = 0;
                            Button1.y = 151;
                        }
#else
                        Button1.y += 12;

                        // test if we need to wrap around top
                        if (++Button1.counter1 > 6) {
                            Button1.counter1 = 0;
                            Button1.y = 63;
                        }
#endif
#endif

                        // scan and draw button
                        spriteUnder(&Button1, VideoBuffer);
                        spriteDraw(&Button1, VideoBuffer, 1);

                        digitalFxPlay(BLZKEY_VOC, 2);

                        timeDelay(1);
                    } else if (KeyboardState[MAKE_ENTER]) {
                        // illuminate button for a second

                        // draw button down
                        Button1.currFrame = 1;
                        spriteDraw(&Button1, VideoBuffer, 1);

                        digitalFxPlay(BLZKEY_VOC, 2);

                        timeDelay(5);

                        // now draw button up
                        Button1.currFrame = 0;
                        spriteDraw(&Button1, VideoBuffer, 1);

                        // test which item is being selected
                        switch (Button1.counter1) {
                            case SETUP_PLAY_SOLO: {
                                clearDisplay(0);

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "ENTERING ARENA", VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "ENTERING ARENA", VideoBuffer);
#endif

#ifdef BLAZERX
                                // single-player: no network link, the remote
                                // ship is flown by the AI. Roles are fixed so
                                // the player and AI spawn at opposite corners.
                                Linked    = 0;
                                AiEnabled = 1;
                                Master    = 1;
                                Slave     = 0;

                                // give the AI the opposite ship type for variety
                                RemotesShipType = (PlayersShipType == GRYFON_SHIP)
                                    ? RAPTOR_SHIP : GRYFON_SHIP;
#else
                                // single-player: no network link
                                Linked = 0;
                                Master = 1;
                                Slave  = 0;
#endif

                                // move to running state
                                GameState = GAME_RUNNING;

                                timeDelay(5);
                            } break;

                            case SETUP_MAKE_CONNECTION: {
                                clearDisplay(0);

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "ENTER NUMBER", VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "ENTER NUMBER", VideoBuffer);
#endif

                                // remove the keyboard handler
                                // makes user input easier for strings
                                keyboardRemoveDriver();

                                result = getLine(number);

                                // replace our handler
                                keyboardInstallDriver();

                                // get the number
                                if (!result) {
                                    clearDisplay(0);

#ifdef BLAZERX
                                    fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "ABORTED...", VideoBuffer);
#else
                                    fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "ABORTED...", VideoBuffer);
#endif

                                    digitalFxPlay(BLZABRT_VOC, 1);

                                    timeDelay(5);

                                    clearDisplay(0);

                                    break;
                                }

                                // make a connection

                                // open comm port
                                modemControl(MODEM_DTR_ON);

                                serialOpen(
                                    CommPortToAddress[CommPort],
                                    SERIAL_BAUD_2400,
                                    SERIAL_PARITY_NONE | SERIAL_BITS_8 | SERIAL_STOP_1);

                                serialFlush();

                                // initialize modem with standard initialization
                                // plus any added init string in blaze.mod
                                initializeModem(modemIniString);

                                // let user know what's going on
                                clearDisplay(0);

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "DIALING:", VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "DIALING:", VideoBuffer);
#endif

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 6, DISPLAY_Y + 6 + 26, 0, 0, number, VideoBuffer);
#else
#ifdef VBE_SUPPORT
                                fontEngine1(DISPLAY_X + 4, DISPLAY_Y + 4 + 16, 0, 0, number, VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2 + 8, 0, 0, number, VideoBuffer);
#endif
#endif

                                digitalFxPlay(BLZDIAL_VOC, 1);

                                result = makeConnection(number);

                                if (result == MODEM_CONNECT_1200 ||
                                    result == MODEM_CONNECT_2400 ||
                                    result == MODEM_CONNECT ||
                                    result == MODEM_CARRIER_2400) {

                                    // set game to linked and local to master
                                    Linked = 1;
                                    Master = 1;
                                    Slave = 0;

                                    GameState = GAME_LINKING;

                                    clearDisplay(0);

#ifdef BLAZERX
                                    fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "CONNECTED!", VideoBuffer);
#else
                                    fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "CONNECTED!", VideoBuffer);
#endif

                                    timeDelay(5);

                                    clearDisplay(0);
                                } else if (result == MODEM_USER_ABORT) {
                                    // close down serial connection
                                    hangUp();
                                    serialFlush();
                                    serialClose();

                                    // print that selection was aborted
                                    clearDisplay(0);

#ifdef BLAZERX
                                    fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "ABORTED...", VideoBuffer);
#else
                                    fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "ABORTED...", VideoBuffer);
#endif

                                    digitalFxPlay(BLZABRT_VOC, 1);

                                    timeDelay(5);

                                    clearDisplay(0);
                                } else {
                                    // close down serial connection
                                    hangUp();
                                    serialFlush();
                                    serialClose();

                                    // print that selection was aborted
                                    clearDisplay(0);

#ifdef BLAZERX
                                    fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "COMM PROBLEM", VideoBuffer);
#else
                                    fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "COMM PROBLEM", VideoBuffer);
#endif

                                    timeDelay(5);

                                    clearDisplay(0);
                                }
                            } break;

                            case SETUP_WAIT_FOR_CONNECTION: {
                                clearDisplay(0);

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "ANSWER MODE", VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "ANSWER MODE", VideoBuffer);
#endif

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 6, DISPLAY_Y + 6 + 26, 0, 0, "ENABLED...", VideoBuffer);
#else
#ifdef VBE_SUPPORT
                                fontEngine1(DISPLAY_X + 4, DISPLAY_Y + 4 + 16, 0, 0, "ENABLED...", VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2 + 8, 0, 0, "ENABLED...", VideoBuffer);
#endif
#endif

                                // open comm port
                                modemControl(MODEM_DTR_ON);

                                serialOpen(
                                    CommPortToAddress[CommPort],
                                    SERIAL_BAUD_2400,
                                    SERIAL_PARITY_NONE | SERIAL_BITS_8 | SERIAL_STOP_1);

                                serialFlush();

                                // wait for a call

                                // initialize modem with standard initialization
                                // plus any added init string in blaze.mod
                                initializeModem(modemIniString);

                                result = waitForConnection();

                                // is the connection message 2400?
                                if (result == MODEM_CONNECT_1200 ||
                                    result == MODEM_CONNECT_2400 ||
                                    result == MODEM_CONNECT ||
                                    result == MODEM_CARRIER_2400) {

                                    // set game state to linked and local to slave
                                    Linked = 1;
                                    Master = 0;
                                    Slave = 1;

                                    GameState = GAME_LINKING;

                                    clearDisplay(0);

#ifdef BLAZERX
                                    fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "CONNECTED!", VideoBuffer);
#else
                                    fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "CONNECTED!", VideoBuffer);
#endif

                                    timeDelay(5);

                                    clearDisplay(0);
                                } else if (result == MODEM_USER_ABORT) {
                                    // user bailed so close down serial
                                    hangUp();
                                    serialFlush();
                                    serialClose();

                                    clearDisplay(0);

#ifdef BLAZERX
                                    fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "ABORTED...", VideoBuffer);
#else
                                    fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "ABORTED...", VideoBuffer);
#endif

                                    // play sound fx
                                    digitalFxPlay(BLZABRT_VOC, 1);

                                    timeDelay(5);

                                    clearDisplay(0);
                                } else {
                                    // close down serial connection
                                    hangUp();
                                    serialFlush();
                                    serialClose();

                                    // print that selection was aborted
                                    clearDisplay(0);

#ifdef BLAZERX
                                    fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "COMM PROBLEM", VideoBuffer);
#else
                                    fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "COMM PROBLEM", VideoBuffer);
#endif

                                    timeDelay(5);

                                    clearDisplay(0);
                                }
                            } break;

                            case SETUP_SELECT_SHIP: {
                                clearDisplay(0);

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "GRYFON  RAPTOR", VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "GRYFON  RAPTOR", VideoBuffer);
#endif

                                // draw ships
                                Displays.currFrame = DISPLAY_IMG_SHIPS;

                                spriteDraw(&Displays, VideoBuffer, 1);

                                // let user select one
                                sel = displaySelect(PlayersShipType);

                                // test if user changed ship
                                if (sel != -1) {
                                    PlayersShipType = sel;

                                    // set up sprite
                                    if (PlayersShipType == GRYFON_SHIP) {
                                        copyFrames(&PlayersShip, &GryfonL);
                                    } else {
                                        copyFrames(&PlayersShip, &RaptorL);
                                    }
                                }
                            } break;

                            case SETUP_SET_COMM_PORT: {
                                clearDisplay(0);

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "COMM 1  COMM 2", VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "COMM 1  COMM 2", VideoBuffer);
#endif

                                // draw comm ports
                                Displays.currFrame = DISPLAY_IMG_PORTS;

                                spriteDraw(&Displays, VideoBuffer, 1);

                                // let user select one
                                sel = displaySelect(CommPort);

                                // test if user changed selection
                                if (sel != -1) {
                                    // change comm port
                                    CommPort = sel;
                                }
                            } break;

                            case SETUP_BRIEFING: {
                                // show user instructions
                                digitalFxPlay(BLZMISS_VOC, 1);

                                introBriefing();

                                // redisplay intro controls
                                introControls();
                            } break;

                            case SETUP_EXIT: {
                                // set state to game over
                                GameState = GAME_OVER;

                                clearDisplay(0);

#ifdef BLAZERX
                                fontEngine1(DISPLAY_X + 3, DISPLAY_Y + 3, 0, 0, "EXITING SYSTEM", VideoBuffer);
#else
                                fontEngine1(DISPLAY_X + 2, DISPLAY_Y + 2, 0, 0, "EXITING SYSTEM", VideoBuffer);
#endif
                            } break;

                            default:
                                break;
                        }
                    }
                }

                // perform special effects to control panel
#ifdef BLAZERX
                panelFx(PANEL_FX_CONTROL);
#else
                panelFx();
#endif

                // slow things down a bit
                timeDelay(1);

                // check on music
                if (MusicEnabled) {
                    // test if piece is complete or has been stopped
                    if (musicStatus() == 2 || musicStatus() == 0) {
                        // advance to next sequence
                        if (++IntroSeqIndex == 11) {
                            IntroSeqIndex = 0;
                        }

                        musicPlay(&Song, IntroSequence[IntroSeqIndex]);
                    }
                }
            }
        } else if (GameState == GAME_LINKING) {
#ifdef BLAZERX
            // a real peer is being negotiated - make sure the AI is off
            AiEnabled = 0;
            // discovery (not the menu) decides who is master: the peer with the
            // higher random nonce wins, so both ends agree regardless of which
            // menu item each player chose
            Master = (netRole() == NET_ROLE_MASTER);
            Slave  = (netRole() == NET_ROLE_SLAVE);
#endif
            // wait a second to make sure other machine is linked
            timeDelay(DELAY_2_SECOND);

            // test if this machine is master or slave and negotiate connection
            if (Master) {
                // send master signal
                serialWrite('M');

                // send ship type
                serialWrite('0' + PlayersShipType);

                // this is a cruciel function call, both machines must use the same
                // random sequence, this is accomplished by the master sending a seed
                // to the remote to be used as the seed for the random number generator

                // send random seed
                seed = timerQuery();

                serialWrite(seed);

                // seed local random number generator
                srand(seed);

                // wait for acknowledge
                response = serialReadWait();

                if (response == 'S') {
                    // get remotes ship type
                    response = serialReadWait();

                    RemotesShipType = response - '0';

                    // move game state into running mode
                    GameState = GAME_RUNNING;
                }
            } else if (Slave) {
                // send master signal
                serialWrite('S');

                // send ship type
                serialWrite('0' + PlayersShipType);

                // wait for a master signal

                // wait for acknowledge
                response = serialReadWait();

                if (response == 'M') {
                    // get remotes ship type
                    response = serialReadWait();

                    RemotesShipType = response - '0';

                    // the next byte is the random number generator seed
                    seed = serialReadWait();

                    srand(seed);

                    // move game state into running mode
                    GameState = GAME_RUNNING;
                }
            }
        } else if (GameState == GAME_RUNNING) {
#ifndef BLAZERX
            // restore palette
            writePalette(&GamePalette);

#endif
            // turn shields off
            shieldControl(THE_PLAYER, 0);
            shieldControl(THE_REMOTE, 0);

            // reset system variables
            resetSystem();

            // select engine colors
            if (PlayersShipType == GRYFON_SHIP) {
                PlayersEngineColor = PrimaryWhite;
            } else {
                PlayersEngineColor = PrimaryGreen;
            }

            if (RemotesShipType == GRYFON_SHIP) {
                RemotesEngineColor = PrimaryWhite;
            } else {
                RemotesEngineColor = PrimaryGreen;
            }

            // set up remote ships ship type
            if (RemotesShipType == GRYFON_SHIP) {
                copyFrames(&RemotesShip, &GryfonR);
            } else {
                copyFrames(&RemotesShip, &RaptorR);
            }

            // restart everything
            initWormhole();
            initStars();
            initMissiles();
            initAsteroids(16, 6, 4);
            initExplosions();
            initNovas();
            initAlien();
            initHeads();
            initFuelCells();

            // start music
            if (MusicEnabled) {
                musicStop();

                GameSeqIndex = 0;

                musicPlay(&Song, GameSequence[GameSeqIndex]);
            }

            digitalFxPlay(BLZENTR_VOC, 1);

            // clear double buffer
            fillDoubleBuffer(0);

            // scan under all objects
            underWormhole();
            underAsteroids();
            underStars();
            underFuelCells();

            spriteUnder(&PlayersShip, DoubleBuffer);

#ifdef VBE_SUPPORT
            pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
            pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;

            RemotesShip.x = (RemotesX - pxWindow) * CAM_ZOOM;
            RemotesShip.y = (RemotesY - pyWindow) * CAM_ZOOM;
#else
            pxWindow = PlayersX - 160 + 11;
            pyWindow = PlayersY - 100 + 9;

            RemotesShip.x = RemotesX - pxWindow;
            RemotesShip.y = RemotesY - pyWindow;
#endif

            spriteUnderClip(&RemotesShip, DoubleBuffer);

            // enter into the main game loop
            while (GameState == GAME_RUNNING) {
                // compute starting time of this frame
                StartingTime = timerQuery();

                // reset all vars
                RefreshHeads = 0;

                // flag engines off
                PlayersEngine = 0;
                RemotesEngine = 0;

                // erase all objects
                eraseFuelCells();
                eraseAlien();
                eraseWormhole();
                eraseStars();
                eraseAsteroids();
                eraseMissiles();
                eraseExplosions();
                eraseNovas();

                // erase scanner data
                if (PlayersScanner == 1) {
                    eraseBlips();
                }

                spriteErase(&PlayersShip, DoubleBuffer);
                spriteEraseClip(&RemotesShip, DoubleBuffer);

                // move player
                PlayersLastX = PlayersX;
                PlayersLastY = PlayersY;

                // test if a key is depressed
                playersKeyState = 0;

                if (PlayersState == ALIVE) {
                    if (KeysActive > 0) {
                        // which key?
                        if (KeyboardState[MAKE_LEFT]) {
                            // rotate left
                            if (--PlayersShip.currFrame < 0) {
                                PlayersShip.currFrame = 15;
                            }

                            // add this action to key state
                            playersKeyState += REMOTE_LEFT;

                            #if DEBUG
                            sentLeft++;
                            #endif
                        } else if (KeyboardState[MAKE_RIGHT]) {
                            // rotate right
                            if (++PlayersShip.currFrame > 15) {
                                PlayersShip.currFrame = 0;
                            }

                            // add this action to key state
                            playersKeyState += REMOTE_RIGHT;

                            #if DEBUG
                            sentRight++;
                            #endif
                        }

                        if (KeyboardState[MAKE_UP] && PlayersEnergy > 0) {
                            // thrust forward
                            PlayersXv += MotionDx[PlayersShip.currFrame];
                            PlayersYv += MotionDy[PlayersShip.currFrame];

                            // bound maximum velocity
                            if (PlayersXv > 8) {
                                PlayersXv = 8;
                            } else if (PlayersXv < -8) {
                                PlayersXv = -8;
                            }

                            if (PlayersYv > 8) {
                                PlayersYv = 8;
                            } else if (PlayersYv < -8) {
                                PlayersYv = -8;
                            }

                            // flag engines on
                            PlayersEngine =1;

                            PlayersEnergy--;

                            // add this action to key state
                            playersKeyState += REMOTE_THRUST;

                            #if DEBUG
                            sentUp++;
                            #endif
                        }

                        if (KeyboardState[MAKE_SPACE] &&
                            !DebounceFire &&
                            PlayersActiveMissiles < 5 &&
                            PlayersEnergy > 0) {

                            // fire weapons
                            startMissile(
                                PlayersX + SHIP_WIDTH / 2,
                                PlayersY + SHIP_HEIGHT / 2,
                                PlayersXv + 2 * MotionDx[PlayersShip.currFrame],
                                PlayersYv + 2 * MotionDy[PlayersShip.currFrame],
#ifdef BLAZERX
                                MISSILE_COLOR,
#else
                                10,
#endif
                                PLAYER_MISSILE);

                            // update energy
                            PlayersEnergy -= 5;

                            // now there is one more active missile
                            PlayersActiveMissiles++;

                            // add this action to key state
                            playersKeyState += REMOTE_FIRE;

                            // set fire button debounce
                            DebounceFire = 1;
                        }

                        // instrumentation
                        if (KeyboardState[MAKE_ALT] &&
                            !DebounceShields &&
                            PlayersCloak == -1 &&
                            PlayersShieldStrength > 0 &&
                            PlayersShieldTime == 0 &&
                            PlayersShieldCooldown == 0) {

                            // turn the shields on - they now stay up for the full
                            // SHIELD_ON_TIME and can't be dropped early
                            shieldControl(THE_PLAYER, 1);

                            digitalFxPlay(BLZSHLD_VOC, 1);

                            // start timer
                            PlayersShieldTime = SHIELD_ON_TIME;

                            // add this action to key state
                            playersKeyState += REMOTE_SHIELDS;

                            // request debounce
                            DebounceShields = 1;
                        } else if (KeyboardState[MAKE_C] &&
                                !DebounceCloak &&
                                PlayersEnergy > 0) {

                            // toggle the cloaking device
                            PlayersCloak = -PlayersCloak;

                            digitalFxPlay(BLZCLK_VOC, 1);

                            // refresh heads up if it's on
                            RefreshHeads = 1;

                            // request debounce
                            DebounceCloak = 1;

                            // add this action to key state
                            playersKeyState += REMOTE_CLOAK;
                        } else if (KeyboardState[MAKE_H] && !DebounceHud) {
                            // toggle the display status
                            PlayersHeads =- PlayersHeads;

                            // test if it's on or off
                            if (PlayersHeads == 1) {
                                drawHeads();
                                digitalFxPlay(BLZTAC_VOC, 1);
                            } else {
                                eraseHeads();
                            }

                            // request debounce
                            DebounceHud = 1;
                        } else if (KeyboardState[MAKE_S] && !DebounceScan) {
                            // toggle the scanner
                            PlayersScanner =- PlayersScanner;

                            // test if it's on or off
                            if (PlayersScanner == 1) {
                                drawScanner();
                                digitalFxPlay(BLZSCN_VOC, 1);
                            } else {
                                eraseScanner();
                            }

                            // refresh heads up if it's on
                            RefreshHeads = 1;

                            // request debounce
                            DebounceScan = 1;
                        } else if (KeyboardState[MAKE_ESC]) {
                            GameState = GAME_SETUP;

                            // add this action to key state
                            playersKeyState += REMOTE_ESC;
                        }
                    }
                }

                // debounce section
                if (!KeyboardState[MAKE_C]) {
                    DebounceCloak = 0;
                }

                if (!KeyboardState[MAKE_H]) {
                    DebounceHud = 0;
                }

                if (!KeyboardState[MAKE_S]) {
                    DebounceScan = 0;
                }

                if (!KeyboardState[MAKE_SPACE]) {
                    DebounceFire = 0;
                }

                if (!KeyboardState[MAKE_ALT]) {
                    DebounceShields = 0;
                }

                // send local players data to remote machine
                if (Linked) {
                    serialWrite(playersKeyState);
                }

                // update energy loss due to normal operation
                if (PlayersCloak == 1) {
                    PlayersEnergy--;
                }

                if (PlayersEnergy > 0) {
                    PlayersEnergy--;

                    // enable refresh
                    RefreshHeads = 1;
                } else {
                    PlayersEnergy = 0;
                }

                // test if shields are on
                if (PlayersShields == 1) {
                    PlayersShieldStrength -= 8;

                    if (PlayersShieldStrength < 0) {
                        PlayersShieldStrength = 0;
                    }

                    // enable refresh
                    RefreshHeads = 1;
                }

                // test if hud needs refreshing
                if (PlayersHeads == 1 && RefreshHeads) {
                    drawHeads();

                    // reset refresh flag
                    RefreshHeads = 0;
                }

                // translate player and apply friction if engines aren't on
                PlayersX += PlayersXv;
                PlayersY += PlayersYv;

                // test for gravity
                if (!PlayersEngine) {
                    if (++PlayersGravity >= PlayersStability) {
                        // reset gravity count
                        PlayersGravity = 0;

                        // apply friction (in space!!!). hey it's just a game
                        if (PlayersXv > 0) {
                            PlayersXv--;
                        } else if (PlayersXv < 0) {
                            PlayersXv++;
                        }

                        if (PlayersYv > 0) {
                            PlayersYv--;
                        } else if (PlayersYv < 0) {
                            PlayersYv++;
                        }
                    }
                }

                // show engines flicker
                if (PlayersEngine) {
                    if (++PlayersFlameCount > PlayersFlameTime) {
                        // turn engines on
#ifdef BLAZERX
                        PlayersTintColors[1] = vgaColorToRGB32(PlayersEngineColor);
#else
                        writeColorReg(PLAYERS_ENGINE_REG, &PlayersEngineColor);
#endif

                        // reset counter
                        PlayersFlameCount = 0;
                    } else {
                        // turn engines off
#ifdef BLAZERX
                        PlayersTintColors[1] = vgaColorToRGB32(PrimaryBlack);
#else
                        writeColorReg(PLAYERS_ENGINE_REG, &PrimaryBlack);
#endif
                    }
                }

                // test if shields should turn off
                if (PlayersShieldTime > 0) {
                    // try and turn off shields
                    if (--PlayersShieldTime <= 0) {
                        shieldControl(THE_PLAYER, 0);

                        // begin the recharge period before shields can return
                        PlayersShieldCooldown = SHIELD_COOLDOWN_TIME;
                    } else {
                        // which shield colors?
                        if (PlayersShipType == GRYFON_SHIP) {
                            if ((PlayersShieldColor.blue += 8) >= 64) {
                                PlayersShieldColor.blue = 24;
                            }

#ifdef BLAZERX
                            PlayersTintColors[0] = vgaColorToRGB32(PlayersShieldColor);
#else
                            writeColorReg(PLAYERS_SHIELD_REG, &PlayersShieldColor);
#endif
                        } else {
                            // must be a raptor
                            if ((PlayersShieldColor.red += 8) >= 64) {
                                PlayersShieldColor.red = 24;
                            }

#ifdef BLAZERX
                            PlayersTintColors[0] = vgaColorToRGB32(PlayersShieldColor);
#else
                            writeColorReg(PLAYERS_SHIELD_REG, &PlayersShieldColor);
#endif
                        }
                    }
                }

                // tick the shield recharge while shields are down
                if (PlayersShieldTime == 0 && PlayersShieldCooldown > 0) {
                    PlayersShieldCooldown--;
                }

                // compute players delta
                PlayersDx = PlayersLastX - PlayersX;
                PlayersDy = PlayersLastY - PlayersY;

                // do player boundary detection
                // we are going to wrap universe around, but maybe placing
                // a barrier at edges would be better?
                if (PlayersX > UNIVERSE_WIDTH) {
                    PlayersX = PlayersX - UNIVERSE_WIDTH;
                } else if (PlayersX < 0) {
                    PlayersX = PlayersX + UNIVERSE_WIDTH;
                }

                if (PlayersY > UNIVERSE_HEIGHT) {
                    PlayersY = PlayersY - UNIVERSE_HEIGHT;
                } else if (PlayersY < 0) {
                    PlayersY = PlayersY + UNIVERSE_HEIGHT;
                }

                // process the remote ship if a peer is linked or the AI is flying it
#ifdef BLAZERX
                if (Linked || AiEnabled) {
#else
                if (Linked) {
#endif
                    // move remote
                    RemotesLastX = RemotesX;
                    RemotesLastY = RemotesY;

                    // reset remotes input
                    remotesKeyState = 0;

#ifdef BLAZERX
                    // get input: from the AI in single-player, else from the peer
                    if (AiEnabled) {
                        remotesKeyState = computeRemoteAi();
                    } else {
                        remotesKeyState = serialReadWait();
                    }
#else
                    // get input from the peer
                    remotesKeyState = serialReadWait();
#endif

                    // test if a key is depressed
                    if (RemotesState == ALIVE) {
                        if (remotesKeyState > 0) {
                            // which key?
                            if (remotesKeyState & REMOTE_LEFT) {
                                // rotate left
                                if (--RemotesShip.currFrame < 0) {
                                    RemotesShip.currFrame = 15;
                                }

                                #if DEBUG
                                recLeft++;
                                #endif
                            } else if (remotesKeyState & REMOTE_RIGHT) {
                                // rotate right
                                if (++RemotesShip.currFrame > 15) {
                                    RemotesShip.currFrame = 0;
                                }

                                #if DEBUG
                                recRight++;
                                #endif
                            }

                            if (remotesKeyState & REMOTE_THRUST && RemotesEnergy > 0) {
                                // thrust forward
                                RemotesXv += MotionDx[RemotesShip.currFrame];
                                RemotesYv += MotionDy[RemotesShip.currFrame];

                                // bound maximum velocity
                                if (RemotesXv > 8) {
                                    RemotesXv = 8;
                                } else if (RemotesXv < -8) {
                                    RemotesXv = -8;
                                }

                                if (RemotesYv > 8) {
                                    RemotesYv = 8;
                                } else if (RemotesYv < -8) {
                                    RemotesYv = -8;
                                }

                                // flag engines on
                                RemotesEngine = 1;
                                RemotesEnergy--;

                                #if DEBUG
                                recUp++;
                                #endif
                            }

                            if (remotesKeyState & REMOTE_FIRE &&
                                RemotesActiveMissiles < 5 &&
                                RemotesEnergy > 0) {

                                // fire weapons
                                startMissile(
                                    RemotesX + SHIP_WIDTH / 2,
                                    RemotesY + SHIP_HEIGHT / 2,
                                    RemotesXv + 2 * MotionDx[RemotesShip.currFrame],
                                    RemotesYv + 2 * MotionDy[RemotesShip.currFrame],
#ifdef BLAZERX
                                    MISSILE_COLOR,
#else
                                    10,
#endif
                                    REMOTE_MISSILE);

                                // decrease energy
                                RemotesEnergy -= 5;

                                // now there is one more active missile
                                RemotesActiveMissiles++;
                            }

                            // instrumentation
                            if (remotesKeyState & REMOTE_SHIELDS &&
                                RemotesCloak == -1 &&
                                RemotesShieldStrength > 0 &&
                                RemotesShieldTime == 0 &&
                                RemotesShieldCooldown == 0) {

                                // turn the shields on - they stay up for the full
                                // SHIELD_ON_TIME and can't be dropped early
                                shieldControl(THE_REMOTE, 1);

                                // start timer
                                RemotesShieldTime = SHIELD_ON_TIME;
                            } else if (remotesKeyState & REMOTE_CLOAK && RemotesEnergy > 0) {
                                // toggle the cloaking device
                                RemotesCloak = -RemotesCloak;
                            } else if (remotesKeyState & REMOTE_ESC) {
                                GameState = GAME_SETUP;
                            }
                        }

                        // update energy loss due to normal operation.
                        // BUG FIX: mirror the player's energy upkeep (~4939). The
                        // book's original line here was `else RemotesEnergy = 0`,
                        // which zeroed energy on every frame the remote wasn't
                        // cloaked -> the remote ship could never thrust/fire (both
                        // gated on RemotesEnergy > 0), so in two-player it coasted
                        // and drifted out of sync with the peer actually flying it.
                        if (RemotesCloak == 1) {
                            RemotesEnergy--;
                        }

                        if (RemotesEnergy > 0) {
                            RemotesEnergy--;
                        } else {
                            RemotesEnergy = 0;
                        }

                        // test if shields are on
                        if (RemotesShields == 1) {
                            RemotesShieldStrength -= 8;

                            if (RemotesShieldStrength < 0) {
                                RemotesShieldStrength = 0;
                            }
                        }
                    }

                    // translate remote and apply friction if engines aren't on
                    RemotesX += RemotesXv;
                    RemotesY += RemotesYv;

                    // test for gravity
                    if (!RemotesEngine) {
                        if (++RemotesGravity >= RemotesStability) {
                            // reset gravity count
                            RemotesGravity = 0;

                            // apply friction (in space!!!). hey it's just a game
                            if (RemotesXv > 0) {
                                RemotesXv--;
                            } else if (RemotesXv < 0) {
                                RemotesXv++;
                            }

                            if (RemotesYv > 0) {
                                RemotesYv--;
                            } else if (RemotesYv < 0) {
                                RemotesYv++;
                            }
                        }
                    }

                    // show engines flicker
                    if (RemotesEngine) {
                        if (++RemotesFlameCount > RemotesFlameTime) {
                            // turn engines on
#ifdef BLAZERX
                            RemotesTintColors[1] = vgaColorToRGB32(RemotesEngineColor);
#else
                            writeColorReg(REMOTES_ENGINE_REG, &RemotesEngineColor);
#endif

                            // reset counter
                            RemotesFlameCount = 0;
                        } else {
                            // turn engines off
#ifdef BLAZERX
                            RemotesTintColors[1] = vgaColorToRGB32(PrimaryBlack);
#else
                            writeColorReg(REMOTES_ENGINE_REG, &PrimaryBlack);
#endif
                        }
                    }

                    // test if shields should turn off
                    if (RemotesShieldTime > 0) {
                        // try and turn off shields
                        if (--RemotesShieldTime <= 0) {
                            shieldControl(THE_REMOTE, 0);

                            // begin the recharge period before shields can return
                            RemotesShieldCooldown = SHIELD_COOLDOWN_TIME;
                        } else {
                            // which shield colors?
                            if (RemotesShipType == GRYFON_SHIP) {
                                if ((RemotesShieldColor.blue += 8) >= 64) {
                                    RemotesShieldColor.blue = 24;
                                }

#ifdef BLAZERX
                                RemotesTintColors[0] = vgaColorToRGB32(RemotesShieldColor);
#else
                                writeColorReg(REMOTES_SHIELD_REG, &RemotesShieldColor);
#endif
                            } else {
                                // must be a raptor
                                if ((RemotesShieldColor.red += 8) >= 64) {
                                    RemotesShieldColor.red = 24;
                                }

#ifdef BLAZERX
                                RemotesTintColors[0] = vgaColorToRGB32(RemotesShieldColor);
#else
                                writeColorReg(REMOTES_SHIELD_REG, &RemotesShieldColor);
#endif
                            }
                        }
                    }

                    // tick the shield recharge while shields are down
                    if (RemotesShieldTime == 0 && RemotesShieldCooldown > 0) {
                        RemotesShieldCooldown--;
                    }

                    // compute remotes delta
                    RemotesDx = RemotesLastX - RemotesX;
                    RemotesDy = RemotesLastY - RemotesY;

                    // do remotes boundary detection
                    // we are going to wrap universe around, but maybe placing
                    // a barrier at edges would be better?
                    if (RemotesX > UNIVERSE_WIDTH) {
                        RemotesX = RemotesX - UNIVERSE_WIDTH;
                    } else if (RemotesX < 0) {
                        RemotesX = RemotesX + UNIVERSE_WIDTH;
                    }

                    if (RemotesY > UNIVERSE_HEIGHT) {
                        RemotesY = RemotesY - UNIVERSE_HEIGHT;
                    } else if (RemotesY < 0) {
                        RemotesY = RemotesY + UNIVERSE_HEIGHT;
                    }
                }

                // call AI logic for alien
                alienControl();

                // move all objects
                moveStars();
                moveAlien();
                moveAsteroids();
                moveMissiles();
                animateExplosions();
                moveNovas();
                animateWormhole();
                animateFuelCells();

                // perform death sequence logic for player
                if (PlayersState == DYING) {
                    // decrement death counter
                    if (--PlayersDeathCount <= 0) {
                        // reset player to starting position
                        resetPlayer();

                        // test if this is it...
                        if (--PlayersNumShips == 0) {
                            // return to setup
                            GameState = GAME_SETUP;
                            Winner = WINNER_REMOTE;
                        }

                        // refresh hud in any case
                        if (PlayersHeads == 1) {
                            drawHeads();
                        }
                    }
                }

                // perform death sequence logic for remote
#ifdef BLAZERX
                if ((Linked || AiEnabled) && RemotesState == DYING) {
#else
                if (Linked && RemotesState == DYING) {
#endif
                    // decrement death counter
                    if (--RemotesDeathCount <= 0) {
                        // reset remote to starting position
                        resetRemote();

                        // test if this is it...
                        if (--RemotesNumShips == 0) {
                            GameState = GAME_SETUP;
                            Winner = WINNER_PLAYER;
                        }
                    }
                }

                // scan under all objects
                underFuelCells();
                underStars();
                underAsteroids();

                underMissiles();
                underExplosions();
                underNovas();
                underWormhole();
                underAlien();

                // render scanner with new data
                if (PlayersScanner == 1) {
                    underBlips();
                }

                spriteUnder(&PlayersShip, DoubleBuffer);

                // translate remote to player
#ifdef VBE_SUPPORT
                pxWindow = PlayersX - VIEW_WIDTH / 2 + SHIP_WIDTH / 2;
                pyWindow = PlayersY - VIEW_HEIGHT / 2 + SHIP_HEIGHT / 2;

                RemotesShip.x = (RemotesX - pxWindow) * CAM_ZOOM;
                RemotesShip.y = (RemotesY - pyWindow) * CAM_ZOOM;
#else
                pxWindow = PlayersX - 160 + 11;
                pyWindow = PlayersY - 100 + 9;

                RemotesShip.x = RemotesX - pxWindow;
                RemotesShip.y = RemotesY - pyWindow;
#endif

                spriteUnderClip(&RemotesShip, DoubleBuffer);

                // draw all objects
                drawWormhole();
                drawFuelCells();
                drawStars();
                drawAsteroids();

                // display proper image of ship depending on state of engines and cloak

                // but first is there a ship?
                if (PlayersState == ALIVE) {
                    // test if ship is totally cloaked
                    if (PlayersCloak == -1) {
                        // ship isn't cloaked
                        if (PlayersEngine) {
                            // index into frames with thrust
                            PlayersShip.currFrame += 16;

                            // draw the ship with thrust showing
#ifdef BLAZERX
                            spriteDrawTinted(&PlayersShip, DoubleBuffer, 1,
                                PlayersTintColors, 2);
#else
                            spriteDraw(&PlayersShip, DoubleBuffer, 1);
#endif

                            // restoire the original frame
                            PlayersShip.currFrame -= 16;
                        } else {
                            // no engines
#ifdef BLAZERX
                            spriteDrawTinted(&PlayersShip, DoubleBuffer, 1,
                                PlayersTintColors, 2);
#else
                            spriteDraw(&PlayersShip, DoubleBuffer, 1);
#endif
                        }
                    } else {
                        // player is cloaked

                        // test if player is trying to engage shields
                        if (PlayersShields) {
                            // cloaking is exclusive with shields, so it ends the
                            // shield session - drop them and start the recharge
                            shieldControl(THE_PLAYER, 0);
                            PlayersShieldTime = 0;
                            PlayersShieldCooldown = SHIELD_COOLDOWN_TIME;
                        }
                    }
                }

                // display proper image of ship depending on state of engines and cloak

                // but first is there a ship?
                if (RemotesState == ALIVE) {
                    // test if ship is totally cloaked
                    if (RemotesCloak == -1) {
                        // ship isn't cloaked
                        // BUG FIX: mirror the player's thrust-frame draw (~5365).
                        // The book's original drew the remote a second time and
                        // subtracted 16 from currFrame an extra time (a second
                        // `-= 16` with no matching `+= 16`), leaving currFrame at
                        // F-16 — a garbage index into MotionDx[] that sent thrust in
                        // random directions once the remote ship could move.
                        if (RemotesEngine) {
                            // index into frames with thrust
                            RemotesShip.currFrame += 16;

                            // draw the ship with thrust showing
#ifdef BLAZERX
                            spriteDrawTinted(&RemotesShip, DoubleBuffer, 1,
                                RemotesTintColors, 2);
#else
                            spriteDrawClip(&RemotesShip, DoubleBuffer, 1);
#endif

                            // restore the original frame
                            RemotesShip.currFrame -= 16;
                        } else {
                            // no engines
#ifdef BLAZERX
                            spriteDrawTinted(&RemotesShip, DoubleBuffer, 1,
                                RemotesTintColors, 2);
#else
                            spriteDrawClip(&RemotesShip, DoubleBuffer, 1);
#endif
                        }
                    } else {
                        // player is cloaked

                        // test if player is trying to engage shields
                        if (RemotesShields) {
                            // cloaking is exclusive with shields, so it ends the
                            // shield session - drop them and start the recharge
                            shieldControl(THE_REMOTE, 0);
                            RemotesShieldTime = 0;
                            RemotesShieldCooldown = SHIELD_COOLDOWN_TIME;
                        }
                    }
                }

                // draw remaining priority objects
                drawMissiles();
                drawAlien();
                drawExplosions();
                drawNovas();

                // render scanner with new data
                if (PlayersScanner == 1) {
                    drawBlips();
                }

                // display coordinates and score
                sprintf(buffer, "COORDINATES:[%d,%d]    ", PlayersX, PlayersY);
                fontEngine1(0, 0, 0, 0, buffer, DoubleBuffer);

                sprintf(buffer, "SCORE: %d", PlayersScore);
#ifdef BLAZERX
                fontEngine1(800, 0, 0, 0, buffer, DoubleBuffer);
#else
#ifdef VBE_SUPPORT
                fontEngine1(500, 0, 0, 0, buffer, DoubleBuffer);
#else
                fontEngine1(250, 0, 0, 0, buffer, DoubleBuffer);
#endif
#endif

#ifdef BLAZERX
                #if DEBUG
                // diagnostic stuff
                sprintf(buffer, "REMOTE:[%d,%d]    ", RemotesX, RemotesY);
                fontEngine1(0, 32, 0, 0, buffer, DoubleBuffer);

                sprintf(buffer, "SENT: R=%d L=%d U=%d", sentRight, sentLeft, sentUp);
                fontEngine1(0, 64, 0, 0, buffer, DoubleBuffer);

                sprintf(buffer, "REC: R=%d L=%d U=%d ", recRight, recLeft, recUp);
                fontEngine1(0, 96, 0, 0, buffer, DoubleBuffer);
                #endif
#else
                #if DEBUG
                // diagnostic stuff
                sprintf(buffer, "REMOTE:[%d,%d]    ", RemotesX, RemotesY);
#ifdef VBE_SUPPORT
                fontEngine1(0, 20, 0, 0, buffer, DoubleBuffer);
#else
                fontEngine1(0, 10, 0, 0, buffer, DoubleBuffer);
#endif

                sprintf(buffer, "SENT: R=%d L=%d U=%d", sentRight, sentLeft, sentUp);
#ifdef VBE_SUPPORT
                fontEngine1(0, 40, 0, 0, buffer, DoubleBuffer);
#else
                fontEngine1(0, 20, 0, 0, buffer, DoubleBuffer);
#endif

                sprintf(buffer, "REC: R=%d L=%d U=%d ", recRight, recLeft, recUp);
#ifdef VBE_SUPPORT
                fontEngine1(0, 60, 0, 0, buffer, DoubleBuffer);
#else
                fontEngine1(0, 30, 0, 0, buffer, DoubleBuffer);
#endif
                #endif
#endif

                // display double buffer
                displayDoubleBuffer(DoubleBuffer, 0);

                // locl onto 18 frames per second max
                while (timerQuery() - StartingTime < 1);

                // check on music
                if (MusicEnabled) {
                    if (musicStatus() ==2 || musicStatus() == 0) {
                        // advance to next sequence
                        if (++GameSeqIndex == 18) {
                            GameSeqIndex = 0;
                        }

                        musicPlay(&Song, GameSequence[GameSeqIndex]);
                    }
                }
            }

            // test if there is a winner or user just decided to exit
            if (Winner == WINNER_REMOTE) {
                // tell player remote is winner
#ifdef BLAZERX
                techPrint(410, 256, "CEASE COMBAT!", VideoBuffer);
#else
#ifdef VBE_SUPPORT
                techPrint(256, 160, "CEASE COMBAT!", VideoBuffer);
#else
                techPrint(128, 80, "CEASE COMBAT!", VideoBuffer);
#endif
#endif

                if (MusicEnabled) {
                    musicStop();
                    musicPlay(&Song, 11);
                }

                timeDelay(25);
                digitalFxPlay(BLZLOS_VOC, 2);
#ifdef BLAZERX
                techPrint(384, 320, "YOU ARE DEFEATED", VideoBuffer);
#else
#ifdef VBE_SUPPORT
                techPrint(240, 200, "YOU ARE DEFEATED", VideoBuffer);
#else
                techPrint(120, 100, "YOU ARE DEFEATED", VideoBuffer);
#endif
#endif
                timeDelay(50);
            } else if (Winner == WINNER_PLAYER) {
                // tell player he is winner
#ifdef BLAZERX
                techPrint(410, 256, "CEASE_COMBAT!", VideoBuffer);
#else
#ifdef VBE_SUPPORT
                techPrint(256, 160, "CEASE_COMBAT!", VideoBuffer);
#else
                techPrint(128, 80, "CEASE_COMBAT!", VideoBuffer);
#endif
#endif

                if (MusicEnabled) {
                    musicStop();
                    musicPlay(&Song, 11);
                }

                timeDelay(25);
                digitalFxPlay(BLZWIN_VOC, 2);
#ifdef BLAZERX
                techPrint(368, 320, "YOU ARE VICTORIOUS", VideoBuffer);
#else
#ifdef VBE_SUPPORT
                techPrint(230, 200, "YOU ARE VICTORIOUS", VideoBuffer);
#else
                techPrint(115, 100, "YOU ARE VICTORIOUS", VideoBuffer);
#endif
#endif
                timeDelay(50);
            }

            // do all exit clean up here to move back to the setup state
            screenTransition(SCREEN_DARKNESS);

            // restart intro music
            if (MusicEnabled) {
                // stop game music, start intro music again
                IntroSeqIndex = 0;
                musicStop();
                musicPlay(&Song, IntroSequence[IntroSeqIndex]);
            }
        }
    }

    // exit in a very cool way
    screenTransition(SCREEN_DARKNESS);

    // free up all resources
    deleteDoubleBuffer();

    // remove the keyboard handler
    keyboardRemoveDriver();

    // close down FX
    digitalFxClose();

    // show the credits
    closingScreen();

    // close down music
    musicClose();

#ifdef BLAZERX
    // release the packet handle, DPMI callback and DOS buffers
    if (NetReady) {
        netShutdown();
    }

#endif
    setGraphicsMode(TEXT_MODE);

    // see ya!
    printf("\nSTARBLAZER Shutdown Normal.\n");
}
