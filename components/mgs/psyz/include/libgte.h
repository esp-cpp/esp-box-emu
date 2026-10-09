#ifndef LIBGTE_H
#define LIBGTE_H
#include <psyz/types.h>

/**
 * @file libgte.h
 * @brief GTE (Geometry Transform Engine) Library
 *
 * This library provides access to the PlayStation's GTE coprocessor for
 * high-speed 3D coordinate transformations, lighting calculations, and
 * matrix operations.
 *
 * Key features:
 * - 3D coordinate transformation (rotation, translation, perspective)
 * - Matrix operations (multiply, inverse, transpose)
 * - Vector operations (inner product, outer product, normalize)
 * - Lighting calculations (flat, Gouraud shading)
 * - Clipping and depth calculations
 * - Color interpolation
 * - Fixed-point arithmetic optimizations
 *
 * Note: GTE operations use fixed-point arithmetic (12-bit fraction).
 */

/* Forward declarations for structures */

/**
 * @brief Matrix structure
 *
 * Specifies each component on the MATRIX m[i][j]. Specifies the transfer
 * volume after conversion on the MATRIX t[i]. Pay attention to the differing
 * word lengths on m and t.
 */
typedef struct {
    short m[3][3]; /**< 3x3 matrix coefficient value */
    int t[3];      /**< Parallel transfer volume */
} MATRIX;

/**
 * @brief Long vector (32-bit)
 *
 * Vector with 32-bit coordinates.
 */
typedef struct {
    int vx, vy, vz;  /**< Vector coordinates */
    int pad;         /**< System reserved */
} VECTOR;

/**
 * @brief Short vector (16-bit)
 *
 * Vector with 16-bit coordinates.
 */
typedef struct {
    short vx, vy, vz; /**< Vector coordinates */
    short pad;        /**< System reserved */
} SVECTOR;

/**
 * @brief Character vector
 *
 * Color palette vector.
 */
typedef struct {
    u_char r, g, b; /**< Color palette (RGB) */
    u_char cd;      /**< GPU code */
} CVECTOR;

/**
 * @brief 2D vector
 *
 * 2D vector with short coordinates.
 */
typedef struct {
    short vx, vy; /**< Vector coordinates */
} DVECTOR;

/**
 * @brief Division vertex vector data
 *
 * Used for polygon division operations.
 */
typedef struct {
    SVECTOR v;    /**< Local object 3D vertex */
    u_char uv[2]; /**< Texture mapping data */
    u_short pad;  /**< Padding */
    CVECTOR c;    /**< Vertex color palette */
    DVECTOR sxy;  /**< Screen 2D vertex */
    u_long sz;    /**< Clip Z-data */
} RVECTOR;

/**
 * @brief Triangular recursive vector data
 *
 * Used for triangle polygon division.
 */
typedef struct {
    RVECTOR r01, r12, r20; /**< Division vertex vector data */
    RVECTOR *r0, *r1, *r2; /**< Pointer to division vector data */
    u_long* rtn;           /**< Pointer to return address for assembler */
} CRVECTOR3;

/**
 * @brief Quadrilateral recursive vector data
 *
 * Used for quadrilateral polygon division.
 */
typedef struct {
    RVECTOR r01, r02, r31, r32, rc; /**< Division vertex vector data */
    RVECTOR *r0, *r1, *r2, *r3; /**< Pointer to division vertex vector data */
    u_long* rtn;                /**< Pointer to return address for assembler */
} CRVECTOR4;

/**
 * @brief Triangular division buffer
 *
 * Buffer used for triangle polygon division operations.
 */
typedef struct {
    u_long ndiv;     /**< Number of divisions */
    u_long pih, piv; /**< Clip area specification (display screen resolution) */
    u_short clut;    /**< CLUT */
    u_short tpage;   /**< Texture page */
    CVECTOR rgbc;    /**< Code + RGB color */
    u_long* ot;      /**< Pointer to OT */
    RVECTOR r0, r1, r2; /**< Division vertex vector data */
    CRVECTOR3 cr[5];    /**< Triangular recursive vector data */
} DIVPOLYGON3;

/**
 * @brief Quadrilateral division buffer
 *
 * Buffer used for quadrilateral polygon division operations.
 */
typedef struct {
    u_long ndiv;     /**< Number of divisions */
    u_long pih, piv; /**< Clip area specification (display screen resolution) */
    u_short clut;    /**< CLUT */
    u_short tpage;   /**< Texture page */
    CVECTOR rgbc;    /**< Code + RGB color */
    u_long* ot;      /**< Pointer to OT */
    RVECTOR r0, r1, r2, r3; /**< Division vertex vector data */
    CRVECTOR4 cr[5];        /**< Quadrilateral recursive vector data */
} DIVPOLYGON4;

/**
 * @brief Clip vector data
 *
 * Vector data used for clipping operations.
 */
typedef struct {
    SVECTOR v;       /**< Local object 3D vertex */
    VECTOR sxyz;     /**< Screen 3D vertex */
    DVECTOR sxy;     /**< Screen 2D vertex */
    CVECTOR rgb;     /**< Color palette */
    short txuv, pad; /**< Texture mapping data */
    int chx, chy;    /**< Clip area data */
} EVECTOR;

/**
 * @brief Triangle polygon
 *
 * Triangle polygon structure with screen coordinates, Z values, texture
 * coordinates, RGB values and code.
 */
typedef struct {
    short sxy[3][2]; /**< Screen coordinates */
    short sz[3][2];  /**< Screen Z coordinates */
    short uv[3][2];  /**< Texture coordinates */
    short rgb[3][3]; /**< RGB values */
    short code;      /**< Code (F3=1, FT3=2, G3=3, GT3=4) */
} POL3;

/**
 * @brief Four-sided polygon
 *
 * Quadrilateral polygon structure with screen coordinates, Z values, texture
 * coordinates, RGB values and code.
 */
typedef struct {
    short sxy[4][2]; /**< Screen coordinates */
    short sz[4][2];  /**< Screen Z coordinates */
    short uv[4][2];  /**< Texture coordinates */
    short rgb[4][3]; /**< RGB values */
    short code;      /**< Code (F4=5, FT4=6, G4=7, GT4=8) */
} POL4;

/**
 * @brief Sprite polygon
 *
 * Sprite polygon structure.
 */
typedef struct {
    short sxy;  /**< Screen coordinates */
    short sz;   /**< Screen Z coordinate */
    short wh;   /**< Width and height */
    short uv;   /**< Texture coordinates */
    short rgb;  /**< RGB value */
    short code; /**< Code */
} SPOL;

/**
 * @brief Triangular mesh
 *
 * Triangular mesh structure for batch rendering.
 */
typedef struct {
    SVECTOR* v; /**< Pointer to shared vertex coordinates array */
    SVECTOR* n; /**< Pointer to shared normal array */
    SVECTOR* u; /**< Pointer to shared texture coordinates array */
    CVECTOR* c; /**< Pointer to shared color data array */
    u_long len; /**< Vertex length */
} TMESH;

/**
 * @brief Quadrilateral mesh
 *
 * Quadrilateral mesh structure for batch rendering.
 */
typedef struct {
    SVECTOR* v;  /**< Pointer to shared vertex coordinates array */
    SVECTOR* n;  /**< Pointer to shared normal array */
    SVECTOR* u;  /**< Pointer to shared texture coordinates array */
    CVECTOR* c;  /**< Pointer to shared color data array */
    u_long lenv; /**< Vertex length (horizontal) */
    u_long lenh; /**< Vertex length (vertical) */
} QMESH;

/* Function declarations */

/**
 * @brief Initialize geometry system
 *
 * Initializes the GTE (Geometry Transform Engine) system.
 */
void InitGeom(void);

/**
 * @brief Average Z value for 3 vertices
 *
 * Calculates the average Z value of three vertices for depth sorting.
 *
 * @param sz0 Z value of vertex 0
 * @param sz1 Z value of vertex 1
 * @param sz2 Z value of vertex 2
 * @return Average Z value
 */
long AverageZ3(long sz0, long sz1, long sz2);

