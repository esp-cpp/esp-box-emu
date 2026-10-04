#include "libdg.h"

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "common.h"
#include "libgv/libgv.h"

/*** data *******************************************************/

typedef struct _DG_PRIM_INFO {
    unsigned char psize;
    unsigned char verts;
    unsigned char voffset;
    unsigned char vstep;
} DG_PRIM_INFO;

// psize, verts, voffset, vstep
#ifdef __psyz
/* These byte layouts describe the PACKED prims of the console: 4-byte tag,
 * first vertex at +8. PSY-Z's prims carry an 8-byte tag+len header, so every
 * prim is 4 bytes bigger and every vertex sits 4 bytes later. Left as the
 * console values, each transformed vertex landed on the prim's COMMAND word
 * and the whole 3D world reached the GPU as code-0 garbage. The inter-vertex
 * stride does not change. */
STATIC DG_PRIM_INFO DG_PrimInfos[DG_PRIM_MAX] = {
    { 20, 2, 12,  4 }, // DG_PRIM_LINE_F2
    { 28, 3, 12,  4 }, // DG_PRIM_LINE_F3
    { 32, 4, 12,  4 }, // DG_PRIM_LINE_F4
    { 24, 2, 12,  8 }, // DG_PRIM_LINE_G2
    { 36, 3, 12,  8 }, // DG_PRIM_LINE_G3
    { 44, 4, 12,  8 }, // DG_PRIM_LINE_G4
    { 24, 1, 12,  0 }, // DG_PRIM_SPRT
    { 20, 1, 12,  0 }, // DG_PRIM_SPRT_8
    { 20, 1, 12,  0 }, // DG_PRIM_SPRT_16
    { 20, 1, 12,  0 }, // DG_PRIM_TILE
    { 16, 1, 12,  0 }, // DG_PRIM_TILE_1
    { 16, 1, 12,  0 }, // DG_PRIM_TILE_8
    { 16, 1, 12,  0 }, // DG_PRIM_TILE_16
    { 24, 3, 12,  4 }, // DG_PRIM_POLY_F3
    { 28, 4, 12,  4 }, // DG_PRIM_POLY_F4
    { 32, 3, 12,  8 }, // DG_PRIM_POLY_G3
    { 40, 4, 12,  8 }, // DG_PRIM_POLY_G4
    { 36, 3, 12,  8 }, // DG_PRIM_POLY_FT3
    { 44, 4, 12,  8 }, // DG_PRIM_POLY_FT4
    { 44, 3, 12, 12 }, // DG_PRIM_POLY_GT3
    { 56, 4, 12, 12 }, // DG_PRIM_POLY_GT4
    { 44, 2, 12,  8 }, // DG_PRIM_LINE_FT2
    { 56, 2, 12, 12 }, // DG_PRIM_LINE_GT2
    { 16, 1, 12,  0 }  // DG_PRIM_FREE
};
#else
STATIC DG_PRIM_INFO DG_PrimInfos[DG_PRIM_MAX] = {
    { 16, 2, 8,  4 }, // DG_PRIM_LINE_F2
    { 24, 3, 8,  4 }, // DG_PRIM_LINE_F3
    { 28, 4, 8,  4 }, // DG_PRIM_LINE_F4
    { 20, 2, 8,  8 }, // DG_PRIM_LINE_G2
    { 32, 3, 8,  8 }, // DG_PRIM_LINE_G3
    { 40, 4, 8,  8 }, // DG_PRIM_LINE_G4
    { 20, 1, 8,  0 }, // DG_PRIM_SPRT
    { 16, 1, 8,  0 }, // DG_PRIM_SPRT_8
    { 16, 1, 8,  0 }, // DG_PRIM_SPRT_16
    { 16, 1, 8,  0 }, // DG_PRIM_TILE
    { 12, 1, 8,  0 }, // DG_PRIM_TILE_1
    { 12, 1, 8,  0 }, // DG_PRIM_TILE_8
    { 12, 1, 8,  0 }, // DG_PRIM_TILE_16
    { 20, 3, 8,  4 }, // DG_PRIM_POLY_F3
    { 24, 4, 8,  4 }, // DG_PRIM_POLY_F4
    { 28, 3, 8,  8 }, // DG_PRIM_POLY_G3
    { 36, 4, 8,  8 }, // DG_PRIM_POLY_G4
    { 32, 3, 8,  8 }, // DG_PRIM_POLY_FT3
    { 40, 4, 8,  8 }, // DG_PRIM_POLY_FT4
    { 40, 3, 8, 12 }, // DG_PRIM_POLY_GT3
    { 52, 4, 8, 12 }, // DG_PRIM_POLY_GT4
    { 40, 2, 8,  8 }, // DG_PRIM_LINE_FT2
    { 52, 2, 8, 12 }, // DG_PRIM_LINE_GT2
    { 12, 1, 8,  0 }  // DG_PRIM_FREE
};
#endif

