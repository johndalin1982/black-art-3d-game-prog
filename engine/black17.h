#define MAX_POINTS_PER_POLY     4   // a maximum of four points per poly
#define MAX_VERTICES_PER_OBJECT 24  // this should be enough
#define MAX_POLYS_PER_OBJECT    16  // this will have to do!
#define MAX_OBJECTS             32  // maximum number of objects in world
#define MAX_POLYS_PER_FRAME     128 // maximum number of polys in a single
                                    // animation frame

// polygon shading specifiers for PLG files

// format = 0xh3 h2 h1h0
// h3   = shading type
// h2   = special attributes= d3d2d1d0

// d3,d2,d1 unused, d0 = two sided flag, 0 = 1 sided, 1 = 2 sided

// h1h0 = color 0-255

#define ONE_SIDED               0   // number of sides polygon has
#define TWO_SIDED               1

#define CONSTANT_SHADING        0
#define FLAT_SHADING            1
#define GOURAUD_SHADING         2   // actually metallic under PLG definition
#define SPFX_SHADING            3

#define ASPECT_RATIO            (float)0.8  // the aspect ratio
#define INVERSE_ASPECT_RATIO    (float)1.25 // the inverse of the aspect ratio

#define HALF_SCREEN_WIDTH       160 // center of screen
#define HALF_SCREEN_HEIGHT      100

#define POLY_CLIP_MIN_X         0   // minimum x,y clip values
#define POLY_CLIP_MIN_Y         0

#define POLY_CLIP_MAX_X         319 // maximum x,y clip values
#define POLY_CLIP_MAX_Y         199

#define CLIP_Z_MODE             0   // this constant tells the clipper to do a
                                    // simple z extent clip

#define CLIP_XYZ_MODE           1   // this constant tells the clipper to do
                                    // a full 3D volume clip

#define OBJECT_CULL_Z_MODE      0   // this constant tells the object culler to do a
                                    // simple z extent clip

#define OBJECT_CULL_XYZ_MODE    1   // this constant tells the object culler to do
                                    // a full 3D volume clip

// these are the constants needed for the shader engine
// note that brightness increses with smaller values and there
// are 16 shades of each color eg. bright blue is 144 and dark blue is 159

#define SHADE_GREY              31  // hex value = 1F
#define SHADE_GREEN             111 // hex value = 6F
#define SHADE_BLUE              159 // hex value = 9F
#define SHADE_RED               47  // hex value = 2F
#define SHADE_YELLOW            79  // hex value = 4F
#define SHADE_BROWN             223 // hex value = DF
#define SHADE_LIGHT_BROWN       207 // hex value = CF
#define SHADE_PURPLE            175 // hex value = AF
#define SHADE_CYAN              127 // hex value = 7F
#define SHADE_LAVENDER          191 // hex value = BF

// defines for object collisions

#define NO_COLLISION            0
#define SOFT_COLLISION          1
#define HARD_COLLISION          2

// defines for polygon list generator

#define RESET_POLY_LIST         0
#define ADD_TO_POLY_LIST        1

// fixed point stuff

#define FP_SHIFT                16          // 16:16 format
#define FP_SCALE                65536L      // 2^16 = 65536, used to convert floats

typedef float Matrix4x4[4][4];  // the standard 4x4 homogenous matrix

typedef float Matrix1x4[4];     // a 1x4 matrix or a row vector

// this structure holds a vector or a simple 3-D point
typedef struct Vector3DType {
    float x, y, z, w;   // a 3-D vector along with normalization factor
                        // if needed
} Point3D, Vector3D, *Point3DPtr, *Vector3DPtr;

// this function holds a objects orientation or direction relative to the
// axis of a left handed system
typedef struct Dir3DType {
    int angX,   // angle relative to x axis
        angY,   // angle relative to y axis
        angZ;   // angle relative to z axis
} Dir3D, *Dir3DPtr;

// this structure holds a polygon, but is used internally by the object definition
typedef struct PolygonType {
    int numPoints;          // number of points in polygon (usually 3 or 4)
    int vertexList[MAX_POINTS_PER_POLY];    // the index number of vertices
    int color;              // color of polygon
    int shade;              // the final shade of color after lighting
    int shading;            // type of lighting, flat or constant shading
    int twoSided;           // flags if the polygon is two sided
    int visible;            // used to remove backfaces
    int active;             // used to turn faces on and off
    int clipped;            // flags that polygon has been clipped or removed
    float normalLength;     // pre-computed magnitude of normal
} Polygon, *PolygonPtr;

// this structure holds a final polygon facet and is self contained
typedef struct FacetType {
    int numPoints;          // number of vertices
    int color;              // color of polygon
    int shade;              // the final shade of color after lighting
    int shading;            // type of shading to use
    int twoSided;           // is the facet two sided
    int visible;            // is the facet transparent
    int clipped;            // has this poly been clipped
    int active;             // used to turn faces on and off
    Point3D vertexList[MAX_POINTS_PER_POLY]; // the points that make up the polygon facet
    float normalLength;     // holds pre-computed length of normal
    int averageZ;           // holds average z, used in sorting
} Facet, *FacetPtr;

// this structure holds an object
typedef struct ObjectType {
    int id;                                             // identification number of object
    int numVertices;                                    // total number of vertices in object
    Point3D verticesLocal[MAX_VERTICES_PER_OBJECT];     // local vertices
    Point3D verticesWorld[MAX_VERTICES_PER_OBJECT];     // world vertices
    Point3D verticesCamera[MAX_VERTICES_PER_OBJECT];    // camera vertices
    int numPolys;                                       // the number of polygons in the object
    Polygon polys[MAX_POLYS_PER_OBJECT];                // the polygons that make up the object
    float radius;                                       // the average radius of object
    int state;                                          // state of object
    Point3D worldPos;                                   // position of object in world coordinates
} Object, *ObjectPtr;