/**
 * @brief Average Z value for 4 vertices
 *
 * Calculates the average Z value of four vertices for depth sorting.
 *
 * @param sz0 Z value of vertex 0
 * @param sz1 Z value of vertex 1
 * @param sz2 Z value of vertex 2
 * @param sz3 Z value of vertex 3
 * @return Average Z value
 */
long AverageZ4(long sz0, long sz1, long sz2, long sz3);

/**
 * @brief Create rotation matrix
 *
 * Creates a rotation matrix from a rotation vector.
 *
 * @param r Pointer to rotation vector (input)
 * @param m Pointer to rotation matrix (output)
 * @return Pointer to rotation matrix
 */
MATRIX* RotMatrix(SVECTOR* r, MATRIX* m);

/**
 * @brief Create rotation matrix around Y axis
 *
 * Creates a rotation matrix for rotation around the Y axis.
 *
 * @param r Rotation angle (input)
 * @param m Pointer to rotation matrix (input/output)
 * @return Pointer to rotation matrix
 */
MATRIX* RotMatrixY(long r, MATRIX* m);

/**
 * @brief Create rotation matrix around X axis
 *
 * Creates a rotation matrix for rotation around the X axis.
 *
 * @param r Rotation angle
 * @param m Pointer to rotation matrix
 * @return Pointer to rotation matrix
 */
MATRIX* RotMatrixX(long r, MATRIX* m);

/**
 * @brief Create rotation matrix around Z axis
 *
 * Creates a rotation matrix for rotation around the Z axis.
 *
 * @param r Rotation angle
 * @param m Pointer to rotation matrix
 * @return Pointer to rotation matrix
 */
MATRIX* RotMatrixZ(long r, MATRIX* m);

/**
 * @brief Create translation matrix
 *
 * Creates a translation matrix from a translation vector.
 *
 * @param m Pointer to matrix (input/output)
 * @param v Pointer to translation vector (input)
 * @return Pointer to matrix
 */
MATRIX* TransMatrix(MATRIX* m, VECTOR* v);

/**
 * @brief Scale matrix
 *
 * Scales a matrix by a scaling vector.
 *
 * @param m Pointer to matrix
 * @param v Pointer to scaling vector
 * @return Pointer to matrix
 */
MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/**
 * @brief Multiply matrices
 *
 * Multiplies two matrices: m0 = m0 * m1.
 *
 * @param m0 Pointer to first matrix (input/output)
 * @param m1 Pointer to second matrix (input)
 * @return Pointer to result matrix
 */
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/**
 * @brief Multiply matrices (alternative)
 *
 * Multiplies two matrices: m2 = m0 * m1.
 *
 * @param m0 Pointer to first matrix
 * @param m1 Pointer to second matrix
 * @param m2 Pointer to result matrix
 * @return Pointer to result matrix
 */
MATRIX* MulMatrix0(MATRIX* m0, MATRIX* m1, MATRIX* m2);

/**
 * @brief Multiply matrix by rotation matrix
 *
 * Multiplies a matrix by the current rotation matrix.
 *
 * @param m Pointer to matrix
 * @return Pointer to result matrix
 */
MATRIX* MulRotMatrix(MATRIX* m);

/**
 * @brief Transpose matrix
 *
 * Transposes a matrix.
 *
 * @param m0 Pointer to source matrix
 * @param m1 Pointer to destination matrix
 * @return Pointer to result matrix
 */
MATRIX* TransposeMatrix(MATRIX* m0, MATRIX* m1);

/**
 * @brief Compose matrix
 *
 * Composes a matrix from rotation, translation and scaling parameters.
 *
 * @param rot Pointer to rotation vector
 * @param trans Pointer to translation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
/* PSY-Q's CompMatrix multiplies three matrices; the previous declaration
 * here had a different signature and no implementation anywhere. */
MATRIX* CompMatrix(MATRIX* m0, MATRIX* m1, MATRIX* m2);

/**
 * @brief Set geometry offset
 *
 * Sets the screen offset for geometry transformations.
 *
 * @param ofx X offset
 * @param ofy Y offset
 */
void SetGeomOffset(long ofx, long ofy);

/**
 * @brief Set geometry screen distance
 *
 * Sets the distance from the viewpoint to the projection plane.
 *
 * @param h Projection distance
 */
void SetGeomScreen(long h);

/**
 * @brief Set rotation matrix
 *
 * Sets the current rotation matrix in the GTE.
 *
 * @param m Pointer to rotation matrix
 */
void SetRotMatrix(MATRIX* m);

/**
 * @brief Set translation matrix
 *
 * Sets the current translation matrix in the GTE.
 *
 * @param m Pointer to translation matrix
 */
void SetTransMatrix(MATRIX* m);

/**
 * @brief Set translation vector
 *
 * Sets the translation vector (TX, TY, TZ) from a VECTOR.
 *
 * @param v Pointer to translation vector
 */
void SetTransVector(VECTOR* v);

/**
 * @brief Set light matrix
 *
 * Sets the light matrix for light source calculations.
 *
 * @param m Pointer to light matrix
 */
void SetLightMatrix(MATRIX* m);

/**
 * @brief Set color matrix
 *
 * Sets the color matrix for color transformation.
 *
 * @param m Pointer to color matrix
 */
void SetColorMatrix(MATRIX* m);

/**
 * @brief Set background color
 *
 * Sets the background color for light source calculations.
 *
 * @param rbk Red component
 * @param gbk Green component
 * @param bbk Blue component
 */
void SetBackColor(long rbk, long gbk, long bbk);

/**
 * @brief Set far color
 *
 * Sets the far color for depth cueing/fog effects.
 *
 * @param rfc Red component
 * @param gfc Green component
 * @param bfc Blue component
 */
void SetFarColor(long rfc, long gfc, long bfc);

/**
 * @brief Set fog near parameters
 *
 * Sets the near fog parameters for depth cueing.
 *
 * @param a Fog coefficient
 * @param h Distance between viewpoint and screen
 */
void SetFogNear(long a, long h);

/**
 * @brief Rotate and translate
 *
 * Performs rotation and translation on a vector.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Flag value
 */
void RotTrans(SVECTOR* v0, VECTOR* v1, int* flag);

/**
 * @brief Rotate, translate and perspective transform
 *
 * Performs rotation, translation and perspective transformation on a vector.
 *
 * @param v0 Pointer to input vector
 * @param sxy Pointer to screen coordinates (output)
 * @param p Pointer to interpolated value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotTransPers(SVECTOR* v0, int* sxy, int* p, int* flag);

/**
 * @brief Rotate, translate and perspective transform (3 vertices)
 *
 * Performs rotation, translation and perspective transformation on 3 vectors.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param p Pointer to interpolated value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotTransPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0, int* sxy1,
                   int* sxy2, int* p, int* flag);

/**
 * @brief Rotate, translate and perspective transform (4 vertices)
 *
 * Performs rotation, translation and perspective transformation on 4 vectors.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param v3 Pointer to fourth input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param sxy3 Pointer to fourth screen coordinates (output)
 * @param p Pointer to interpolated value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotTransPers4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0, int* sxy1,
    int* sxy2, int* sxy3, int* p, int* flag);

/**
 * @brief Rotate, average and normal clip (3 vertices)
 *
 * Performs rotation, averaging and normal clipping for 3 vertices.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param otz Pointer to OTZ value (output)
 * @param flag Pointer to flag (output)
 * @return Normal clip result
 */
long RotAverageNclip3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0,
                      int* sxy1, int* sxy2, int* p, int* otz, int* flag);

/**
 * @brief Rotate, average and normal clip (4 vertices)
 *
 * Performs rotation, averaging and normal clipping for 4 vertices.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param v3 Pointer to fourth input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param sxy3 Pointer to fourth screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param otz Pointer to OTZ value (output)
 * @param flag Pointer to flag (output)
 * @return Normal clip result
 */
long RotAverageNclip4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0, int* sxy1,
    int* sxy2, int* sxy3, int* p, int* otz, int* flag);

/**
 * @brief Normal color calculation
 *
 * Calculates the color based on normal vector and light sources.
 *
 * @param v0 Pointer to normal vector (input)
 * @param v1 Pointer to primary color vector (input)
 * @param v2 Pointer to color vector (output)
 */
