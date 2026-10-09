#ifndef LIBGS_H
#define LIBGS_H
#include <libgpu.h>
#include <libgte.h>

/**
 * @file libgs.h
 * @brief Graphics System Library
 *
 * This library provides high-level 3D graphics support, built on top of libgpu
 * and libgte. It offers automatic coordinate transformation, lighting
 * calculations, and primitive sorting.
 *
 * Key features:
 * - Automatic GTE (Geometry Transform Engine) coordinate calculations
 * - Light source management (ambient, parallel, point light)
 * - Fog effects and lighting modes
 * - Ordering table (OT) management
 * - Screen coordinate transformation
 * - Packet area management for primitives
 * - World/Screen/Light matrix handling
 */

/* Constants */
#define GsOFSGTE 0
#define GsOFSGPU 4
#define GsINTER 1
#define GsNONINTER 0
#define GsRESET0 0
#define GsRESET3 (3 << 4)

/* Lighting modes */
#define GsLMODE_NORMAL 0     /**< Normal mode */
#define GsLMODE_FOG 1        /**< Fog only mode */
#define GsLMODE_LOFF 2       /**< Light source calculation off */
#define GsLMODE_NORMAL_FOG 3 /**< Normal + fog */

/* Attribute bit masks */
#define GsDOFF (1 << 31) /**< Display off */
#define GsAON (1 << 30)  /**< Semi-transparency on */
#define GsLLMOD (1 << 5) /**< Local lighting mode */
#define GsLOFF (1 << 6)  /**< Light off */

typedef unsigned char PACKET;

/**
 * @brief Ordering table tag
 */
typedef struct {
#ifndef __psyz
    unsigned p : 24;
    unsigned char num : 8;
#else
    O_TAG;
#endif
} GsOT_TAG;

/**
 * @brief Ordering table structure
 *
 * Manages the ordering table used for depth sorting of graphics primitives.
 */
typedef struct {
    unsigned int length; /**< Number of OT entries */
    GsOT_TAG* org;       /**< Pointer to OT buffer */
    unsigned int offset; /**< Z offset value */
    unsigned int point;  /**< Current registration position */
    GsOT_TAG* tag;       /**< Work pointer */
} GsOT;

/**
 * @brief Coordinate parameter format
 *
 * Used to retain information for GsCOORDINATE2 when TOD animation is used.
 */
typedef struct {
    VECTOR scale;   /**< Coordinate scaling information */
    SVECTOR rotate; /**< Coordinate rotation information */
    VECTOR trans;   /**< Coordinate parallel shift information */
} GsCOORD2PARAM;

/**
 * @brief Matrix type coordinate system
 *
 * Has superior coordinates and is defined by the matrix type coord. workm
 * retains the result of multiplication of matrices performed by GsGetLw() and
 * GsGetLs().
 */
typedef struct _GsCOORDINATE2 {
    u_long flg;           /**< Flag indicating whether coord was rewritten */
    MATRIX coord;         /**< Matrix */
    MATRIX workm;         /**< Result of multiplication to WORLD coordinates */
    GsCOORD2PARAM* param; /**< Pointer for scale, rotation, and transferq */
    struct _GsCOORDINATE2* super; /**< Pointer to superior coordinates */
    struct _GsCOORDINATE2* sub;   /**< Not in current use */
} GsCOORDINATE2;

/**
 * @brief Three-dimensional object handler
 *
 * Used to manipulate objects in a three-dimensional model. Use GsLinkObject4()
 * to link to TMD-format model data. Use GsSortObject4() to register in the
 * ordering table.
 */
typedef struct {
    u_long attribute;      /**< Object attribute (32-bit) */
    GsCOORDINATE2* coord2; /**< Pointer to local coordinate system */
    u_long* tmd;           /**< Pointer to model data */
    u_long id;             /**< Reserved by layout tool */
} GsDOBJ2;

/**
 * @brief Three-dimensional object handler for PMD format
 *
 * Used with PMD format model data. Use GsLinkObject3() to link to PMD file
 * model data. Use GsSortObject3() to register in the ordering table.
 */