MATRIX DG_ZeroMatrix = {
    {{0x1000, 0x0000, 0x0000},
     {0x0000, 0x1000, 0x0000},
     {0x0000, 0x0000, 0x1000}},
    {0, 0, 0}};

SVECTOR DG_ZeroVector = { 0, 0, 0, 0 };

/****************************************************************/

// Number of vertices to process at once
// ALIGN_DOWN( SCRATCHPAD_SIZE / sizeof(SVECTOR), 3 )
#define BATCH_SIZE (126)

void DG_PrimStart( void )
{
    /* do nothing */
}

STATIC void DG_AdjustLaserPrim( DVECTOR *xy0, DVECTOR *xy1, DVECTOR *xy2, DVECTOR *xy3 )
{
    int x0 = xy0->vx;
    int x1 = xy1->vx;
    int y0 = xy0->vy;
    int y1 = xy1->vy;

    int dx = x1 - x0;
    int dy = y1 - y0;

    if ( dx < 0 )
    {
        dx = -dx;
    }

    if ( dy < 0 )
    {
        dy = -dy;
    }

    if ( dy >= dx )
    {
        xy2->vx = x0 + 2;
        xy2->vy = y0;
        xy3->vx = x1 + 2;
        xy3->vy = y1;
    }
    else
    {
        xy2->vy = y0 + 1;
        xy2->vx = x0;
        xy3->vx = x1;
        xy3->vy = y1 + 1;
    }
}

STATIC void DG_AdjustLaserPrims( DG_PRIM *prim, int type )
{
    POLY_FT4 *ft2;
    POLY_GT4 *gt2;
    int       i;

    if ( type == DG_PRIM_LINE_FT2 )
    {
        ft2 = (POLY_FT4 *)prim->packs[GV_Clock];

        for (i = prim->prim_count ; i > 0; --i )
        {
            DG_AdjustLaserPrim((DVECTOR *)&ft2->x0, (DVECTOR *)&ft2->x1, (DVECTOR *)&ft2->x2, (DVECTOR *)&ft2->x3);
            ft2++;
        }
    }
    else // type == DG_PRIM_LINE_GT2
    {
        gt2 = (POLY_GT4 *)prim->packs[GV_Clock];

        for (i = prim->prim_count ; i > 0; --i )
        {
            DG_AdjustLaserPrim((DVECTOR *)&gt2->x0, (DVECTOR *)&gt2->x1, (DVECTOR *)&gt2->x2, (DVECTOR *)&gt2->x3);
            gt2++;
        }
    }
}

// process vecs in spad
STATIC SVECTOR *_RotTransPers( SVECTOR *in, int n_verts )
{
    SVECTOR *out;

    out = (SVECTOR *)getScratchAddr(0);
    for ( --n_verts ; n_verts >= 0 ; n_verts-- )
    {
        gte_ldv3c( in );
        gte_rtpt();
        gte_stsxy3( &out[0].vx, &out[1].vx, &out[2].vx );
        gte_stsz3(  &out[0].vz, &out[1].vz, &out[2].vz );
        in += 3;
        out += 3;
    }

    return in;
}