void NormalColor(SVECTOR* v0, CVECTOR* v1, CVECTOR* v2);

/**
 * @brief Normal color calculation with color matrix
 *
 * Calculates the color based on normal vector, light sources and color matrix.
 *
 * @param v0 Pointer to normal vector (input)
 * @param v1 Pointer to primary color vector (input)
 * @param v2 Pointer to color vector (output)
 */
void NormalColorCol(SVECTOR* v0, CVECTOR* v1, CVECTOR* v2);

/**
 * @brief Normal color calculation with depth cueing
 *
 * Calculates the color based on normal vector, light sources and depth cueing.
 *
 * @param v0 Pointer to normal vector (input)
 * @param v1 Pointer to primary color vector (input)
 * @param v2 Pointer to color vector (output)
 */
void NormalColorDpq(SVECTOR* v0, CVECTOR* v1, CVECTOR* v2);

/**
 * @brief Normal color calculation (3 vertices)
 *
 * Calculates colors for 3 vertices based on normal vectors and light sources.
 *
 * @param v0 Pointer to first normal vector
 * @param v1 Pointer to second normal vector
 * @param v2 Pointer to third normal vector
 * @param v3 Pointer to primary color vector
 * @param v4 Pointer to first output color
 * @param v5 Pointer to second output color
 * @param v6 Pointer to third output color
 */
void NormalColor3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, CVECTOR* v3,
                  CVECTOR* v4, CVECTOR* v5, CVECTOR* v6);

/**
 * @brief Outer product of three points.
 *
 * Returns the outer product for a triangle formed by three points (sx0, sy0),
 * (sx1, sy1), and (sx2, sy2).
 *
 * When viewed from the direction of the viewpoint (Z axis negative) the value
 * is positive when the triangle is righthanded
 *
 * @param sxy0 Vector 0 coordinates (upper 16-bit: X, lower 16-bit: Y)
 * @param sxy1 Vector 1 coordinates (upper 16-bit: X, lower 16-bit: Y)
 * @param sxy2 Vector 2 coordinates (upper 16-bit: X, lower 16-bit: Y)
 * @return Clip result (positive = front-facing, negative = back-facing)
 */
long NormalClip(long sxy0, long sxy1, long sxy2);

/**
 * @brief Apply matrix to vector
 *
 * Applies a matrix transformation to a vector.
 *
 * @param m Pointer to matrix
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 */
void ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1);

/**
 * @brief Apply rotation matrix
 *
 * Applies the current rotation matrix to a vector.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 */
void ApplyRotMatrix(SVECTOR* v0, VECTOR* v1);

/**
 * @brief Square root (integer part)
 *
 * Calculates the square root of a value, returning the integer part.
 *
 * @param a Input value
 * @return Square root (integer part)
 */
long SquareRoot0(long a);

/**
 * @brief Square root (with fraction)
 *
 * Calculates the square root of a value, returning integer and fractional
 * parts.
 *
 * @param a Input value
 * @return Square root (with 12-bit fraction)
 */
long SquareRoot12(long a);

/**
 * @brief Cosine function
 *
 * Calculates the cosine of an angle (4096 = 1 degree).
 *
 * @param a Angle value
 * @return Cosine value
 */
int rcos(int a);

/**
 * @brief Sine function
 *
 * Calculates the sine of an angle (4096 = 1 degree).
 *
 * @param a Angle value
 * @return Sine value
 */
int rsin(int a);

/**
 * @brief Arc tangent function
 *
 * Calculates the arc tangent of y/x.
 *
 * @param y Y value
 * @param x X value
 * @return Angle value (4096 = 1 degree)
 */
long ratan2(long y, long x);

/**
 * @brief Read rotation matrix
 *
 * Reads the current rotation matrix from the GTE.
 *
 * @param m Pointer to matrix (output)
 * @return Pointer to matrix
 */
MATRIX* ReadRotMatrix(MATRIX* m);

/**
 * @brief Read light matrix
 *
 * Reads the current light matrix from the GTE.
 *
 * @param m Pointer to matrix (output)
 * @return Pointer to matrix
 */
MATRIX* ReadLightMatrix(MATRIX* m);

/**
 * @brief Read color matrix
 *
 * Reads the current color matrix from the GTE.
 *
 * @param m Pointer to matrix (output)
 * @return Pointer to matrix
 */
MATRIX* ReadColorMatrix(MATRIX* m);

/**
 * @brief Push matrix
 *
 * Pushes the current matrix onto the matrix stack.
 */
void PushMatrix(void);

/**
 * @brief Pop matrix
 *
 * Pops a matrix from the matrix stack.
 */
void PopMatrix(void);

/**
 * @brief Read geometry offset
 *
 * Reads the current geometry offset values.
 *
 * @param x Pointer to X offset (output)
 * @param y Pointer to Y offset (output)
 */
void ReadGeomOffset(int* x, int* y);

/**
 * @brief Read geometry screen distance
 *
 * Reads the current projection distance.
 *
 * @return Projection distance
 */
long ReadGeomScreen(void);

/**
 * @brief Outer product
 *
 * Calculates the outer (cross) product of two vectors.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to output vector
 * @return Pointer to output vector
 */
VECTOR* OuterProduct0(VECTOR* v0, VECTOR* v1, VECTOR* v2);

/**
 * @brief Outer product (with 12-bit fraction)
 *
 * Calculates the outer product with 12-bit fractional precision.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to output vector
 * @return Pointer to output vector
 */
VECTOR* OuterProduct12(VECTOR* v0, VECTOR* v1, VECTOR* v2);

/**
 * @brief Normalize vector
 *
 * Normalizes a vector to unit length.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Length of input vector
 */
long VectorNormal(VECTOR* v0, VECTOR* v1);

/**
 * @brief Normalize short vector
 *
 * Normalizes a short vector to unit length.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Length of input vector
 */
long VectorNormalS(VECTOR* v0, SVECTOR* v1);

/**
 * @brief Normalize short vector (short to short)
 *
 * Normalizes a short vector to unit length, returning short vector.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Sum of squared v0 elements
 */
long VectorNormalSS(SVECTOR* v0, SVECTOR* v1);

/**
 * @brief Get OTZ from depth cueing interpolation value
 *
 * Converts depth cueing interpolation value p to OTZ value.
 *
 * @param p Interpolation value (0 to 4096)
 * @param projection Distance between visual point and screen
 * @return OTZ value
 */
long p2otz(long p, long projection);

/**
 * @brief Perspective conversion texture mapping
 *
 * Performs texture mapping with no distortion (flat texture, no light source).
 *
 * @param abuf ID of displayed buffer
 * @param vertex 3D coordinates of 4 vertices
 * @param tex Texture address of 4 vertices
 * @param dtext Pointer to texture storage location
 */
void pers_map(int abuf, SVECTOR** vertex, int tex[4][2], u_short* dtext);

/**
 * @brief Phong shading line
 *
 * Performs one line Phong shading.
 *
 * @param istart_x X coordinate of starting point
 * @param iend_x X coordinate of finishing point
 * @param p Differential X coordinate of fs value
 * @param q Differential caused by X coordinate of ft value
 * @param pixx Pixel pointer
 * @param fs Interpolation coefficient at start point
 * @param ft Interpolation coefficient at start point
 * @param i4 (Line number) %4 due to dithering
 * @param det Queue method of edge queue
 */
void PhongLine(int istart_x, int iend_x, int p, int q, u_short* pixx, int fs,
               int ft, int i4, int det);

/**
 * @brief Read RGBcd values
 *
 * Stores the RGBcd0, RGBcd1, and RGBcd2 values.
 *
 * @param v0 Pointer to first color vector (output)
 * @param v1 Pointer to second color vector (output)
 * @param v2 Pointer to third color vector (output)
 */
void ReadRGBfifo(CVECTOR* v0, CVECTOR* v1, CVECTOR* v2);

/**
 * @brief Read SXSY values
 *
 * Stores the sx0, sy0, sx1, sy1, sx2, and sy2 values.
 *
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 */
void ReadSXSYfifo(int* sxy0, int* sxy1, int* sxy2);

