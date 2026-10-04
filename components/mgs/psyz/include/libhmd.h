#ifndef LIBHMD_H
#define LIBHMD_H
#include <libgpu.h>

/**
 * @file libhmd.h
 * @brief HMD (Hierarchical Model Data) Library
 *
 * This library provides functions for handling HMD format 3D model data,
 * including coordinate system management, viewpoint control, animation,
 * and primitive rendering. HMD is a hierarchical model data format that
 * supports polygon primitives, shared primitives, images, animation,
 * MIMe (morphing), and ground primitives.
 *
 * Key features:
 * - Hierarchical coordinate system management (GsCOORDUNIT)
 * - Viewpoint control (GsVIEWUNIT, GsRVIEWUNIT)
 * - Animation with sequence control (GsSEQ, GsSEH)
 * - MIMe (morphing) animation support
 * - Primitive driver architecture for extensible rendering
 */

/* Forward declarations for types from other libraries */
#ifndef MATRIX
typedef struct {
    short m[3][3];
    int t[3];
} MATRIX;
#endif

#ifndef SVECTOR
typedef struct {
    short vx, vy, vz;
    short pad;
} SVECTOR;
#endif

#ifndef DVECTOR
typedef struct {
    int vx, vy;
    int vz;
    int pad;
} DVECTOR;
#endif

#ifndef PACKET
typedef struct {
    O_TAG tag;
    unsigned char code[3];
    unsigned char len;
} PACKET;
#endif

#ifndef GsOT
typedef struct {
    O_TAG length;
    unsigned long* org;
} GsOT;
#endif

/* Division macros for GsU_ primitive drivers */
#define GsUNIT_DIV1 (1 << 24) /**< 2x2 divisions */
#define GsUNIT_DIV2 (2 << 24) /**< 4x4 divisions */
#define GsUNIT_DIV3 (3 << 24) /**< 8x8 divisions */
#define GsUNIT_DIV4 (4 << 24) /**< 16x16 divisions */
#define GsUNIT_DIV5 (5 << 24) /**< 32x32 divisions */

/* MIMe type name macros */
#define GsJntAxesMIMe 0x04010010 /**< Axis interpolation joint mime */
#define GsRstJntAxesMIMe                                                       \
    0x04010018                  /**< Axis interpolation joint mime reset       \
                                 */
#define GsJntRPYMIMe 0x04010011 /**< RPY value interpolation joint mime */
#define GsRstJntRPYMIMe                                                        \
    0x04010019                  /**< RPY value interpolation joint mime reset */
#define GsVtxMIMe 0x04010020    /**< Vertex mime */
#define GsNrmMIMe 0x04010021    /**< Normal mime */
#define GsRstVtxMIMe 0x04010028 /**< Vertex mime reset */
#define GsRstNrmMIMe 0x04010029 /**< Normal mime reset */

/* Forward declarations */
typedef struct _GsCOORDUNIT GsCOORDUNIT;
typedef struct GsVIEWUNIT GsVIEWUNIT;
typedef struct GsWORKUNIT GsWORKUNIT;

/* Structures */

/**
 * @brief Primitive driver argument area
 *
 * The common arguments passed to a primitive driver called from GsSortUnit().
 * For high speed operation, use the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
} GsARGUNIT;

/**
 * @brief Animation driver argument area
 *
 * The arguments passed to an animation primitive driver called from
 * GsSortUnit(). In addition to the common arguments in GsARGUNIT, it also
 * contains the start address for each section needed for the primitive header
 * size and animation. For high speed operation, use the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    int header_size;     /**< Size of primitive header used in animation */
    u_long* htop; /**< Start address of interpolation function table section */
    u_long* ctop; /**< Start address of sequence control section */
    u_long* ptop; /**< Start address of parameter section */
} GsARGUNIT_ANIM;

/**
 * @brief HMD ground primitive driver argument area
 *
 * The arguments passed to a ground primitive driver called from GsSortUnit().
 * Use the scratch pad to improve performance.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when sorting OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    u_long* polytop;     /**< Pointer to start of ground POLYGON section */
    u_long* boxtop;      /**< Pointer to start of ground box section */
    u_long* pointtop;    /**< Pointer to start of ground vertex section */
    SVECTOR* nortop;     /**< Pointer to start of NORMAL section */
} GsARGUNIT_GND;