typedef struct {
    u_long attribute;      /**< Object attribute (32-bit) */
    GsCOORDINATE2* coord2; /**< Pointer to local coordinate system */
    u_long* pmd;           /**< Pointer to model data (PMD format) */
    u_long* base;          /**< Pointer to object base address */
    u_long* sv;            /**< Pointer to shared vertex base address */
    u_long id;             /**< Reserved by layout tool */
} GsDOBJ3;

/**
 * @brief Three-dimensional object handler for use with GsSortObject5()
 *
 * Use GsLinkObject5() to link to TMD file model data. Use GsSortObject5() to
 * register in the ordering table. Supports preset packets.
 */
typedef struct {
    u_long attribute;      /**< Object attribute (32-bit) */
    GsCOORDINATE2* coord2; /**< Pointer to local coordinate system */
    u_long* tmd;           /**< Pointer to model data */
    u_long* packet;        /**< Pointer to preset packet area */
    u_long id;             /**< Reserved by layout tool */
} GsDOBJ5;

/**
 * @brief Cells constituting BG
 *
 * A rectangular array of GsCELL structures describes individual cells that fit
 * together to create a BG.
 */
typedef struct {
    u_short u;     /**< Offset (X-direction) within the page */
    u_short v;     /**< Offset (Y-direction) within the page */
    u_short cba;   /**< CLUT ID */
    u_short flag;  /**< Drawing options (flip flags) */
    u_short tpage; /**< Texture page number */
} GsCELL;

/**
 * @brief Map structure for BG
 *
 * Describes the mapping of cells for background surfaces.
 */
typedef struct {
    u_char cellw;   /**< Cell width */
    u_char cellh;   /**< Cell height */
    u_short ncellw; /**< Number of cells in width */
    u_short ncellh; /**< Number of cells in height */
    GsCELL* base;   /**< Pointer to cell array */
    u_short* index; /**< Pointer to map index */
} GsMAP;

/**
 * @brief BG (background surface) handler
 *
 * A BG is drawn as a large rectangle based on GsMAP data on a combination of
 * small rectangles defined by GsCELL data. Use GsSortBg() to register in the
 * ordering table.
 */
typedef struct {
    u_long attribute;       /**< Attribute */
    short x, y;             /**< Top left point display position */
    short w, h;             /**< BG display size */
    short scrollx, scrolly; /**< X and Y scroll values */
    u_char r, g, b;         /**< Display brightness (128 = normal) */
    GsMAP* map;             /**< Pointer to map data */
    short mx, my; /**< Rotation and enlargement central point coordinates */
    short scalex, scaley; /**< Scale values in X and Y directions */
    int rotate;           /**< Rotation angle (4096 = 1 degree) */
} GsBG;

/**
 * @brief Rectangle handler
 *
 * Used to draw a rectangle in a single color. Use GsSortBoxFill() to register
 * in the ordering table.
 */
typedef struct {
    u_long attribute; /**< Attribute */
    short x, y;       /**< Display position (top left point) */
    u_short w, h;     /**< Size of rectangle (width, height) */
    u_char r, g, b;   /**< Drawing color */
} GsBOXF;

/**
 * @brief Line handler
 *
 * Used to draw lines. Use GsSortLine() to register in the ordering table.
 */
typedef struct {
    u_long attribute; /**< Attribute */
    short x0, y0;     /**< Start point */
    short x1, y1;     /**< End point */
    u_char r, g, b;   /**< Drawing color */
} GsLINE;

/**
 * @brief Gouraud-shaded line handler
 *
 * Used to draw Gouraud-shaded lines. Use GsSortGLine() to register in the
 * ordering table.
 */
typedef struct {
    u_long attribute;  /**< Attribute */
    short x0, y0;      /**< Start point */
    short x1, y1;      /**< End point */
    u_char r0, g0, b0; /**< Start point color */
    u_char r1, g1, b1; /**< End point color */
} GsGLINE;

/**
 * @brief Sprite handler
 *
 * Used to draw sprites. Use GsSortSprite() to register in the ordering table.
 */
typedef struct {
    u_long attribute;     /**< Attribute */
    short x, y;           /**< Display position */
    short w, h;           /**< Sprite size */
    u_short tpage;        /**< Texture page ID */
    u_char u, v;          /**< Texture coordinates */
    short cx, cy;         /**< Rotation center */
    u_char r, g, b;       /**< Color */
    short mx, my;         /**< Enlargement center */
    short scalex, scaley; /**< Scale values */
    int rotate;           /**< Rotation angle */
} GsSPRITE;