STATIC char *_MakeXYZ( DG_PRIM *prim, char *out, int n_prims )
{
    int      psize;
    int      verts;
    int      voffset;
    int      vstep;
    SVECTOR *in;
    char    *vert;

    psize = prim->psize;
    verts = prim->verts;
    voffset = prim->voffset;
    vstep = prim->vstep;

    in = (SVECTOR *)getScratchAddr(0);

    for ( n_prims--; n_prims >= 0; n_prims-- )
    {
        vert = out + voffset;

        SCOPYL(&in->vz, out);

        switch( verts )
        {
        case 4:
            LCOPY(in, vert);
            vert += vstep;
            in++;

        case 3:
            LCOPY(in, vert);
            vert += vstep;
            in++;

        case 2:
            LCOPY(in, vert);
            vert += vstep;
            in++;

        case 1:
            LCOPY(in, vert);
            in++;
        }

        out += psize;
    }

    return out;
}

STATIC void MakePrims( DG_PRIM *prim )
{
    int      n_verts;
    int      vert_batch;
    int      prim_batch;
    SVECTOR *verts;
    char    *packs;
    int      n_prims;

    n_verts = prim->verts;

    if ( n_verts == 4 )
    {
        vert_batch = 40;
        prim_batch = 30;
    }
    else
    {
        vert_batch = 42;
        prim_batch = BATCH_SIZE / n_verts;
    }

    verts = prim->pos;
    packs = (char *)prim->packs[ GV_Clock ];

    for ( n_prims = prim->prim_count; n_prims > prim_batch; n_prims -= prim_batch )
    {
        verts = _RotTransPers( verts, vert_batch );
        packs = _MakeXYZ( prim, packs, prim_batch );
    }

    _RotTransPers( verts, ( n_prims * n_verts + 2 ) / 3 );
    _MakeXYZ( prim, packs, n_prims );
}

STATIC char *_MakeXYZOneface( DG_PRIM *prim, char *out, int n_prims )
{
    int      psize;
    int      verts;
    int      voffset;
    int      vstep;
    SVECTOR *in;
    char    *vert;
    int     *area;

    psize = prim->psize;
    verts = prim->verts;
    voffset = prim->voffset;
    vstep = prim->vstep;

    in = (SVECTOR *)getScratchAddr(0);

    for ( n_prims--; n_prims >= 0; n_prims-- )
    {
        area = (int *)getScratchAddr(252);
        gte_NormalClip( *(int *)&in[0].vx, *(int *)&in[1].vx, *(int *)&in[2].vx, getScratchAddr(252) );

        vert = out + voffset;

        if ( *area <= 0 )
        {
            SSTOREL(0, out);
            in += verts;
        }
        else
        {
            SCOPYL(&in->vz, out);

            switch( verts )
            {
            case 4:
                gte_stsxy0( vert );
                vert += vstep;
                in++;

            case 3:
                gte_stsxy1( vert );
                vert += vstep;
                in++;

            case 2:
                gte_stsxy2( vert );
                vert += vstep;
                in++;

            case 1:
                LCOPY(in, vert);
                in++;
            }
        }

        out += psize;
    }

    return out;
}

STATIC void MakePrimsOneface( DG_PRIM *prim )
{
    int      n_verts;
    int      vert_batch;
    int      prim_batch;
    SVECTOR *verts;
    char    *packs;
    int      n_prims;

    n_verts = prim->verts;

    if ( n_verts == 4 )
    {
        vert_batch = 40;
        prim_batch = 30;
    }
    else
    {
        vert_batch = 42;
        prim_batch = BATCH_SIZE / n_verts;
    }

    verts = prim->pos;
    packs = (char *)prim->packs[ GV_Clock ];

    for ( n_prims = prim->prim_count; n_prims > prim_batch; n_prims -= prim_batch )
    {
        verts = _RotTransPers( verts, vert_batch );
        packs = _MakeXYZOneface( prim, packs, prim_batch );
    }

    _RotTransPers( verts, ( n_prims * n_verts + 2 ) / 3 );
    _MakeXYZOneface( prim, packs, n_prims );
}