/**
 * @brief HMD ground primitive driver argument area with texture
 *
 * GsARGUNIT_GNDT uses texture, while GsARGUNIT_GND does not.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when sorting OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    u_long* polytop;     /**< Pointer to start of ground POLYGON section */
    u_long* boxtop;      /**< Pointer to start of ground box section */
    u_long* pointtop;    /**< Pointer to start of ground vertex section */
    SVECTOR* nortop;     /**< Pointer to start of NORMAL section */
    u_long* uvtop;       /**< Pointer to start of UV section */
} GsARGUNIT_GNDT;

/**
 * @brief Image primitive driver argument area
 *
 * The arguments passed to an image primitive driver called from GsSortUnit().
 * In addition to the GsARGUNIT members, it includes the pointer to the start
 * of the clut data and texture image pixel data. For high speed operation, use
 * the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    u_long* imagetop;    /**< Pointer to pixel data start */
    u_long* cluttop;     /**< Pointer to clut data start */
} GsARGUNIT_IMAGE;

/**
 * @brief Joint MIMe primitive driver argument area
 *
 * The arguments passed to a joint MIMe primitive driver called from
 * GsSortUnit(). For high-speed operation, use the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    u_long* coord_sect;  /**< Pointer to COORDINATE section top */
    long* mimepr;        /**< Pointer to mime coefficient area */
    u_long mimenum;      /**< Number of mime keys */
    u_short mimeid;      /**< MIMeID */
    u_short reserved;
    u_long* mime_diff_sect; /**< Pointer to MIMe DIFF section */
} GsARGUNIT_JntMIMe;

/**
 * @brief Independent primitive driver argument area
 *
 * The arguments passed to an independent primitive driver called from
 * GsSortUnit(). For high-speed operation, use the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    u_long* primtop;     /**< Pointer to POLYGON section top */
    SVECTOR* vertop;     /**< Pointer to VERTEX section top */
    SVECTOR* nortop;     /**< Pointer to NORMAL section top */
} GsARGUNIT_NORMAL;

/**
 * @brief Joint mime reset primitive driver argument area
 *
 * The arguments passed to a joint MIMe reset primitive driver called from
 * GsSortUnit(). For high-speed operation, use the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    u_long* coord_sect;  /**< Pointer to COORDINATE section top */
    u_short mimeid;      /**< MIMeID */
    u_short reserved;
    u_long* mime_diff_sect; /**< Pointer to MIMe DIFF section */
} GsARGUNIT_RstJntMIMe;

/**
 * @brief Argument area of vertex normal MIMe reset primitive driver
 *
 * The arguments passed to a vertex normal primitive driver called from
 * GsSortUnit. For high-speed operation, use the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    u_short mimeid;      /**< MIMeID */
    u_short reserved;
    u_long* mime_diff_sect; /**< MIMe DIFF section pointer */
    SVECTOR* orgs_vn_sect;  /**< OrgsVN section pointer */
    SVECTOR* vert_sect;     /**< Vertex section pointer */
    SVECTOR* norm_sect;     /**< Normal section pointer */
} GsARGUNIT_RstVNMIMe;

/**
 * @brief Shared primitive driver argument area
 *
 * The arguments passed to a shared primitive driver called from GsSortUnit().
 * For high-speed operation, use the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    u_long* primtop;     /**< Pointer to unused packet area */
    SVECTOR* vertop;     /**< Pointer to POLYGON section top */
    GsWORKUNIT* vertop2; /**< Pointer to shared VERTEX section top */
    SVECTOR* nortop;  /**< Pointer to area storing calculation results of shared
                         vertex section */
    SVECTOR* nortop2; /**< Pointer to shared NORMAL section top / area storing
                         calculation results */
} GsARGUNIT_SHARED;

/**
 * @brief Vertex normal mime primitive driver argument area
 *
 * The arguments passed to the vertex normal mime primitive driver called from
 * GsSortUnit(). For high-speed operation, use the scratch pad.
 */
