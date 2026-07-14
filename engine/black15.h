// these constants are used to place the walls in world coordinates.
// WORLD_SCALE_X/Z and SCREEN_TO_WORLD_X/Z convert bspdemo.c's editor pixel
// coordinates into world units - both black15.c's own buildBspTree
// (highlight overlay, converts world back to screen) and bspdemo.c
// (screen to world) use these, so they're defined once here rather than
// separately in each file. Under VBE_SUPPORT the editor canvas doubles
// ((640/320) for X, (480/200) for Y - see bspdemo.c's BSP_MAX_X/Y), so
// SCREEN_TO_WORLD_X/Z grow by the same factor (the offset still centers
// against the bigger pixel range) and WORLD_SCALE_X/Z shrink by the same
// factor (each of the now-more-numerous editor pixels represents a
// proportionally smaller world distance) - net effect: the same-shaped
// level produces the exact same world geometry regardless of editor
// resolution, matching the same "world stays in book units, only the
// screen-facing conversion scales" camera pattern used elsewhere in the
// engine. WORLD_SCALE_Z landing on a non-integer is a direct consequence
// of mode-13h's non-square pixels, not a mistake.
#ifdef VBE_SUPPORT
#define WORLD_SCALE_X   1.0f            // 2 / (640/320)
#define WORLD_SCALE_Y   2               // unused anywhere - book leftover
#define WORLD_SCALE_Z   (-2.0f / 2.4f)  // -2 / (480/200)
#else
#define WORLD_SCALE_X   2   // scaling factors used to scale the
#define WORLD_SCALE_Y   2   // screen coordinates that the BSP
#define WORLD_SCALE_Z  -2   // is drawn with
#endif

#define WORLD_POS_X     0   // the final position to move the
#define WORLD_POS_Y     0   // walls to when "view" is selected
#define WORLD_POS_Z     300

#ifdef VBE_SUPPORT
#define SCREEN_TO_WORLD_X   -224    // -112 * (640/320)
#define SCREEN_TO_WORLD_Z   -240    // -100 * (480/200)
#else
#define SCREEN_TO_WORLD_X   -112    // the translation factors to move the origin
#define SCREEN_TO_WORLD_Z   -100    // to the center of the screen in mode 320x200
#endif

#define WALL_CEILING        20  // the y coordinate of the artificial ceiling
#define WALL_FLOOR         -20  // the y coordinate of the artificial floor

#define BSP_WALL_COLOR      47  // color of all the walls in the bsp tree
#define BSP_WALL_SHADE      47

// tests if two 3-d points are equal
#define POINTS_EQUAL_3D(p1,p2) (p1.x == p2.x && p1.y == p2.y && p1.z == p2.z)

// this structure holds a wall, it's very similar to our standard polygon
// structure, but stripped down for demo purposes
typedef struct WallType {
    int id; // used for debugging
    int color;  // color of wall
    Point3D wallWorld[4];   // the points that make up the wall
    Point3D wallCamera[4];  // the final camera coordinates of the wall
    Vector3D normal;        // the outward normal to the wall used during
                            // creation of BSP only, after that it becomes
                            // invalid
    struct WallType* link;  // pointer to next wall
    struct WallType* front; // pointer to walls in front
    struct WallType* back;  // pointer to walls behind
} Wall, *WallPtr;

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
    int color);

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
    int color);

void drawPolyListZ(void);

int createZBuffer(unsigned int height);

void deleteZBuffer(void);

void fillZBuffer(int value);

void bspWorldToCamera(WallPtr root);

void bspTranslate(WallPtr root, int xTrans, int yTrans, int zTrans);

void bspShade(WallPtr root);

void bspTraverse(WallPtr root);

void bspDelete(WallPtr root);

void bspPrint(WallPtr root);

void bspView(WallPtr root);

void buildBspTree(WallPtr root);

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
    float* yi);

extern int FAR* ZBuffer;    // the current z buffer memory
extern int FAR* ZBank1;     // memory bank 1 of z buffer
extern int FAR* ZBank2;     // memory bank 2 of z buffer

extern unsigned int ZHeight;    // the height of the z buffer
extern unsigned int ZHeight2;   // the height of half the z buffer
extern unsigned int ZBankSize;  // size of a z buffer bank in bytes

extern FILE* FpOut; // general output file