STATIC char *_MakeXYZRectangleSingle( DG_PRIM *prim, char *out, int n_prims )
{
    RECT    *rect;
    int      psize;
    int      x, y;
    SVECTOR *in;

    rect = prim->rect;
    psize = prim->psize;

    x = rect->x;
    y = rect->y;

    in = (SVECTOR *)getScratchAddr(0);

    for ( n_prims--; n_prims >= 0; n_prims-- )
    {
        SCOPYL(&in->vz, out);
#ifdef __psyz
        /* The only hard-coded prim offsets left in this file. +8/+10 are x0/y0
         * on the console; with PSY-Z's 8-byte tag+len header they land on
         * r0/g0/b0 AND the command byte, so every TILE/SPRT rectangle reached
         * the GPU with its opcode replaced by the low byte of a screen
         * coordinate. The table already carries the right value (12 here). */
        SSTOREL(in->vx - x, out + prim->voffset);
        SSTOREL(in->vy - y, out + prim->voffset + 2);
#else
        SSTOREL(in->vx - x, out + 8);
        SSTOREL(in->vy - y, out + 10);
#endif

        in++;
        out += psize;
    }

    return out;
}

STATIC void MakePrimsRectangleSingle( DG_PRIM *prim )
{
    SVECTOR *verts;
    char    *packs;
    int      n_prims;

    verts = prim->pos;
    packs = (char *)prim->packs[GV_Clock];

    for ( n_prims = prim->prim_count; n_prims > BATCH_SIZE; n_prims -= BATCH_SIZE )
    {
        verts = _RotTransPers(verts, BATCH_SIZE / 3);
        packs = _MakeXYZRectangleSingle(prim, packs, BATCH_SIZE);
    }

    _RotTransPers(verts, ( n_prims + 2 ) / 3);
    _MakeXYZRectangleSingle(prim, packs, n_prims);
}

#define INIT_GEOM_OFFSET( addr ) \
    ((VECTOR *)(addr))->vy = 0;  \
    ((VECTOR *)(addr))->vx = 0;

#define SET_GEOM_OFFSET( vec, addr )             \
    gte_SetGeomOffset( (vec)->vx , (vec)->vy );  \
    ((VECTOR *)(addr))->vz = *(int *)&(vec)->vz; \
    gte_SetTransVector( addr );

STATIC char *_MakeXYZRectangle( DG_PRIM *prim, char *out, int n_prims )
{
    SVECTOR  bound[2];
    RECT    *rect;
    int      x, y;
    int      psize;
    int      voffset;
    int      vstep;
    SVECTOR *in;
    DVECTOR *processed;
    char    *vert;

    rect = prim->rect;

    x = -rect->x;
    bound[0].vx = x;

    y = -rect->y;
    bound[0].vy = y;
    bound[0].vz = 0;

    bound[1].vx = rect->w + x;
    bound[1].vy = rect->h + y;
    bound[1].vz = 0;

    gte_ldv01c ( bound );
    gte_SetRotMatrix( &DG_ZeroMatrix );

    psize = prim->psize;
    voffset = prim->voffset;
    vstep = prim->vstep;

    INIT_GEOM_OFFSET( getScratchAddr(252) );

    in = (SVECTOR *)getScratchAddr(0);
    processed = (DVECTOR *)getScratchAddr(246);

    SET_GEOM_OFFSET( in, (VECTOR *)getScratchAddr(252) );
    gte_rtpt();

    in++;

    gte_stsxy01( &processed[0] , &processed[1] );

    for ( n_prims--; n_prims >= 0; n_prims-- )
    {
        SET_GEOM_OFFSET( in, (VECTOR *)getScratchAddr(252) );
        gte_rtpt();

        vert = out + voffset;

        SSTOREL( in[-1].vz, out );

        LCOPY( &processed[0], vert );
        vert += vstep;

        SSTOREL( processed[1].vx, vert + 0 );
        SSTOREL( processed[0].vy, vert + 2 );
        vert += vstep;

        SSTOREL( processed[0].vx, vert + 0 );
        SSTOREL( processed[1].vy, vert + 2 );
        vert += vstep;

        LCOPY( &processed[1], vert );

        in++;
        out += psize;

        gte_stsxy01( &processed[0] , &processed[1] );
    }

    gte_SetGeomOffset( 0, 0 );
    return out;
}