typedef struct {
    u_long* primp;       /**< Primitive start address */
    GsOT* tagp;          /**< Pointer to current GsOT */
    int shift;           /**< Number of bits to shift when assigning OT */
    int offset;          /**< OT screen coordinate system Z-axis offset */
    PACKET* out_packetp; /**< Pointer to unused packet area */
    long* mimepr;        /**< Pointer to mime coefficient area */
    u_long mimenum;      /**< Number of mime keys */
    u_short mimeid;      /**< MIMeID */
    u_short reserved;
    u_long* mime_diff_sect; /**< MIMe DIFF section pointer */
    SVECTOR* orgs_vn_sect;  /**< OrgsVN section pointer */
    SVECTOR* vert_sect;     /**< Vertex section pointer */
    SVECTOR* norm_sect;     /**< Normal section pointer */
} GsARGUNIT_VNMIMe;

/**
 * @brief Matrix type coordinate system
 *
 * GsCOORDUNIT has parent coordinates and is defined by matrix. workm retains
 * the result of multiplication of matrices performed by GsGetLwUnit() and
 * GsGetLsUnit() in each node using the WORLD coordinates. GsGetLwUnit() and
 * GsGetLsUnit() use flg to omit calculations for a node when they have already
 * been performed. If you change the contents of matrix, you must clear this
 * flag.
 */
struct _GsCOORDUNIT {
    u_long flg;    /**< Flag indicating whether matrix was rewritten */
    MATRIX matrix; /**< Matrix */
    MATRIX workm;  /**< Result of multiplication from this coordinate system to
                      WORLD */
    SVECTOR rot;   /**< Rotation vector for creating matrix */
    struct _GsCOORDUNIT* super; /**< Pointer to parent coordinates */
};

/**
 * @brief HMD viewpoint position (Reference type)
 *
 * GsRVIEWUNIT contains viewpoint information and is set by GsSetRefViewUnit().
 * The viewpoint coordinates in the coordinate system displayed by super are set
 * in vpx, vpy and vpz. The reference point coordinates are set in vrx, vry and
 * vrz. When the z axis is a vector from the viewpoint to the reference point,
 * rz specifies the screen inclination against the z axis in fixed decimal
 * format, with 4096 set to one degree.
 */
typedef struct {
    int vpx, vpy, vpz; /**< Viewpoint coordinates */
    int vrx, vry, vrz; /**< Reference point coordinates */
    int rz;            /**< Viewpoint twist */
    GsCOORDUNIT*
        super; /**< Pointer to coordinate system which sets viewpoint */
} GsRVIEWUNIT;

/**
 * @brief HMD animation sequence header
 *
 * Contains sequence information. Multiple sequences are stored as an array of
 * GsSEH after the sequence pointer. idx provides the index to the sequence
 * control descriptor of the sequence playback start.
 */
typedef struct {
    short idx;  /**< Index within the sequence control section */
    u_char sid; /**< Sequence ID */
    u_char pad; /**< System reservation */
} GsSEH;

/**
 * @brief HMD animation sequence pointer
 *
 * GsSEQ contains the frame update driver internal status. By rewriting this
 * structure during execution, animation can be dynamically controlled. GsSEQ
 * structures in the HMD data can be indicated using GsLinkAnim().
 */
typedef struct {
    u_long rewrite_idx; /**< Upper 8 bits: word offset into animation primitive
                           header; Lower 24 bits: relative offset in section */
    u_short size; /**< Size up to area where next sequence pointer is stored */
    u_short num;  /**< Number of sequences stored in this sequence pointer */
    u_short
        ii; /**< Index of area which stores parameters after interpolation */
    u_short aframe; /**< Frame count of sequence; 0xffff for endless playback */
    u_char sid;     /**< Current status sequence ID */
    char speed;     /**< Controls sequence playback speed (0x10 = standard) */
    u_short srcii;  /**< Retains data which should be specified in ii */
    short rframe;   /**< Interpolation coefficient (12-bit fixed-point) */
    u_short tframe; /**< Distance between key frames (12-bit fixed-point) */
    u_short ci;     /**< Index to parameter which retains source key frames */
    u_short ti;     /**< Parameter index which retains destination keyframes */
    u_short start;  /**< Sequence start index */
    u_char start_sid; /**< Stream ID of sequence to be started */
    u_char pad;       /**< Clears to 0 when key frame is switched */
} GsSEQ;