/**
 * @brief Image handler
 *
 * Used to draw images.
 */
typedef struct {
    short x, y;     /**< Display position */
    short w, h;     /**< Image size */
    u_short* pixel; /**< Pointer to pixel data */
    u_short* clut;  /**< Pointer to CLUT data */
} GsIMAGE;

/**
 * @brief Fog parameter
 *
 * Used to set fog parameters with GsSetFogParam().
 */
typedef struct {
    int dqa;              /**< Fog coefficient A */
    int dqb;              /**< Fog coefficient B */
    u_char rfc, gfc, bfc; /**< Fog color (R, G, B) */
} GsFOGPARAM;

/**
 * @brief Flat light source
 *
 * Used to set flat light source with GsSetFlatLight().
 */
typedef struct {
    SVECTOR direction; /**< Light direction vector */
    u_char r, g, b;    /**< Light color */
} GsF_LIGHT;

/**
 * @brief Viewpoint position (reference type)
 *
 * Contains viewpoint information. Set with GsSetRefView2(). The viewpoint
 * coordinates in the coordinate system displayed by super are set in vpx, vpy,
 * vpz. The reference point coordinates are set in vrx, vry, vrz.
 */
typedef struct {
    int vpx, vpy, vpz; /**< Viewpoint coordinates */
    int vrx, vry, vrz; /**< Reference point coordinates */
    int rz;            /**< Viewpoint twist */
    GsCOORDINATE2*
        super; /**< Pointer to coordinate system which sets viewpoint */
} GsRVIEW2;

/**
 * @brief Viewpoint position (matrix type)
 *
 * Sets the viewpoint coordinates used by libgs. Directly specifies the matrix
 * used to change from parent coordinates to viewpoint coordinates. Set with
 * GsSetView2().
 */
typedef struct {
    MATRIX view; /**< Matrix from parent coordinates to viewpoint coordinates */
    GsCOORDINATE2*
        super; /**< Pointer to coordinate system which sets viewpoint */
} GsVIEW2;

/**
 * @brief Object table for TOD
 *
 * Used to manage multiple objects for TOD animation.
 */
typedef struct {
    u_long n;      /**< Number of objects */
    GsDOBJ2** top; /**< Pointer to object array */
} GsOBJTABLE2;

/* Function declarations */

/**
 * @brief Clear V-BLANK count
 *
 * Clears the V-BLANK counter to zero.
 */
void GsClearVcount(void);

/**
 * @brief Get V-BLANK count
 *
 * Returns the number of V-BLANKs since the last call to GsClearVcount() or
 * GsInitVcount().
 *
 * @return V-BLANK count
 */
long GsGetVcount(void);

/**
 * @brief Initialize V-BLANK count
 *
 * Initializes the V-BLANK counter and starts counting.
 */
void GsInitVcount(void);

/**
 * @brief Clear ordering table
 *
 * Clears the ordering table and sets the offset and point values.
 *
 * @param offset Z offset value
 * @param point Current registration position
 * @param otp Pointer to ordering table
 */
void GsClearOt(unsigned short offset, unsigned short point, GsOT* otp);

/**
 * @brief Initialize graphics system
 *
 * Initializes the graphics system with the specified parameters.
 *
 * @param x Display area X position
 * @param y Display area Y position
 * @param intmode Interlace mode (GsINTER or GsNONINTER)
 * @param dith Dithering flag (0: off, 1: on)
 * @param varmmode Video mode (GsRESET0 or GsRESET3)
 */
void GsInitGraph(unsigned short x, unsigned short y, unsigned short intmode,
                 unsigned short dith, unsigned short varmmode);

/**
 * @brief Define double buffer
 *
 * Sets up double buffering with the specified coordinates for two buffers.
 *
 * @param x0 Buffer 0 X position
 * @param y0 Buffer 0 Y position
 * @param x1 Buffer 1 X position
 * @param y1 Buffer 1 Y position
 */
void GsDefDispBuff(
    unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1);

/**
 * @brief Get active buffer
 *
 * Returns which buffer is currently active for drawing.
 *
 * @return 0 or 1 indicating active buffer
 */
int GsGetActiveBuff(void);

/**
 * @brief Set work base address
 *
 * Sets the base address for the packet work area.
 *
 * @param outpacketp Pointer to packet buffer
 */