/**
 * @brief Read SZ values (3 vertices)
 *
 * Stores the sz0, sz1, and sz2 values.
 *
 * @param sz0 Pointer to first SZ value (output)
 * @param sz1 Pointer to second SZ value (output)
 * @param sz2 Pointer to third SZ value (output)
 */
void ReadSZfifo3(int* sz0, int* sz1, int* sz2);

/**
 * @brief Read SZ values (4 vertices)
 *
 * Stores the szx, sz0, sz1, and sz2 values.
 *
 * @param szx Pointer to extra SZ value (output)
 * @param sz0 Pointer to first SZ value (output)
 * @param sz1 Pointer to second SZ value (output)
 * @param sz2 Pointer to third SZ value (output)
 */
void ReadSZfifo4(int* szx, int* sz0, int* sz1, int* sz2);

/**
 * @brief Rotate, average and get Z values (3 vertices)
 *
 * Performs coordinate and perspective transformation for 3 points.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value (average of three Z values)
 */
long RotAverage3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0, int* sxy1,
                 int* sxy2, int* p, int* flag);

/**
 * @brief Rotate, average (3 vertices, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @return Flag value
 */
long RotAverage3_nom(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);

/**
 * @brief Rotate, average and get Z values (4 vertices)
 *
 * Performs coordinate and perspective transformation for 4 points.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param v3 Pointer to fourth input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param sxy3 Pointer to fourth screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value (average of four Z values)
 */
long RotAverage4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0,
                 int* sxy1, int* sxy2, int* sxy3, int* p, int* flag);

/**
 * @brief Coordinate transformation with normal clip (3 vertices)
 *
 * Performs rotation, averaging, normal clipping and returns outer product.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param otz Pointer to OTZ value (output)
 * @param flag Pointer to flag (output)
 * @return Outer product (negative = back-facing)
 */
long RotAverageNclip3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0,
                      int* sxy1, int* sxy2, int* p, int* otz, int* flag);

/**
 * @brief Coordinate transformation with normal clip (3 vertices, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @return None
 */
long RotAverageNclip3_nom(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);

/**
 * @brief Coordinate transformation with normal clip (4 vertices)
 *
 * Performs rotation, averaging, normal clipping and returns outer product.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param v3 Pointer to fourth input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param sxy3 Pointer to fourth screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param otz Pointer to OTZ value (output)
 * @param flag Pointer to flag (output)
 * @return Outer product (negative = back-facing)
 */
long RotAverageNclip4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0, int* sxy1,
    int* sxy2, int* sxy3, int* p, int* otz, int* flag);

/**
 * @brief Coordinate transformation with color calculation (3 vertices)
 *
 * Performs rotation, perspective, and color matrix calculations.
 *
 * @param v0 Pointer to first position vector
 * @param v1 Pointer to second position vector
 * @param v2 Pointer to third position vector
 * @param v3 Pointer to first normal vector
 * @param v4 Pointer to second normal vector
 * @param v5 Pointer to third normal vector
 * @param v6 Pointer to primary color vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param v7 Pointer to first color output
 * @param v8 Pointer to second color output
 * @param v9 Pointer to third color output
 * @param otz Pointer to OTZ value (output)
 * @param flag Pointer to flag (output)
 * @return Outer product
 */
long RotAverageNclipColorCol3(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, SVECTOR* v4,
    SVECTOR* v5, CVECTOR* v6, int* sxy0, int* sxy1, int* sxy2, CVECTOR* v7,
    CVECTOR* v8, CVECTOR* v9, int* otz, int* flag);

/**
 * @brief Coordinate transformation with color (3 vertices, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to first position vector
 * @param v1 Pointer to second position vector
 * @param v2 Pointer to third position vector
 * @param v3 Pointer to first normal vector
 * @param v4 Pointer to second normal vector
 * @param v5 Pointer to third normal vector
 * @param v6 Pointer to primary color vector
 * @return None
 */
long RotAverageNclipColorCol3_nom(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, SVECTOR* v4,
    SVECTOR* v5, CVECTOR* v6);

/**
 * @brief Coordinate transformation with depth cueing (3 vertices)
 *
 * Performs rotation, perspective, color and depth cueing calculations.
 *
 * @param v0 Pointer to first position vector
 * @param v1 Pointer to second position vector
 * @param v2 Pointer to third position vector
 * @param v3 Pointer to first normal vector
 * @param v4 Pointer to second normal vector
 * @param v5 Pointer to third normal vector
 * @param v6 Pointer to primary color vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param v7 Pointer to first color output
 * @param v8 Pointer to second color output
 * @param v9 Pointer to third color output
 * @param otz Pointer to OTZ value (output)
 * @param flag Pointer to flag (output)
 * @return Outer product
 */
long RotAverageNclipColorDpq3(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, SVECTOR* v4,
    SVECTOR* v5, CVECTOR* v6, int* sxy0, int* sxy1, int* sxy2, CVECTOR* v7,
    CVECTOR* v8, CVECTOR* v9, int* otz, int* flag);

/**
 * @brief Coordinate transformation with depth cueing (3 vertices, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to first position vector
 * @param v1 Pointer to second position vector
 * @param v2 Pointer to third position vector
 * @param v3 Pointer to first normal vector
 * @param v4 Pointer to second normal vector
 * @param v5 Pointer to third normal vector
 * @param v6 Pointer to primary color vector
 * @return None
 */
long RotAverageNclipColorDpq3_nom(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, SVECTOR* v4,
    SVECTOR* v5, CVECTOR* v6);

/**
 * @brief Color calculation with depth cueing (1 vertex)
 *
 * Performs rotation, perspective and depth cueing for one point.
 *
 * @param v0 Pointer to position vector
 * @param v1 Pointer to normal vector
 * @param v2 Pointer to primary color vector
 * @param sxy Pointer to screen coordinates (output)
 * @param v3 Pointer to color output
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotColorDpq(
    SVECTOR* v0, SVECTOR* v1, CVECTOR* v2, int* sxy, CVECTOR* v3, int* flag);

/**
 * @brief Color calculation with depth cueing (1 vertex, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to position vector
 * @param v1 Pointer to normal vector
 * @param v2 Pointer to primary color vector
 * @return None
 */
long RotColorDpq_nom(SVECTOR* v0, SVECTOR* v1, CVECTOR* v2);

/**
 * @brief Color calculation with depth cueing (3 vertices)
 *
 * Performs rotation, perspective and depth cueing for three points.
 *
 * @param v0 Pointer to first position vector
 * @param v1 Pointer to second position vector
 * @param v2 Pointer to third position vector
 * @param v3 Pointer to first normal vector
 * @param v4 Pointer to second normal vector
 * @param v5 Pointer to third normal vector
 * @param v6 Pointer to primary color vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param v7 Pointer to first color output
 * @param v8 Pointer to second color output
 * @param v9 Pointer to third color output
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotColorDpq3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                  SVECTOR* v4, SVECTOR* v5, CVECTOR* v6, int* sxy0, int* sxy1,
                  int* sxy2, CVECTOR* v7, CVECTOR* v8, CVECTOR* v9, int* flag);

/**
 * @brief Color calculation with depth cueing (3 vertices, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to first position vector
 * @param v1 Pointer to second position vector
 * @param v2 Pointer to third position vector
 * @param v3 Pointer to first normal vector
 * @param v4 Pointer to second normal vector
 * @param v5 Pointer to third normal vector
 * @param v6 Pointer to primary color vector
 * @return None
 */
long RotColorDpq3_nom(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3,
                      SVECTOR* v4, SVECTOR* v5, CVECTOR* v6);