/**
 * @brief Type storage area
 *
 * This structure is passed to GsScanUnit(). By assigning the subordinate
 * functions corresponding to the type to ptr, the type is overwritten on the
 * subordinate functions pointer. GsSortUnit() calls that subordinate function.
 */
typedef struct {
    u_long type; /**< Primitive type */
    u_long* ptr; /**< Address in HMD data where type is stored */
} GsTYPEUNIT;

/**
 * @brief Three-dimensional object handler for GsSortUnit()
 *
 * GsUNIT exists in every HMD data primitive block and allows movement of
 * three-dimensional models. GsSortUnit() is used to register GsUNIT to the
 * ordering table. coord is the pointer to the primitive block individual
 * coordinate system.
 */
typedef struct {
    GsCOORDUNIT* coord; /**< Pointer to local coordinate system */
    u_long* primtop;    /**< Pointer to primitive block header */
} GsUNIT;

/**
 * @brief HMD viewpoint position (matrix type)
 *
 * This structure sets the viewpoint coordinates used by HMD. It directly
 * specifies the matrix used to change from parent coordinates to viewpoint
 * coordinates. The function used to set GsVIEWUNIT is GsSetViewUnit().
 */
struct GsVIEWUNIT {
    MATRIX view; /**< Matrix used to change from parent coordinates to viewpoint
                    coordinates */
    GsCOORDUNIT*
        super; /**< Pointer to coordinate system which sets viewpoint */
};

/**
 * @brief Calculation result storage area for shared primitive
 *
 * When using a shared primitive, first a perspective conversion of each
 * three-dimensional coordinate value is carried out by its matrix to convert it
 * into a two-dimensional coordinate. This structure is the area which stores
 * the results of that calculation.
 */
struct GsWORKUNIT {
    DVECTOR vec; /**< Area storing 3D coordinate values and 2D coordinate values
                    after perspective conversion */
    short otz;   /**< Area storing OTZ value obtained when vec is requested */
    short p;     /**< Area storing interpolation value obtained when vec is
                    requested */
};

/* Functions */

/**
 * @brief Obtain current HMD header address
 *
 * Returns the current type header address when using GsScanUnit() to scan HMD
 * data.
 *
 * @return The current type header address
 */
u_long* GsGetHeadpUnit(void);

/**
 * @brief Calculate a local screen matrix
 *
 * Calculates a local screen perspective transformation matrix from the
 * GsCOORDUNIT structure pointed to by coord and stores the result in the MATRIX
 * structure pointed to by m. For high speed operation, the function retains the
 * result of calculation at each node of the hierarchical coordinate system.
 *
 * @param coord Local coordinates
 * @param m Matrix
 */
void GsGetLsUnit(GsCOORDUNIT* coord, MATRIX* m);

/**
 * @brief Calculate local world and local screen matrices
 *
 * Calculates both local world coordinates and local screen coordinates. It is
 * faster than calling GsGetLwUnit() and then calling GsGetLsUnit(). This is a
 * quick way of obtaining a local world matrix (lw) to pass to
 * GsSetLightMatrix() to carry out light source calculations.
 *
 * @param coord Pointer to local coordinates
 * @param lw Matrix which stores local world coordinates as the result
 * @param ls Matrix which stores local screen coordinates as the result
 */
void GsGetLwsUnit(GsCOORDUNIT* coord, MATRIX* lw, MATRIX* ls);

/**
 * @brief Calculate local world matrix
 *
 * Calculates a local screen perspective transformation matrix from the
 * GsCOORDUNIT structure pointed to by coord and stores the result in the MATRIX
 * structure pointed to by m. For high speed operation, the function retains the
 * result of calculation at each node of the hierarchical coordinate system.
 *
 * @param coord Local coordinates
 * @param m Matrix
 */
void GsGetLwUnit(GsCOORDUNIT* coord, MATRIX* m);

/**
 * @brief Initialize HMD normal MIMe driver
 *
 * Initializes the HMD library normal MIMe driver. When using the normal MIMe
 * reset driver (GsU_04010029), it is necessary to initialize data using this
 * function.
 *
 * @param primtop Primitive start address
 * @param hp Primitive header start address
 */