STATIC void MakePrimsRectangle( DG_PRIM *prim )
{
    SVECTOR *verts;
    char    *packs;
    int      n_prims;

    verts = prim->pos;
    packs = (char *)prim->packs[GV_Clock];

    for ( n_prims = prim->prim_count; n_prims > (BATCH_SIZE - 3); n_prims -= (BATCH_SIZE - 3) )
    {
        verts = _RotTransPers(verts, (BATCH_SIZE - 3) / 3);
        packs = _MakeXYZRectangle(prim, packs, (BATCH_SIZE - 3));
    }

    _RotTransPers(verts, ( n_prims + 2 ) / 3);
    _MakeXYZRectangle(prim, packs, n_prims);
}

STATIC void MakePrimsFreePacks( DG_PRIM *prim )
{
    int       prim_count;
    POLY_FT4 *packs;

    prim_count = prim->prim_count;
    packs = (POLY_FT4 *)prim->packs[GV_Clock];

    _RotTransPers(prim->pos, prim->n_prims);
    prim->handler(prim, packs, prim_count);
}

//todo: this is dumb, must be something else
static inline void AdjustOverscan( MATRIX *matrix, int val )
{
    matrix->m[1][0] = val;
    //matrix->m[1][0] = (matrix->m[1][0] * 58) / 64;
    matrix->m[1][1] = (matrix->m[1][1] * 58) / 64;
    matrix->m[1][2] = (matrix->m[1][2] * 58) / 64;

    matrix->t[1]    = (matrix->t[1]    * 58) / 64;
}

void DG_PrimChanl( DG_CHANL *chanl, int idx )
{
    MATRIX    modelview;
    int       n_prims;
    RECT     *clip_rect;
    int       group_id;
    MATRIX   *eye;
    DG_PRIM **queue;
    DG_PRIM  *prim;
    int       type;
    int       x;

    n_prims = chanl->queue_size - chanl->prim_index;
    clip_rect = &chanl->clip_rect;

    if ( n_prims == 0 )
    {
        return;
    }

    DG_Clip( clip_rect, chanl->clip_distance );

    group_id = DG_CurrentGroupID;
    eye = &chanl->eye_inv;

    queue = (DG_PRIM **)&chanl->queue[ chanl->prim_index ];
    for ( ; n_prims > 0 ; n_prims-- )
    {
        prim = *queue++;
        type = prim->type;

        if ( type & ( DG_PRIM_INVISIBLE | DG_PRIM_SORTONLY ) )
        {
            continue;
        }

        if ( prim->group_id && !( prim->group_id & group_id ) )
        {
            continue;
        }

        if ( !( type & DG_PRIM_ON_CAMERA ) )
        {
            if ( prim->root )
            {
                prim->world = *prim->root;
            }

            gte_CompMatrix( eye, &prim->world, &modelview );

            x = (modelview.m[1][0] * 58) / 64;
            AdjustOverscan( &modelview, x );

            gte_SetRotMatrix( &modelview );
            gte_SetTransMatrix( &modelview );
        }
        else
        {
            gte_SetRotMatrix( &DG_ZeroMatrix );
            gte_SetTransMatrix( &DG_ZeroMatrix );
        }

        if ( !( type & DG_PRIM_FREEPACKS ) )
        {
            if ( !( type & DG_PRIM_RECTANGLE ) )
            {
                if ( type & DG_PRIM_ONESIDE )
                {
                    MakePrimsOneface( prim );
                }
                else
                {
                    MakePrims( prim );
                }
            }
            else
            {
                if ( prim->verts == 1 )
                {
                    MakePrimsRectangleSingle( prim );
                }
                else
                {
                    MakePrimsRectangle( prim );
                }
            }
        }
        else
        {
            MakePrimsFreePacks( prim );
        }

        type &= 0x1F;

        if ( type == DG_PRIM_LINE_FT2 || type == DG_PRIM_LINE_GT2 )
        {
            DG_AdjustLaserPrims( prim, type );
        }
    }
}