void GsSetWorkBase(PACKET* outpacketp);

/**
 * @brief Swap display buffer
 *
 * Swaps the display and drawing buffers for double buffering.
 */
void GsSwapDispBuff(void);

/**
 * @brief Sort clear primitive
 *
 * Registers a clear screen primitive to the ordering table.
 *
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param otp Pointer to ordering table
 */
void GsSortClear(unsigned char r, unsigned char g, unsigned char b, GsOT* otp);

/**
 * @brief Draw ordering table
 *
 * Draws all primitives in the ordering table.
 *
 * @param ot Pointer to ordering table
 */
void GsDrawOt(GsOT* ot);

/**
 * @brief Set drawing buffer clip
 *
 * Sets the drawing clip area to match the current drawing buffer.
 */
void GsSetDrawBuffClip(void);

/**
 * @brief Set drawing buffer offset
 *
 * Sets the drawing offset to match the current drawing buffer.
 */
void GsSetDrawBuffOffset(void);

/**
 * @brief Initialize 3D graphics
 *
 * Initializes the 3D graphics system.
 */
void GsInit3D(void);

/**
 * @brief Initialize coordinate system
 *
 * Initializes a GsCOORDINATE2 structure.
 *
 * @param coord Pointer to coordinate system to initialize
 */
void GsInitCoordinate2(GsCOORDINATE2* coord);

/**
 * @brief Calculate local screen matrix
 *
 * Calculates the local screen perspective transformation matrix.
 *
 * @param coord Local coordinates
 * @param m Output matrix
 */
void GsGetLs(GsCOORDINATE2* coord, MATRIX* m);

/**
 * @brief Calculate local world matrix
 *
 * Calculates the local world matrix from the coordinate system.
 *
 * @param coord Local coordinates
 * @param m Output matrix
 */
void GsGetLw(GsCOORDINATE2* coord, MATRIX* m);

/**
 * @brief Calculate local world and screen matrices
 *
 * Calculates both local world and local screen matrices. Faster than calling
 * GsGetLw() and GsGetLs() separately.
 *
 * @param coord Pointer to local coordinates
 * @param lw Output local world matrix
 * @param ls Output local screen matrix
 */
void GsGetLws(GsCOORDINATE2* coord, MATRIX* lw, MATRIX* ls);

/**
 * @brief Set viewpoint (reference type)
 *
 * Calculates GsWSMATRIX from viewpoint information.
 *
 * @param pv Viewpoint position information
 */
void GsSetRefView2(GsRVIEW2* pv);

/**
 * @brief Set viewpoint (reference type, high precision)
 *
 * High precision version of GsSetRefView2().
 *
 * @param pv Viewpoint position information
 */
void GsSetRefView2L(GsRVIEW2* pv);

/**
 * @brief Set viewpoint (matrix type)
 *
 * Directly sets GsWSMATRIX from a matrix.
 *
 * @param pv Viewpoint position information
 */
void GsSetView2(GsVIEW2* pv);

/**
 * @brief Link object to TMD data (version 4)
 *
 * Links a GsDOBJ2 structure to TMD-format model data.
 *
 * @param objnum Number of objects
 * @param base Pointer to TMD data
 * @param objp Pointer to object handler array
 */
void GsLinkObject4(u_long objnum, u_long* base, GsDOBJ2* objp);

/**
 * @brief Link object to PMD data (version 3)
 *
 * Links a GsDOBJ3 structure to PMD-format model data.
 *
 * @param objnum Number of objects
 * @param base Pointer to PMD data
 * @param objp Pointer to object handler array
 */
void GsLinkObject3(u_long objnum, u_long* base, GsDOBJ3* objp);

/**
 * @brief Link object to TMD data (version 5)
 *
 * Links a GsDOBJ5 structure to TMD-format model data with preset packet
 * support.
 *
 * @param objnum Number of objects
 * @param base Pointer to TMD data
 * @param objp Pointer to object handler array
 */
void GsLinkObject5(u_long objnum, u_long* base, GsDOBJ5* objp);

/**
 * @brief Sort 3D object to OT (version 4)
 *
 * Performs perspective transformation and light source calculation on a
 * GsDOBJ2 object and registers it to the ordering table.
 *
 * @param objp Pointer to object handler
 * @param otp Pointer to ordering table
 */