/**
 * @brief Color calculation with material and depth cueing
 *
 * Performs rotation, perspective, material and depth cueing for one point.
 *
 * @param v0 Pointer to position vector
 * @param v1 Pointer to normal vector
 * @param v2 Pointer to primary color vector
 * @param sxy Pointer to screen coordinates (output)
 * @param v3 Pointer to color output
 * @param matc Material coefficient
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotColorMatDpq(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy,
                    CVECTOR* v3, long matc, int* flag);

/**
 * @brief Alternative rotation matrix (XZY order)
 *
 * Creates rotation matrix with X-Z-Y axis order.
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrixXZY(SVECTOR* r, MATRIX* m);

/**
 * @brief Alternative rotation matrix (YXZ order)
 *
 * Creates rotation matrix with Y-X-Z axis order.
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrixYXZ(SVECTOR* r, MATRIX* m);

/**
 * @brief Alternative rotation matrix (YZX order)
 *
 * Creates rotation matrix with Y-Z-X axis order.
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrixYZX(SVECTOR* r, MATRIX* m);

/**
 * @brief Alternative rotation matrix (ZXY order)
 *
 * Creates rotation matrix with Z-X-Y axis order.
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrixZXY(SVECTOR* r, MATRIX* m);

/**
 * @brief Alternative rotation matrix (ZYX order)
 *
 * Creates rotation matrix with Z-Y-X axis order.
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrixZYX(SVECTOR* r, MATRIX* m);

/**
 * @brief Fast rotation matrix (GTE optimized)
 *
 * Approximately 2x faster than RotMatrix().
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrix_gte(SVECTOR* r, MATRIX* m);

/**
 * @brief Compact rotation matrix
 *
 * Smaller table size, slower speed than RotMatrix().
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrixC(SVECTOR* r, MATRIX* m);

/**
 * @brief Fast YXZ rotation matrix (GTE optimized)
 *
 * Approximately 2x faster than RotMatrixYXZ().
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrixYXZ_gte(SVECTOR* r, MATRIX* m);

/**
 * @brief Fast ZYX rotation matrix (GTE optimized)
 *
 * Approximately 2x faster than RotMatrixZYX().
 *
 * @param r Pointer to rotation vector
 * @param m Pointer to output matrix
 * @return Pointer to matrix
 */
MATRIX* RotMatrixZYX_gte(SVECTOR* r, MATRIX* m);

/**
 * @brief Coordinate transformation (no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to input vector
 */
void RotTrans_nom(SVECTOR* v0);

/**
 * @brief Coordinate and perspective transformation (1 vertex)
 *
 * Performs rotation, translation and perspective transformation.
 *
 * @param v0 Pointer to input vector
 * @param sxy Pointer to screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotTransPers(SVECTOR* v0, int* sxy, int* p, int* flag);

/**
 * @brief Coordinate and perspective transformation (1 vertex, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to input vector
 * @return None
 */
long RotTransPers_nom(SVECTOR* v0);

/**
 * @brief Coordinate and perspective transformation (3 vertices)
 *
 * Performs rotation, translation and perspective transformation for 3 points.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotTransPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0, int* sxy1,
                   int* sxy2, int* p, int* flag);

/**
 * @brief Coordinate and perspective transformation (3 vertices, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @return None
 */
long RotTransPers3_nom(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);

/**
 * @brief Coordinate and perspective transformation (multiple triangles)
 *
 * Executes RotTransPers3() for n triangles.
 *
 * @param v0 Pointer to vertex array (3*n vertices)
 * @param v1 Pointer to screen coordinates array (3*n vertices)
 * @param sz Pointer to SZ values array (n values)
 * @param flag Pointer to flags array (n values)
 * @param n Number of triangles
 */
void RotTransPers3N(
    SVECTOR* v0, DVECTOR* v1, u_short* sz, u_short* flag, long n);

/**
 * @brief Coordinate and perspective transformation (4 vertices)
 *
 * Performs rotation, translation and perspective transformation for 4 points.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param v3 Pointer to fourth input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param sxy3 Pointer to fourth screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long RotTransPers4(
    SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0, int* sxy1,
    int* sxy2, int* sxy3, int* p, int* flag);

/**
 * @brief Coordinate and perspective transformation (4 vertices, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param v3 Pointer to fourth input vector
 * @return Flag value
 */
long RotTransPers4_nom(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3);

/**
 * @brief Coordinate and perspective transformation (N vertices)
 *
 * Executes RotTransPers() for n vertices.
 *
 * @param v0 Pointer to vertex array (n vertices)
 * @param v1 Pointer to screen coordinates array (n vertices)
 * @param sz Pointer to SZ values array (n values)
 * @param p Pointer to interpolation values array (n values)
 * @param flag Pointer to flags array (n values)
 * @param n Number of vertices
 */
void RotTransPersN(
    SVECTOR* v0, DVECTOR* v1, u_short* sz, u_short* p, u_short* flag, long n);

/**
 * @brief Coordinate transformation (short vector output)
 *
 * RotTrans with short vector output: v1 = RTM x v0.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @param flag Pointer to flag (output)
 */
void RotTransSV(SVECTOR* v0, SVECTOR* v1, int* flag);

/**
 * @brief Coordinate transformation with outer product (3 vertices)
 *
 * Performs rotation, perspective and returns outer product.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param otz Pointer to OTZ value (output)
 * @param flag Pointer to flag (output)
 * @return Outer product
 */
long RotNclip3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0, int* sxy1,
               int* sxy2, int* p, int* otz, int* flag);

/**
 * @brief Coordinate transformation with outer product (3 vertices, no output)
 *
 * Results must be retrieved from GTE.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @return None
 */
long RotNclip3_nom(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);

/**
 * @brief Coordinate transformation with outer product (4 vertices)
 *
 * Performs rotation, perspective and returns outer product.
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param v3 Pointer to fourth input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param sxy3 Pointer to fourth screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param otz Pointer to OTZ value (output)
 * @param flag Pointer to flag (output)
 * @return Outer product
 */
long RotNclip4(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, int* sxy0,
               int* sxy1, int* sxy2, int* sxy3, int* p, int* otz, int* flag);

/**
 * @brief Scale matrix (column-wise)
 *
 * Scales matrix m by vector v (column-wise scaling).
 *
 * @param m Pointer to matrix
 * @param v Pointer to scale vector
 * @return Pointer to matrix
 */
MATRIX* ScaleMatrixL(MATRIX* m, VECTOR* v);

/**
 * @brief Set primary color vector and GPU code
 *
 * Sets the primary color vector and GPU code.
 *
 * @param v Pointer to color vector and GPU code
 */
void SetRGBcd(CVECTOR* v);

/**
 * @brief Set fog far parameter
 *
 * Defines Z value at which fog is 100%.
 *
 * @param a Z value (0-65536)
 * @param h Distance between visual point and screen
 */
void SetFogFar(long a, long h);

/**
 * @brief Set fog near and far parameters
 *
 * Defines Z values for fog 0% and 100%.
 *
 * @param a Z value with fog at 0% (0-65536)
 * @param b Z value with fog at 100% (0-65536)
 * @param h Distance between visual point and screen
 */
void SetFogNearFar(long a, long b, long h);

/**
 * @brief Multiply and set rotation matrix
 *
 * Multiplies two matrices and sets as rotation matrix.
 *
 * @param m0 Pointer to first matrix
 * @param m1 Pointer to second matrix
 * @return Pointer to result matrix
 */
MATRIX* SetMulMatrix(MATRIX* m0, MATRIX* m1);

/**
 * @brief Multiply constant rotation matrix
 *
 * Multiplies constant rotation matrix by a matrix.
 *
 * @param m0 Pointer to input matrix
 * @return Pointer to result matrix
 */
MATRIX* SetMulRotMatrix(MATRIX* m0);

/**
 * @brief Square a vector
 *
 * Returns vector with each component squared.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Pointer to output vector
 */
VECTOR* Square0(VECTOR* v0, VECTOR* v1);

/**
 * @brief Square a vector (with 12-bit fraction)
 *
 * Returns vector with each component squared and divided by 4096.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Pointer to output vector
 */
VECTOR* Square12(VECTOR* v0, VECTOR* v1);

/**
 * @brief Square short vector to long vector
 *
 * Returns long vector with each short component squared.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Pointer to output vector
 */
VECTOR* SquareSL0(SVECTOR* v0, VECTOR* v1);

/**
 * @brief Square short vector to long vector (with 12-bit fraction)
 *
 * Returns long vector with each short component squared and divided by 4096.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Pointer to output vector
 */