// fixed point type
typedef long Fixed;

int loadPaletteDisk(char* filename, RgbPalettePtr palette);

int savePaletteDisk(char* filename, RgbPalettePtr palette);

float computeObjectRadius(ObjectPtr object);

void buildLookUpTables(void);

float dotProduct3D(Vector3DPtr u, Vector3DPtr v);

void makeVector3D(
    Point3DPtr init,
    Point3DPtr term,
    Vector3DPtr result);

void crossProduct3D(
    Vector3DPtr u,
    Vector3DPtr v,
    Vector3DPtr normal);

float vectorMag3D(Vector3DPtr v);

void matPrint4x4(Matrix4x4 a);

void matPrint1x4(Matrix1x4 a);

void matMul4x4With4x4(
    Matrix4x4 a,
    Matrix4x4 b,
    Matrix4x4 result);

void matMul1x4With4x4(
    Matrix1x4 a,
    Matrix4x4 b,
    Matrix1x4 result);

void matIdentity4x4(Matrix4x4 a);

void matZero4x4(Matrix4x4 a);

void matCopy4x4(Matrix4x4 source, Matrix4x4 destination);

void localToWorldObject(ObjectPtr object);

void createWorldToCamera(void);

void worldToCameraObject(ObjectPtr object);

void rotateObject(ObjectPtr object, int angleX, int angleY, int angleZ);

void translateObject(ObjectPtr object, int xTrans, int yTrans, int zTrans);

int objectsCollide(ObjectPtr object1, ObjectPtr object2);

void scaleObject(ObjectPtr object, float scaleFactor);

void positionObject(ObjectPtr object, int x, int y, int z);

char* plgGetLine(char* string, int maxLength, FILE* fp);

int plgLoadObject(ObjectPtr object, char* filename, float scale);

void clipObject3D(ObjectPtr object, int mode);

void removeBackfacesAndShade(ObjectPtr object);

int removeObject(ObjectPtr object, int mode);

void generatePolyList(ObjectPtr object, int mode);

int polyCompare(FacetPtr* arg1, FacetPtr* arg2);

void sortPolyList(void);

void printPolyList(void);

void drawPolyList(void);

void drawTriangle2D(
    int x1, int y1,
    int x2, int y2,
    int x3, int y3,
    int color);

void drawTopTriangle(
    int x1, int y1,
    int x2, int y2,
    int x3, int y3,
    int color);

void drawBottomTriangle(
    int x1, int y1,
    int x2, int y2,
    int x3, int y3,
    int color);

void triangleLine(
    unsigned char FAR* destAddr,
    unsigned int xs,
    unsigned int xe,
    int color);

void makeGreyPalette(void);

// new 32 bit functions

void displayDoubleBuffer32(unsigned char FAR* buffer, int y);

void fillDoubleBuffer32(int color);

// external assembly language functions —
// __cdecl forces stack-based C calling convention to match the .asm files
// (which use PROC C / PROC FAR C); without this, wcc386 defaults to
// __watcall register-based calling and the linker-success build crashes at
// runtime on the first ASM call

void __cdecl fquadcpy(void FAR* dest, void FAR* source, long count);

void __cdecl fquadset(void FAR* dest, long data, long count);

void __cdecl triangle32Line(
    unsigned char FAR* destAddr,
    unsigned int xs,
    unsigned int xe,
    int color);

void __cdecl triangle16Line(
    unsigned char FAR* destAddr,
    unsigned int xs,
    unsigned int xe,
    int color);

void __cdecl triangleAsm(
    void FAR* destAddr,
    int y1,
    int y3,
    float xs,
    float xe,
    float dxLeft,
    float dxRight,
    int color);

// fixed point functions

Fixed __cdecl fpMul(Fixed multiplicand, Fixed multiplier);

Fixed __cdecl fpDiv(Fixed dividend, Fixed divisor);

extern float ClipNearZ,         // the near or hither clipping plane
             ClipFarZ,          // the far or yon clipping plane
             ScreenWidth,       // dimensions of the screen
             ScreenHeight;

extern float ViewingDistance;                       // distance of projection plane from camera

extern Point3D ViewPoint;                           // position of camera

extern Vector3D LightSource;                        // position of point light source

extern float AmbientLight;                          // ambient light level

extern Dir3D ViewAngle;                             // angle of camera

extern Matrix4x4 GlobalView;                        // the global inverse world to camera

extern RgbPalette ColorPalette3D;                   // the color palette used for the 3D system

extern int NumObjects;                              // number of objects in the world

extern ObjectPtr WorldObjectList[MAX_OBJECTS];      // the objects in the world

extern int NumPolysFrame;                           // the number of polys in this frame

extern FacetPtr WorldPolys[MAX_POLYS_PER_FRAME];    // the visible polygons for this frame

extern Facet WorldPolyStorage[MAX_POLYS_PER_FRAME]; // the storage for the visible
                                                    // polygons is pre-allocated
                                                    // so it doesn't need to be
                                                    // allocated frame by frame

// look up tables

extern float SinLook[360 + 1],  // SIN from 0 to 360
             CosLook[360 + 1];  // COSINE from 0 to 360

// the clipping region, set it to default on start up

extern int PolyClipMinX,
           PolyClipMinY,

           PolyClipMaxX,
           PolyClipMaxY;

extern Sprite Textures; // this holds the textures