void GsInitRstNrmMIMe(u_long* primtop, u_long* hp);

/**
 * @brief Initialize HMD vertex MIMe driver
 *
 * Initializes the HMD library vertex MIMe driver. When using the vertex MIMe
 * reset driver (GsU_04010028), it is necessary to initialize data using this
 * function.
 *
 * @param primtop Primitive start address
 * @param hp Primitive header start address
 */
void GsInitRstVtxMIMe(u_long* primtop, u_long* hp);

/**
 * @brief Link GsSEQ array and HMD data
 *
 * Links a GsSEQ structure, which contains information to control one sequence,
 * to corresponding data in an HMD file. As with GsScanAnim(), GsLinkAnim() must
 * be activated during global SCAN. By using seq, the programmer can control
 * animation playback in real time.
 *
 * @param seq GsSEQ structure array address
 * @param p Address of first type animation section
 * @return The linked number
 */
int GsLinkAnim(GsSEQ* seq, u_long* p);

/**
 * @brief Map COORDINATE within the HMD data to actual address
 *
 * In cases where COORDINATE exists within the HMD data, one type in which INI
 * bits are set up exists in GsTYPEUNIT type when GsScanUnit() is carried out.
 * In such cases, by transferring that type address to GsMapCoordUnit(), the
 * COORDINATE TOP within the data and super within COORDINATE are converted to
 * the actual address.
 *
 * @param base HMD data start address
 * @param p Type of address where INI (init) bits are set up
 * @return COORDINATE section start address
 */
GsCOORDUNIT* GsMapCoordUnit(u_long* base, u_long* p);

/**
 * @brief Map HMD data offset to actual address
 *
 * In HMD data, sections are referred to by pointers. When creating HMD data,
 * the pointers are specified as word offsets from the start of HMD data.
 * GsMapUnit() converts these into actual addresses, so the HMD data can be
 * used. A flag in the HMD header records whether addresses have been mapped, so
 * there are no adverse effects even if GsMapUnit() is called again.
 *
 * @param p HMD data start address
 */
void GsMapUnit(u_long* p);

/**
 * @brief Perform SCAN for HMD animation
 *
 * HMD data scanning is divided into global scan (GsScanUnit) and local scan for
 * each section. GsScanAnim() is a dedicated scan function for HMD animation.
 * Scanning should be carried out after the animation type has been globally
 * scanned.
 *
 * @param p Address where animation section first type exists (0 to continue
 * from previous call)
 * @param ut Pointer to area in which the type and its address are stored
 * @return 0 if type to be scanned does not exist, 1 if scan successful
 */
u_long* GsScanAnim(u_long* p, GsTYPEUNIT* ut);

/**
 * @brief Examine types within HMD data
 *
 * HMD data is divided by type and primitive drivers are called according to
 * type. By overwriting the type by address of the primitive driver, the
 * primitive driver can be called during GsSortUnit(). Initially, set p to the
 * block start address; subsequent calls should set p to 0, and it will scan to
 * the end of the block.
 *
 * @param p Block start address (0 to continue from previous call)
 * @param ut Pointer to area where the type and its address are stored
 * @param ot Pointer to the OT
 * @param scratch Specifies scratch pad address
 * @return 0 if reached end of block, 1 if type exists
 */
int GsScanUnit(u_long* p, GsTYPEUNIT* ut, GsOT* ot, u_long* scratch);

/**
 * @brief Set HMD viewpoint position (High precision)
 *
 * Calculates GsWSMATRIX from viewpoint information. If the viewpoint is not
 * moved, GsWSMATRIX doesn't change. The number of calculation mistakes using
 * this function is smaller than with GsSetRefViewUnit(), but execution time is
 * double. When the GsRVIEWUNIT super member is set to anything besides WORLD,
 * GsSetRefViewLUnit() must be called for each frame.
 *
 * @param pv Viewpoint position information (viewpoint observation point type)
 * @return 0 if viewpoint setting is successful, 1 if unsuccessful
 */
int GsSetRefViewLUnit(GsRVIEWUNIT* pv);