VECTOR* SquareSL12(SVECTOR* v0, VECTOR* v1);

/**
 * @brief Square short vector to short vector
 *
 * Returns short vector with each component squared.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Pointer to output vector
 */
SVECTOR* SquareSS0(SVECTOR* v0, SVECTOR* v1);

/**
 * @brief Square short vector to short vector (with 12-bit fraction)
 *
 * Returns short vector with each component squared and divided by 4096.
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @return Pointer to output vector
 */
SVECTOR* SquareSS12(SVECTOR* v0, SVECTOR* v1);

/**
 * @brief Subdivide triangle
 *
 * Subdivides a triangle polygon by 2^ndiv.
 *
 * @param p Pointer to 3-vertex polygon
 * @param sp Pointer to subdivision vertex array
 * @param ndiv Number of subdivisions (0=none, 1=2x2, 2=4x4)
 */
void SubPol3(POL3* p, SPOL* sp, int ndiv);

/**
 * @brief Subdivide quadrilateral
 *
 * Subdivides a quadrilateral polygon by 2^ndiv.
 *
 * @param p Pointer to 4-vertex polygon
 * @param sp Pointer to subdivision vertex array
 * @param ndiv Number of subdivisions (0=none, 1=2x2, 2=4x4)
 */
void SubPol4(POL4* p, SPOL* sp, int ndiv);

/**
 * @brief Inverse rotation (translate then rotate, 1 vertex)
 *
 * Performs translation then rotation (inverse of RotTransPers).
 *
 * @param v0 Pointer to input vector
 * @param sxy Pointer to screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long TransRotPers(SVECTOR* v0, int* sxy, int* p, int* flag);

/**
 * @brief Inverse rotation (translate then rotate, 3 vertices)
 *
 * Performs translation then rotation for 3 points (inverse of RotTransPers3).
 *
 * @param v0 Pointer to first input vector
 * @param v1 Pointer to second input vector
 * @param v2 Pointer to third input vector
 * @param sxy0 Pointer to first screen coordinates (output)
 * @param sxy1 Pointer to second screen coordinates (output)
 * @param sxy2 Pointer to third screen coordinates (output)
 * @param p Pointer to interpolation value (output)
 * @param flag Pointer to flag (output)
 * @return OTZ value
 */
long TransRotPers3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, int* sxy0, int* sxy1,
                   int* sxy2, int* p, int* flag);

/**
 * @brief Inverse rotation (translate then rotate)
 *
 * Performs translation then rotation (inverse of RotTrans).
 *
 * @param v0 Pointer to input vector
 * @param v1 Pointer to output vector
 * @param flag Pointer to flag (output)
 */
void TransRot_32(VECTOR* v0, VECTOR* v1, int* flag);

/**
 * @brief Triangle division (flat)
 *
 * Recursive function for division of flat triangles.
 *
 * @param s Pointer to GPU packet buffer
 * @param divp Pointer to division work area
 * @return Updated GPU packet buffer address
 */
u_long* RCpolyF3(void* s, DIVPOLYGON3* divp);

/**
 * @brief Triangle division (flat, textured)
 *
 * Recursive function for division of flat, textured triangles.
 *
 * @param s Pointer to GPU packet buffer
 * @param divp Pointer to division work area
 * @return Updated GPU packet buffer address
 */
u_long* RCpolyFT3(void* s, DIVPOLYGON3* divp);

/**
 * @brief Triangle division (Gouraud)
 *
 * Recursive function for division of Gouraud-shaded triangles.
 *
 * @param s Pointer to GPU packet buffer
 * @param divp Pointer to division work area
 * @return Updated GPU packet buffer address
 */
u_long* RCpolyG3(void* s, DIVPOLYGON3* divp);

/**
 * @brief Triangle division (Gouraud, textured)
 *
 * Recursive function for division of Gouraud-shaded, textured triangles.
 *
 * @param s Pointer to GPU packet buffer
 * @param divp Pointer to division work area
 * @return Updated GPU packet buffer address
 */
u_long* RCpolyGT3(void* s, DIVPOLYGON3* divp);

/**
 * @brief Quadrilateral division (flat)
 *
 * Recursive function for division of flat quadrilaterals.
 *
 * @param s Pointer to GPU packet buffer
 * @param divp Pointer to division work area
 * @return Updated GPU packet buffer address
 */
u_long* RCpolyF4(void* s, DIVPOLYGON4* divp);

/**
 * @brief Quadrilateral division (flat, textured)
 *
 * Recursive function for division of flat, textured quadrilaterals.
 *
 * @param s Pointer to GPU packet buffer
 * @param divp Pointer to division work area
 * @return Updated GPU packet buffer address
 */
u_long* RCpolyFT4(void* s, DIVPOLYGON4* divp);

/**
 * @brief Quadrilateral division (Gouraud)
 *
 * Recursive function for division of Gouraud-shaded quadrilaterals.
 *
 * @param s Pointer to GPU packet buffer
 * @param divp Pointer to division work area
 * @return Updated GPU packet buffer address
 */
u_long* RCpolyG4(void* s, DIVPOLYGON4* divp);

/**
 * @brief Quadrilateral division (Gouraud, textured)
 *
 * Recursive function for division of Gouraud-shaded, textured
 * quadrilaterals.
 *
 * @param s Pointer to GPU packet buffer
 * @param divp Pointer to division work area
 * @return Updated GPU packet buffer address
 */
u_long* RCpolyGT4(void* s, DIVPOLYGON4* divp);

/**
 * @brief Coordinate transformation for mesh
 *
 * Performs coordinate and perspective transformation for m x n mesh vertices.
 *
 * @param Yheight Pointer to vertex Y coordinates
 * @param Vo Pointer to screen coordinates (output)
 * @param sz Pointer to SZ values (output)
 * @param flag Pointer to flags (output)
 * @param Xoffset X offset
 * @param Zoffset Z offset
 * @param m Number of vertices (horizontal)
 * @param n Number of vertices (vertical)
 * @param base Pointer to base address
 */
void RotMeshH(short* Yheight, DVECTOR* Vo, u_short* sz, u_short* flag,
              short Xoffset, short Zoffset, short m, short n, DVECTOR* base);

/* GTE inline assembly macros */

#ifndef __psyz
#define gte_SetRotMatrix(r0)                                                   \
    __asm__ volatile(                                                          \
        "lw	$12, 0( %0 );"                                                     \
        "lw	$13, 4( %0 );"                                                     \
        "ctc2	$12, $0;"                                                        \
        "ctc2	$13, $1;"                                                        \
        "lw	$12, 8( %0 );"                                                     \
        "lw	$13, 12( %0 );"                                                    \
        "lw	$14, 16( %0 );"                                                    \
        "ctc2	$12, $2;"                                                        \
        "ctc2	$13, $3;"                                                        \
        "ctc2	$14, $4"                                                         \
        :                                                                      \
        : "r"(r0)                                                              \
        : "$12", "$13", "$14")

#define gte_SetTransMatrix(r0)                                                 \
    __asm__ volatile(                                                          \
        "lw	$12, 20( %0 );"                                                    \
        "lw	$13, 24( %0 );"                                                    \
        "ctc2	$12, $5;"                                                        \
        "lw	$14, 28( %0 );"                                                    \
        "ctc2	$13, $6;"                                                        \
        "ctc2	$14, $7"                                                         \
        :                                                                      \
        : "r"(r0)                                                              \
        : "$12", "$13", "$14")

#define gte_ldv0(r0)                                                           \
    __asm__ volatile("lwc2	$0, 0( %0 );"                                       \
                     "lwc2	$1, 4( %0 )"                                        \
                     :                                                         \
                     : "r"(r0))

#define gte_ldv01c(r0)                                                         \
    __asm__ volatile("lwc2	$0, 0( %0 );"                                       \
                     "lwc2	$1, 4( %0 );"                                       \
                     "lwc2	$2, 8( %0 );"                                       \
                     "lwc2	$3, 12( %0 )"                                       \
                     :                                                         \
                     : "r"(r0))