void GsSortObject4(GsDOBJ2* objp, GsOT* otp);

/**
 * @brief Sort 3D object to OT (version 3)
 *
 * Sorts a GsDOBJ3 object to the ordering table.
 *
 * @param objp Pointer to object handler
 * @param otp Pointer to ordering table
 */
void GsSortObject3(GsDOBJ3* objp, GsOT* otp);

/**
 * @brief Sort 3D object to OT (version 5)
 *
 * Sorts a GsDOBJ5 object (with preset packets) to the ordering table.
 *
 * @param objp Pointer to object handler
 * @param otp Pointer to ordering table
 */
void GsSortObject5(GsDOBJ5* objp, GsOT* otp);

/**
 * @brief Sort background to OT
 *
 * Registers a GsBG background to the ordering table.
 *
 * @param bg Pointer to background handler
 * @param otp Pointer to ordering table
 */
void GsSortBg(GsBG* bg, GsOT* otp);

/**
 * @brief Sort box fill to OT
 *
 * Registers a GsBOXF rectangle to the ordering table.
 *
 * @param boxf Pointer to rectangle handler
 * @param otp Pointer to ordering table
 */
void GsSortBoxFill(GsBOXF* boxf, GsOT* otp);

/**
 * @brief Sort line to OT
 *
 * Registers a GsLINE to the ordering table.
 *
 * @param line Pointer to line handler
 * @param otp Pointer to ordering table
 */
void GsSortLine(GsLINE* line, GsOT* otp);

/**
 * @brief Sort Gouraud line to OT
 *
 * Registers a GsGLINE to the ordering table.
 *
 * @param gline Pointer to Gouraud line handler
 * @param otp Pointer to ordering table
 */
void GsSortGLine(GsGLINE* gline, GsOT* otp);

/**
 * @brief Sort sprite to OT
 *
 * Registers a GsSPRITE to the ordering table.
 *
 * @param sprite Pointer to sprite handler
 * @param otp Pointer to ordering table
 */
void GsSortSprite(GsSPRITE* sprite, GsOT* otp);

/**
 * @brief Set light matrix
 *
 * Sets the light matrix for light source calculations.
 *
 * @param mp Pointer to light matrix
 */
void GsSetLightMatrix(MATRIX* mp);

/**
 * @brief Set ambient light
 *
 * Sets the ambient light color.
 *
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 */
void GsSetAmbient(long r, long g, long b);

/**
 * @brief Set flat light
 *
 * Sets a flat (directional) light source.
 *
 * @param id Light source ID (0-2)
 * @param light Pointer to light source data
 */
void GsSetFlatLight(int id, GsF_LIGHT* light);

/**
 * @brief Set fog parameter
 *
 * Sets fog parameters for fog rendering.
 *
 * @param fogp Pointer to fog parameters
 */
void GsSetFogParam(GsFOGPARAM* fogp);

/**
 * @brief Set projection distance
 *
 * Sets the distance from the viewpoint to the projection plane.
 *
 * @param h Projection distance
 */
void GsSetProjection(long h);

/**
 * @brief Set screen offset
 *
 * Sets the screen offset for 2D drawing.
 *
 * @param x X offset
 * @param y Y offset
 */
void GsSetOffset(long x, long y);

/**
 * @brief Set screen origin
 *
 * Sets the screen origin for coordinate transformations.
 *
 * @param x X origin
 * @param y Y origin
 */
void GsSetOrign(long x, long y);

/**
 * @brief Set clip region
 *
 * Sets the 3D clipping region.
 *
 * @param clip Pointer to clip parameters
 */
void GsSetClip(RECT* clip);

/**
 * @brief Map modeling data
 *
 * Maps TMD modeling data offsets to actual memory addresses.
 *
 * @param base Pointer to TMD data
 */
void GsMapModelingData(u_long* base);

/**
 * @brief Get work base address
 *
 * Gets the current packet work area base address.
 *
 * @return Pointer to packet buffer
 */
PACKET* GsGetWorkBase(void);

/**
 * @brief Multiply coordinate matrices
 *
 * Multiplies coordinate system matrices.
 *
 * @param coord Pointer to coordinate system
 */
void GsMulCoord2(GsCOORDINATE2* coord);

#endif