/**
 * @brief Set HMD viewpoint position
 *
 * Calculates GsWSMATRIX from viewpoint information. If the viewpoint isn't
 * moved, GsWSMATRIX doesn't change. When the GsRVIEWUNIT super member is set to
 * anything besides WORLD, GsSetRefViewUnit() must be called for each frame.
 *
 * @param pv Viewpoint position information (viewpoint observation point type)
 * @return 0 if viewpoint setting is successful, 1 if unsuccessful
 */
int GsSetRefViewUnit(GsRVIEWUNIT* pv);

/**
 * @brief Set HMD viewpoint
 *
 * Directly sets GsWSMATRIX. If you use GsSetRefViewUnit() to determine
 * GsWSMATRIX from the viewpoint and the focal point, insufficient precision may
 * cause errors when you move the viewpoint; it is more effective to use
 * GsSetViewUnit(). When the GsVIEWUNIT super member is set to anything besides
 * WORLD, you must call GsSetViewUnit() for each frame.
 *
 * @param pv Viewpoint position information (matrix type)
 * @return 0 if setting is successful, 1 if unsuccessful
 */
int GsSetViewUnit(GsVIEWUNIT* pv);

/**
 * @brief Allocate an object to the ordering table
 *
 * Performs perspective transformation and light source calculation on a three
 * dimensional object handled by GsUNIT. The rendering command is generated in
 * the packet area specified by GsSetWorkBase(). The rendering command generated
 * is then Z-sorted and allocated to the OT displayed by otp.
 *
 * @param objp Pointer to an object
 * @param otp Pointer to the OT
 * @param scratch Specifies scratch pad address
 */
void GsSortUnit(GsUNIT* objp, GsOT* otp, u_short* scratch);

/**
 * @brief GsSortUnit() primitive driver group (Polygon)
 *
 * Polygon primitive drivers for GsSortUnit(). The primitive drivers currently
 * supported are listed in the HMD section in the File Formats manual. When
 * using a primitive driver which performs division, the number of divisions
 * must be set using GsUNIT_DIV1 through GsUNIT_DIV5 macros.
 *
 * @param ap Start address for arguments transfer area
 * @return Start address of the next primitive driver
 */
u_long* GsU_00(GsARGUNIT* ap);

/**
 * @brief HMD animation frame update driver
 *
 * The frame update driver interprets the sequence descriptor and calls the
 * appropriate interpolation function. It carries out sequence jumps, reverses,
 * etc. For high speed operation, specify scratch pad. The frame update driver
 * is linked to the HMD primitive block PRE-PROCESS.
 *
 * @param sp Argument transfer area
 * @return The area in which the start address of the next primitive driver is
 * stored
 */
u_long* GsU_03000000(GsARGUNIT_ANIM* sp);

/**
 * @brief HMD animation interpolation function (alignment, COORDINATE)
 *
 * Interpolation driver for parameter interpolation. For high-speed operation,
 * use the scratch pad. The interpolation coefficient is calculated from the
 * TFRAME and RFRAME stored in the sequence pointer and interpolation is
 * performed. Interpolation algorithms supported: LINEAR, BEZIER, BSPLINE.
 *
 * @param sp Argument transfer area
 * @return 0 after normal interpolation; 1 if next key frame is read
 * unconditionally
 */
int GsU_03000001(GsARGUNIT_ANIM* sp);

/**
 * @brief HMD animation interpolation function (general)
 *
 * Generic interpolation driver for parameter interpolation. The numerical
 * values that are interpolated can be packaged as 3-element vectors of 8-bits,
 * 16-bits, or 32-bits, or as a scalar. For high-speed operation, use the
 * scratch pad. Interpolation algorithms supported: LINEAR, BEZIER, BSPLINE.
 *
 * @param sp Argument transfer area
 * @return 0 after normal interpolation; 1 if next key frame is read
 * unconditionally
 */
int GsU_03010110(GsARGUNIT_ANIM* sp);

/**
 * @brief HMD MIMe driver
 *
 * Driver group for MIMe animation from the HMD library. Although the ap
 * argument is GsARGUNIT type, it is handled internally as a different type
 * depending on the specific MIMe driver being called.
 *
 * @param ap Start address of argument transfer area
 * @return Start address of next primitive driver
 */
u_long* GsU_040100(GsARGUNIT* ap);

#endif