#define gte_ldv3c(r0)                                                          \
    __asm__ volatile("lwc2	$0, 0( %0 );"                                       \
                     "lwc2	$1, 4( %0 );"                                       \
                     "lwc2	$2, 8( %0 );"                                       \
                     "lwc2	$3, 12( %0 );"                                      \
                     "lwc2	$4, 16( %0 );"                                      \
                     "lwc2	$5, 20( %0 )"                                       \
                     :                                                         \
                     : "r"(r0))

#define gte_rtps()                                                             \
    __asm__ volatile("nop;"                                                    \
                     "nop;"                                                    \
                     ".word 0x4A180001")

#define gte_rtpt()                                                             \
    __asm__ volatile("nop;"                                                    \
                     "nop;"                                                    \
                     ".word 0x4A280030")

#define gte_nclip()                                                            \
    __asm__ volatile("nop;"                                                    \
                     "nop;"                                                    \
                     ".word 0x4B400006")

#define gte_stsxy2(r0)                                                         \
    __asm__ volatile("swc2	$14, 0( %0 )" : : "r"(r0) : "memory")

#define gte_stsxy01c(r0)                                                       \
    __asm__ volatile("swc2	$12, 0( %0 );"                                      \
                     "swc2	$13, 4( %0 )"                                       \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#define gte_stsxy3_gt3(r0)                                                     \
    __asm__ volatile("swc2	$12, 8( %0 );"                                      \
                     "swc2	$13, 20( %0 );"                                     \
                     "swc2	$14, 32( %0 )"                                      \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "memory")

#define gte_stszotz(r0)                                                        \
    __asm__ volatile(                                                          \
        "mfc2	$12, $19;"                                                       \
        "nop;"                                                                 \
        "sra	$12, $12, 2;"                                                     \
        "sw	$12, 0( %0 )"                                                      \
        :                                                                      \
        : "r"(r0)                                                              \
        : "$12", "memory")

#define gte_ldv3(r0, r1, r2)                                                   \
    __asm__ volatile(                                                          \
        "lwc2	$0, 0( %0 );"                                                    \
        "lwc2	$1, 4( %0 );"                                                    \
        "lwc2	$2, 0( %1 );"                                                    \
        "lwc2	$3, 4( %1 );"                                                    \
        "lwc2	$4, 0( %2 );"                                                    \
        "lwc2	$5, 4( %2 )"                                                     \
        :                                                                      \
        : "r"(r0), "r"(r1), "r"(r2))

#define gte_stopz(r0)                                                          \
    __asm__ volatile("swc2	$24, 0( %0 )" : : "r"(r0) : "memory")

#define gte_stsxy3(r0, r1, r2)                                                 \
    __asm__ volatile(                                                          \
        "swc2	$12, 0( %0 );"                                                   \
        "swc2	$13, 0( %1 );"                                                   \
        "swc2	$14, 0( %2 )"                                                    \
        :                                                                      \
        : "r"(r0), "r"(r1), "r"(r2)                                            \
        : "memory")

#define gte_stsxy(r0)                                                          \
    __asm__ volatile("swc2	$14, 0( %0 )" : : "r"(r0) : "memory")

#define gte_SetGeomScreen(r0) __asm__ volatile("ctc2	%0, $26" : : "r"(r0))

#else // __psyz defined
#define gte_SetGeomScreen SetGeomScreen
#define gte_SetRotMatrix(x) SetRotMatrix((MATRIX*)(x))
#define gte_SetTransMatrix SetTransMatrix
#define gte_SetColorMatrix SetColorMatrix
#define gte_SetTransVector(r0) SetTransVector(r0)
#define gte_rtps() Psyz_GteRtps()
#define gte_stsxy(x) Psyz_GteStsxy((unsigned int*)(x))
#define gte_stszotz(x) Psyz_GteStszotz((unsigned int*)(x))
#define gte_ldv3(x, y, z) Psyz_GteLdv3((SVECTOR*)(x), (SVECTOR*)(y), (SVECTOR*)(z))
#define gte_stsxy3(x, y, z) Psyz_GteStsxy3((unsigned int*)(x), (unsigned int*)(y), (unsigned int*)(z))
#define gte_rtpt() Psyz_GteRtpt()
#define gte_nclip() Psyz_GteNclip()
#define gte_stopz(x) Psyz_GteStopz((int*)(x))
#define gte_ldv0(x) Psyz_GteLdv0((SVECTOR*)(x))
#define gte_ldv01c(x) Psyz_GteLdv01c((SVECTOR*)(x))
#define gte_ldv3c(x) Psyz_GteLdv3c((SVECTOR*)(x))
#define gte_stsxy01c(x) Psyz_GteStsxy01c((unsigned int*)(x))
#define gte_stsxy3_gt3(x) Psyz_GteStsxy3Gt3((POLY_GT3*)(x))
#define gte_avsz3() Psyz_GteAvsz3()
#define gte_dpcs() Psyz_GteDpcs()
#define gte_lcir() Psyz_GteLcir()
#define gte_ldclmv(x) Psyz_GteLdClmv((void*)(x))
#define gte_ldrgb(x) Psyz_GteLdRgb((CVECTOR*)(x))
#define gte_ldtr(x, y, z) Psyz_GteLdTr(x, y, z)
#define gte_ldtx(x) Psyz_GteLdTx(x)
#define gte_ldty(x) Psyz_GteLdTy(x)
#define gte_ldtz(x) Psyz_GteLdTz(x)
#define gte_stclmv(x) Psyz_GteStClmv((void*)(x))
#define gte_strgb(x) Psyz_GteStRgb((CVECTOR*)(x))

/* These are defined in libgte.c and already macro-aliased below, but were never
 * declared here; without prototypes every user compiles them implicitly. */
void Psyz_GteLdClmv(void* p);
void Psyz_GteLdRgb(CVECTOR* v);
void Psyz_GteLdv0(SVECTOR* v);
void Psyz_GteLdv01c(SVECTOR* v);
void Psyz_GteLdv3(SVECTOR* v0, SVECTOR* v1, SVECTOR* v2);
void Psyz_GteLdv3c(SVECTOR* v);
void Psyz_GteNclip(void);
void Psyz_GteRtps(void);
void Psyz_GteRtpt(void);
void Psyz_GteStClmv(void* p);
void Psyz_GteStRgb(CVECTOR* v);
void Psyz_GteStopz(int* out);
void Psyz_GteStsxy(unsigned int* out);
void Psyz_GteStsxy3(unsigned int* out0, unsigned int* out1, unsigned int* out2);
void Psyz_GteStsxy3Gt3(void* polyGte);
void Psyz_GteStsxy01c(unsigned int* out);
void Psyz_GteAvsz3(void);
void Psyz_GteDpcs(void);
void Psyz_GteLcir(void);
void Psyz_GteLdTr(long tx, long ty, long tz);
void Psyz_GteLdTx(long v);
void Psyz_GteLdTy(long v);
void Psyz_GteLdTz(long v);

/* --- macros used by Metal Gear Solid, implemented in libgte.c ------------- */
void Psyz_GteRtir(void);
void Psyz_GteRt(void);
void Psyz_GteSqr0(void);
void Psyz_GteIntpl(void);
void Psyz_GteNcs(void);
void Psyz_GteNctB(void);
void Psyz_GteRtptB(void);
void Psyz_GteLdsv(const void* p);
void Psyz_GteLdopv2SV(const void* p);
void Psyz_GteLdlvl(const void* p);
void Psyz_GteLddp(long v);
void Psyz_GteLdlzc(long v);
void Psyz_GteLdlv0(const void* p);
void Psyz_GteLdsxy3(unsigned int xy0, unsigned int xy1, unsigned int xy2);
void Psyz_GteLdIntpolSv0(const void* p);
void Psyz_GteStsv(void* p);
void Psyz_GteStsz(unsigned int* p);
void Psyz_GteStsz3(unsigned int* p0, unsigned int* p1, unsigned int* p2);
void Psyz_GteStsz3c(unsigned int* p);
void Psyz_GteStsxy0(unsigned int* p);
void Psyz_GteStsxy1(unsigned int* p);
void Psyz_GteStsxy2(unsigned int* p);
void Psyz_GteStsxy01(unsigned int* p0, unsigned int* p1);
void Psyz_GteStsxy3c(unsigned int* p);
void Psyz_GteStlvnl(int* p);
void Psyz_GteStlvnl0(int* p);
void Psyz_GteStlvnl1(int* p);
void Psyz_GteStlvnl2(int* p);
void Psyz_GteStlzc(int* p);
void Psyz_GteStrgb3(unsigned int* p0, unsigned int* p1, unsigned int* p2);
void Psyz_GteReadRotMatrix(MATRIX* m);
void Psyz_GteSetLightMatrix(const MATRIX* m);

#define gte_rtir() Psyz_GteRtir()
#define gte_rt() Psyz_GteRt()
#define gte_sqr0() Psyz_GteSqr0()
#define gte_intpl() Psyz_GteIntpl()
#define gte_ncs() Psyz_GteNcs()
#define gte_nct_b() Psyz_GteNctB()
#define gte_rtpt_b() Psyz_GteRtptB()
#define gte_ldsv(x) Psyz_GteLdsv((const void*)(x))
#define gte_ldopv2SV(x) Psyz_GteLdopv2SV((const void*)(x))
#define gte_ldlvl(x) Psyz_GteLdlvl((const void*)(x))
#define gte_lddp(x) Psyz_GteLddp((long)(x))
#define gte_ldlzc(x) Psyz_GteLdlzc((long)(x))
#define gte_ldlv0(x) Psyz_GteLdlv0((const void*)(x))
#define gte_ldsxy3(a, b, c) Psyz_GteLdsxy3((unsigned int)(a), (unsigned int)(b), (unsigned int)(c))
#define gte_ld_intpol_sv0(x) Psyz_GteLdIntpolSv0((const void*)(x))
#define gte_stsv(x) Psyz_GteStsv((void*)(x))
#define gte_stsz(x) Psyz_GteStsz((unsigned int*)(x))
#define gte_stsz3(a, b, c) Psyz_GteStsz3((unsigned int*)(a), (unsigned int*)(b), (unsigned int*)(c))
#define gte_stsz3c(x) Psyz_GteStsz3c((unsigned int*)(x))
#define gte_stsxy0(x) Psyz_GteStsxy0((unsigned int*)(x))
#define gte_stsxy1(x) Psyz_GteStsxy1((unsigned int*)(x))
#define gte_stsxy2(x) Psyz_GteStsxy2((unsigned int*)(x))
#define gte_stsxy01(a, b) Psyz_GteStsxy01((unsigned int*)(a), (unsigned int*)(b))
#define gte_stsxy3c(x) Psyz_GteStsxy3c((unsigned int*)(x))
#define gte_stlvnl(x) Psyz_GteStlvnl((int*)(x))
#define gte_stlvnl0(x) Psyz_GteStlvnl0((int*)(x))
#define gte_stlvnl1(x) Psyz_GteStlvnl1((int*)(x))
#define gte_stlvnl2(x) Psyz_GteStlvnl2((int*)(x))
#define gte_stlzc(x) Psyz_GteStlzc((int*)(x))
#define gte_strgb3(a, b, c) Psyz_GteStrgb3((unsigned int*)(a), (unsigned int*)(b), (unsigned int*)(c))
#define gte_ReadRotMatrix(x) Psyz_GteReadRotMatrix((MATRIX*)(x))

void Psyz_GteLdIntpolSv0Xz(const void* p);
void Psyz_GteLdIntpolSv1Xz(const void* p);
void Psyz_GteLdv0h(const void* p);
int  Psyz_GtePopColor(void);
void Psyz_GteReadNormal(int* x, int* y, int* z);
void Psyz_GteStbh(void* p);
void Psyz_GteStsvXz(void* p);
#define gte_ld_intpol_sv0_xz(x) Psyz_GteLdIntpolSv0Xz((const void*)(x))
#define gte_ld_intpol_sv1_xz(x) Psyz_GteLdIntpolSv1Xz((const void*)(x))
#define gte_ldv0h(x) Psyz_GteLdv0h((const void*)(x))
#define gte_pop_color(r) ((r) = Psyz_GtePopColor())
#define gte_read_normal(x, y, z) Psyz_GteReadNormal((int*)&(x), (int*)&(y), (int*)&(z))
#define gte_stbh(x) Psyz_GteStbh((void*)(x))
#define gte_stsv_xz(x) Psyz_GteStsvXz((void*)(x))
void Psyz_GteNccs(void);
void Psyz_GteLl(void);
void Psyz_GteLlv0(void);
void Psyz_GteIntplB(void);
void Psyz_GteLdfcdir(const void* p);
#define gte_nop() ((void)0)
#define gte_nccs() Psyz_GteNccs()
#define gte_ll() Psyz_GteLl()
#define gte_llv0() Psyz_GteLlv0()
#define gte_intpl_b() Psyz_GteIntplB()
#define gte_ldfcdir(x) Psyz_GteLdfcdir((const void*)(x))
void Psyz_GteLdfcdir3(long r, long g, long b);
#define gte_ldfcdir3(r, g, b) Psyz_GteLdfcdir3((long)(r), (long)(g), (long)(b))
#define gte_ApplyMatrixSV(m, v0, v1) ApplyMatrixSV((MATRIX*)(m), (SVECTOR*)(v0), (SVECTOR*)(v1))
VECTOR* ApplyMatrixLV(MATRIX* m, VECTOR* v0, VECTOR* v1);
SVECTOR* ApplyMatrixSV(MATRIX* m, SVECTOR* v0, SVECTOR* v1);
VECTOR* ApplyRotMatrixLV(VECTOR* v0, VECTOR* v1);
void Psyz_GteRtv0(void);
void Psyz_GteRtv1(void);
void Psyz_GteRtv2(void);
#define gte_rtv0() Psyz_GteRtv0()
#define gte_rtv1() Psyz_GteRtv1()
#define gte_rtv2() Psyz_GteRtv2()
#define gte_rtv0_b() Psyz_GteRtv0()
#define gte_rtv1_b() Psyz_GteRtv1()
#define gte_rtv2_b() Psyz_GteRtv2()

/* MGS's inline_x.h -- direct vertex-register loads */
void Psyz_GteReadOpz(int* out);
void Psyz_GteLdVXY0(unsigned int xy);
void Psyz_GteLdVXY1(unsigned int xy);
void Psyz_GteLdVXY2(unsigned int xy);
void Psyz_GteLdVZ0(int z);
void Psyz_GteLdVZ1(int z);
void Psyz_GteLdVZ2(int z);
/* the MIPS original is an OUTPUT constraint -- "=r"(r0) -- so the alias must
 * take the variable's ADDRESS; passing its (uninitialized) value as the
 * pointer sent the store into address zero */
#define gte_read_opz(x) Psyz_GteReadOpz((int*)&(x))
#define gte_ldVXY0(x) Psyz_GteLdVXY0((unsigned int)(x))
#define gte_ldVXY1(x) Psyz_GteLdVXY1((unsigned int)(x))
#define gte_ldVXY2(x) Psyz_GteLdVXY2((unsigned int)(x))
#define gte_ldVZ0(z) Psyz_GteLdVZ0((int)(z))
#define gte_ldVZ1(z) Psyz_GteLdVZ1((int)(z))
#define gte_ldVZ2(z) Psyz_GteLdVZ2((int)(z))
#define gte_ldVXYZ0(x, z) (Psyz_GteLdVXY0((unsigned int)(x)), Psyz_GteLdVZ0((int)(z)))
#define gte_ldVXYZ1(x, z) (Psyz_GteLdVXY1((unsigned int)(x)), Psyz_GteLdVZ1((int)(z)))
#define gte_ldVXYZ2(x, z) (Psyz_GteLdVXY2((unsigned int)(x)), Psyz_GteLdVZ2((int)(z)))
#define gte_SetLightMatrix(x) Psyz_GteSetLightMatrix((const MATRIX*)(x))
#define gte_SetBackColor(r, g, b) SetBackColor((long)(r), (long)(g), (long)(b))
#define gte_SetGeomOffset(x, y) SetGeomOffset((long)(x), (long)(y))
#endif

#endif