void DG_PrimEnd( void )
{
    /* do nothing */
}

DG_PRIM *DG_MakePrim( int type, int prim_count, int chanl, SVECTOR *pos, RECT *rect )
{
    DG_PRIM_INFO *info;
    int           pack_size;
    DG_PRIM      *prim;

    info = &DG_PrimInfos[type & 31];
    pack_size = info->psize * prim_count;

    prim = GV_Malloc(sizeof(DG_PRIM) + pack_size * 2);
    if (!prim)
    {
        return 0;
    }

    GV_ZeroMemory(prim, sizeof(DG_PRIM));
    prim->world = DG_ZeroMatrix;

    prim->type = type;
    prim->prim_count = prim_count;
    prim->chanl = chanl;
    prim->pos = pos;
    prim->rect = rect;

    // copy prim info
    prim->psize = info->psize;
    prim->verts = info->verts;
    prim->voffset = info->voffset;
    prim->vstep = info->vstep;

    // Point to data after the end of the structure
    prim->packs[0] = &prim[1];
    prim->packs[1] = (char *)&prim[1] + pack_size;

#ifdef __psyz
    /* Give every pack its length. GV_ZeroMemory above only clears the DG_PRIM
     * header, so the pack area arrives holding whatever the heap block held
     * before, and each caller is trusted to write the length itself. Most do.
     * The one drawing the dock's searchlights does not, and its packs reached
     * the queue with a stale word where the length belongs -- the same value in
     * every consecutive pack, which is what a leftover colour array looks like.
     * The reader then dropped each packet and resumed mid-primitive.
     *
     * On the console this could not happen: the length shared a word with the
     * next-pointer, and addPrim() rewrote that word on every insertion, so a
     * forgotten setlen() was invisible. PSY-Z splits them, and addPrim() no
     * longer touches the length -- so "forgotten" now means "never written".
     * That is why this is a port bug and not a game bug.
     *
     * len counts the words after the tag: psize covers the 12-byte header plus
     * the payload, so psize/4 - 2. Verified against PSY-Z's own macros --
     * FT4 44 -> 9, GT4 56 -> 12, TILE 20 -> 3. Callers that do set their own
     * length just write the same value again. */
    {
        int len = info->psize / 4 - 2;
        int i, n;

        if (len > 0)
        {
            for (i = 0; i < 2; i++)
            {
                char *p = prim->packs[i];
                for (n = prim_count; n > 0; n--, p += info->psize)
                {
                    setlen(p, len);
                }
            }
        }
    }
#endif

    return prim;
}

void DG_FreePrim( DG_PRIM *prim )
{
    if (prim)
    {
        GV_DelayedFree(prim);
    }
}

void DG_SetFreePrimParam( int psize, int verts, int voffset, int vstep )
{
    DG_PRIM_INFO *info;

    info = &DG_PrimInfos[DG_PRIM_FREE];
    info->psize = psize;
    info->verts = verts;
    info->voffset = voffset;
    info->vstep = vstep;
}
