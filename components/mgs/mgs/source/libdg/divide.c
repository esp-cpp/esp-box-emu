#include "libdg.h"
#include "common.h"

extern GV_HEAP       MemorySystems_800AD2F0[MAX_MEMSYS];
extern OT_TYPE ptr_800B1400[256];

typedef struct DG_DivideMem
{
    OT_TYPE    *ot;         // 0x00
    short       field_04;   // 0x04
    u_short     raise;      // 0x06
    long        opz;        // 0x08 outer product
    int         field_0C;   // 0x0C  some sort of delta
    long        field_10;   // 0x10
    long        field_14;   // 0x14
    long        field_18;   // 0x18
    POLY_GT4   *pack;       // 0x1C
    GV_HEAP    *pHeap;      // 0x20
    GV_ALLOC   *pAlloc;     // 0x24
    int         n_packs;    // 0x28
    void       *pDataStart; // 0x2C
    int         size;       // 0x30
    DG_RVECTOR *rvec;       // 0x34
} DG_DivideMem;

static inline DG_DivideMem *GetDivideMem()
{
    return (DG_DivideMem *)(SCRPAD_ADDR);
}

STATIC void *DG_SplitMemory( int memIdx, int* n_split, int size );
STATIC POLY_GT4 *DG_InitDividePacks( int memIdx );
STATIC void *DG_AllocDividePackMem( GV_HEAP *heap, GV_ALLOC **alloc_list, int *size );
STATIC POLY_GT4 *DG_GetDividePacks( void );
STATIC int  DG_GetRVectorCode( DG_RVECTOR *rvec );
STATIC void DG_SetRVectorCode( DG_RVECTOR *rvec );
STATIC void DG_SubdivideRVectorPoints( DG_RVECTOR *rvec1, DG_RVECTOR *rvec2, DG_RVECTOR *rvec3 );
STATIC void DG_SetRVectorDelta( DG_RVECTOR *rvec1, DG_RVECTOR *rvec2, DG_RVECTOR *rvec3 );
STATIC int  DG_CopyPackToRVector( DG_RVECTOR *rvec );
STATIC void DG_SubDivideRVectors( void );
STATIC void DG_InitRVector( DG_OBJ *obj,  int idx );
STATIC void DG_AddSubdividedPrim( DG_OBJ *obj, int idx );

STATIC void *DG_SplitMemory( int memIdx, int *n_split, int size )
{
    int i, split_count;
    GV_HEAP *heap;
    GV_ALLOC *alloc;

    split_count = 0;
    heap = &MemorySystems_800AD2F0[ memIdx ];

    alloc = heap->units;
    i = heap->used;
    while ( i > 0 )
    {
        if (alloc->state == GV_ALLOC_STATE_FREE)
        {
            split_count += (alloc[1].start - alloc[0].start ) / size;
        }
        --i;
        alloc++;
    }

    *n_split = split_count;
    return heap;
}

STATIC POLY_GT4 *DG_InitDividePacks( int memIdx )
{
    POLY_GT4 *pack;
    GV_HEAP  *heap;

    DG_DivideMem *divide_mem = GetDivideMem();

#ifdef __psyz
    /* 0x34 = 52 = the PACKED POLY_GT4. PSY-Z's is 56 (8-byte header): slots
     * carved at the console size make consecutive packs overlap by 4 bytes,
     * each one's last vertex overwriting the next one's tag. */
    heap = DG_SplitMemory( memIdx, &divide_mem->n_packs, sizeof(POLY_GT4) );
#else
    heap = DG_SplitMemory( memIdx, &divide_mem->n_packs, 0x34 );
#endif

    divide_mem->pHeap = heap;
    divide_mem->pAlloc = 0;
    pack = DG_AllocDividePackMem( heap, &divide_mem->pAlloc, &divide_mem->size );

    divide_mem->pDataStart = pack;
    return pack;
}

void DG_DivideStart( void )
{
    /* do nothing */
}

STATIC void *DG_AllocDividePackMem( GV_HEAP *heap, GV_ALLOC **alloc_list, int *size )
{
    int i;
    int alloc_idx;
    GV_ALLOC *allocs;

    allocs = *alloc_list;

    if (!allocs)
    {
        allocs = heap->units;
    }
    else
    {
        allocs++;
    }

    //gets the number of allocs between the current one and the total
    alloc_idx  = (int)(allocs - 2);
    alloc_idx -= (int)heap;
    alloc_idx >>= 3;

    i = heap->used - alloc_idx;

    for ( ; i > 0 ; --i )
    {
        if ( allocs->state == GV_ALLOC_STATE_FREE )
        {
            alloc_list[0] = allocs;
            *size = allocs[1].start - allocs[0].start;
            return allocs->start;
        }
        allocs++;
    }

    *size = 0;
    return  0;
}

STATIC POLY_GT4 *DG_GetDividePacks( void )
{
    POLY_GT4        *pack_addr;
    DG_DivideMem    *divide_mem;

    divide_mem = GetDivideMem();

#ifdef __psyz
    divide_mem->size = divide_mem->size - (int)sizeof(POLY_GT4);
#else
    divide_mem->size = divide_mem->size - 0x34;
#endif

    if (divide_mem->size < 0)
    {
        divide_mem->pDataStart = DG_AllocDividePackMem( divide_mem->pHeap, &divide_mem->pAlloc, &divide_mem->size );
    }
    else
    {
        divide_mem->n_packs -= 1;
        pack_addr = divide_mem->pDataStart;
#ifdef __psyz
        divide_mem->pDataStart = (char *)divide_mem->pDataStart + sizeof(POLY_GT4);
#else
        divide_mem->pDataStart += 0x34;
#endif
        return pack_addr;
    }

    if ( divide_mem->pDataStart )
    {
        return DG_GetDividePacks();
    }
    else
    {
        divide_mem->n_packs = 0;
        return 0;
    }
}

STATIC int DG_GetRVectorCode( DG_RVECTOR *rvec )
{
    int code;

    if ( !rvec->c.cd ) return 0;

    code = rvec[3].c.cd;

    return rvec->c.cd & rvec[1].c.cd & code & rvec[4].c.cd;
}

STATIC void DG_SetRVectorCode( DG_RVECTOR *rvec )
{
    char code = 0;

    if ( rvec->sxy.vx < -160 )
    {
        code = 1;
    }
    else if ( 160 < rvec->sxy.vx  )
    {
        code = 2;
    }

    if ( rvec->sxy.vy < -112 )
    {
        code |= 4;
    }
    else if ( 112 < rvec->sxy.vy )
    {
        code |= 8;
    }

    if ( !rvec->sz )
    {
        code |= 16;
    }

    rvec->c.cd = code;
}

STATIC void DG_SubdivideRVectorPoints( DG_RVECTOR *rvec1, DG_RVECTOR *rvec2, DG_RVECTOR *rvec3 )
{
    rvec3->v.vx = (rvec1->v.vx + rvec2->v.vx) / 2;
    rvec3->v.vy = (rvec1->v.vy + rvec2->v.vy) / 2;
    rvec3->v.vz = (rvec1->v.vz + rvec2->v.vz) / 2;

    rvec3->uv[0] = (rvec1->uv[0] + rvec2->uv[0]) / 2;
    rvec3->uv[1] = (rvec1->uv[1] + rvec2->uv[1]) / 2;

    rvec3->c.r = (rvec1->c.r + rvec2->c.r) / 2;
    rvec3->c.g = (rvec1->c.g + rvec2->c.g) / 2;
    rvec3->c.b = (rvec1->c.b + rvec2->c.b) / 2;
}

STATIC void DG_SetRVectorDelta( DG_RVECTOR *rvec1, DG_RVECTOR *rvec2, DG_RVECTOR *rvec3 )
{
    int vy_diff, vx_diff, delta;

    vy_diff = rvec2->sxy.vy - rvec1->sxy.vy;
    vx_diff = rvec1->sxy.vx - rvec2->sxy.vx;
    delta = GetDivideMem()->field_0C;

    if ( vy_diff >= 0 )
    {
        if (vx_diff >= 0)
        {
            if ( vx_diff / 2 < vy_diff )
            {
                rvec3->sxy.vx += delta;
            }
            //loc_80019370:
            if (vy_diff / 2 < vx_diff)
            {
                rvec3->sxy.vy += delta;
            }
        }
        else
        {
            //loc_80019398
            vx_diff = -vx_diff;
            if ( vx_diff / 2 < vy_diff )
            {
                rvec3->sxy.vx += delta;
            }
            //loc_8001941C:
            if (vy_diff / 2 < vx_diff)
            {
                rvec3->sxy.vy -= delta;
            }
        }
    }
    else
    {
        vy_diff = -vy_diff;
        if (vx_diff >= 0)
        {
            if ( vx_diff / 2 < vy_diff )
            {
                rvec3->sxy.vx -= delta;
            }
            //loc_80019370:
            if (vy_diff / 2 < vx_diff)
            {
                rvec3->sxy.vy += delta;
            }
        }
        else
        {
            //loc_800193EC
            vx_diff = -vx_diff;
            if ( vx_diff / 2 < vy_diff )
            {
                rvec3->sxy.vx -= delta;
            }
            //loc_8001941C:
            if (vy_diff / 2 < vx_diff)
            {
                rvec3->sxy.vy -= delta;
            }
        }
    }
}

STATIC int DG_CopyPackToRVector( DG_RVECTOR *rvec )
{
    int v1;
    DG_DivideMem    *divide_mem;
    DG_DivideMem    *divide_mem2;
    POLY_GT4        *pack;
    POLY_GT4        *pack2;
    OT_TYPE         *ot;
    int              z_idx;

    if ( DG_GetRVectorCode( rvec ) ) return 0;

    divide_mem = GetDivideMem();

    if ( ( (unsigned int)divide_mem->rvec < (SCRPAD_ADDR + 0x254) ) && ( divide_mem->n_packs >= 4 ) )
    {
        gte_NormalClip( *(int*)&rvec->sxy, *(int*)&rvec[1].sxy, *(int*)&rvec[3].sxy, &divide_mem->opz );
        v1 = divide_mem->opz;

        if ( v1 < 0 ) v1 = -v1;

        if ( divide_mem->field_14 < v1 )
        {
            DG_RVECTOR *rvec_temp = divide_mem->rvec;
            rvec_temp[0] = rvec[0];
            rvec_temp[2] = rvec[1];
            rvec_temp[6] = rvec[3];
            rvec_temp[8] = rvec[4];
            return 1;
        }
    }

    pack = DG_GetDividePacks();

    if ( !pack ) return 0;

    divide_mem2 = GetDivideMem();

    z_idx = (unsigned short)( ( ( rvec->sz + rvec[4].sz ) / 2 ) - divide_mem2->raise );

    if ( z_idx < 0 ) z_idx = 0;

    //loc_800195F0
    LCOPY2( &rvec[0].sxy, &pack->x0, &rvec[1].sxy, &pack->x1 );
    LCOPY2( &rvec[3].sxy, &pack->x2, &rvec[4].sxy, &pack->x3 );

    SCOPYL2( &rvec[0].uv, &pack->u0, &rvec[1].uv, &pack->u1 );
    SCOPYL2( &rvec[3].uv, &pack->u2, &rvec[4].uv, &pack->u3 );

    LCOPY2( &rvec[0].c, &pack->r0, &rvec[1].c, &pack->r1 );
    LCOPY2( &rvec[3].c, &pack->r2, &rvec[4].c, &pack->r3 );

    pack2 = divide_mem2->pack;
    pack->code = pack2->code;

    SCOPYL( &pack2->clut,  &pack->clut );
    SCOPYL( &pack2->tpage, &pack->tpage );

    ot = divide_mem2->ot;
    ot = &ot[ ( unsigned char ) z_idx ];

    /* first radix pass, same as add_prim_mid: low byte picks the bucket, high
     * byte rides in the length field until DG_SortChanl re-buckets by it */
    addPrim( ot, pack );
    setlen( pack, z_idx >> 8 );
    return 0;
}

static inline void divide_setup( void )
{
    DG_RVECTOR *rvec;
    rvec = GetDivideMem()->rvec;

    DG_SubdivideRVectorPoints( &rvec[0], &rvec[2], &rvec[1] );
    DG_SubdivideRVectorPoints( &rvec[6], &rvec[8], &rvec[7] );

    gte_ldv3( &rvec[1], &rvec[1], &rvec[7] );
    gte_rtpt();

    DG_SubdivideRVectorPoints( &rvec[0], &rvec[6], &rvec[3] );
    DG_SubdivideRVectorPoints( &rvec[1], &rvec[7], &rvec[4] );
    DG_SubdivideRVectorPoints( &rvec[2], &rvec[8], &rvec[5] );

    gte_stsxy3( &rvec[1].sxy, &rvec[1].sxy, &rvec[7].sxy );
    gte_stsz3(  &rvec[1].sz, &rvec[1].sz, &rvec[7].sz );
    gte_ldv3( &rvec[3], &rvec[4] , &rvec[5] );
    gte_rtpt();

    DG_SetRVectorCode( &rvec[1] );
    DG_SetRVectorCode( &rvec[7] );
    DG_SetRVectorDelta( &rvec[0], &rvec[2], &rvec[1] );
    DG_SetRVectorDelta( &rvec[8], &rvec[6], &rvec[7] );

    gte_stsxy3( &rvec[3].sxy, &rvec[4].sxy, &rvec[5].sxy );
    gte_stsz3(  &rvec[3].sz, &rvec[4].sz, &rvec[5].sz );

    DG_SetRVectorCode( &rvec[3] );
    DG_SetRVectorCode( &rvec[4] );
    DG_SetRVectorCode( &rvec[5] );
    DG_SetRVectorDelta( &rvec[2], &rvec[8], &rvec[5] );
    DG_SetRVectorDelta( &rvec[6], &rvec[0], &rvec[3] );
}

//todo: odd function requires do while hack, need to revisit
STATIC void DG_SubDivideRVectors( void )
{
    int mask = 1;
    DG_DivideMem *divide_mem;

    divide_setup();
    do {} while (0); //hack thats needed to make it use the right registers

    divide_mem = GetDivideMem();

    divide_mem->rvec += 9;

    if ( DG_CopyPackToRVector( &divide_mem->rvec[-9] ) ) DG_SubDivideRVectors();
    if ( DG_CopyPackToRVector( &divide_mem->rvec[-8] ) ) DG_SubDivideRVectors();
    if ( DG_CopyPackToRVector( &divide_mem->rvec[-6] ) && mask ) DG_SubDivideRVectors();
    if ( DG_CopyPackToRVector( &divide_mem->rvec[-5] ) ) DG_SubDivideRVectors();

    divide_mem->rvec -= 9;
}

typedef struct cpystrct {
    unsigned long cpy[2];
} cpystrct;

static inline void copy_verts(unsigned char *faceIndexOffset, SVECTOR *vertexIndexOffset)
{
    *(cpystrct*)(SCRPAD_ADDR + 0x038) = *(cpystrct*)&vertexIndexOffset[faceIndexOffset[0]];
    *(cpystrct*)(SCRPAD_ADDR + 0x060) = *(cpystrct*)&vertexIndexOffset[faceIndexOffset[1]];
    *(cpystrct*)(SCRPAD_ADDR + 0x0B0) = *(cpystrct*)&vertexIndexOffset[faceIndexOffset[3]];
    *(cpystrct*)(SCRPAD_ADDR + 0x0D8) = *(cpystrct*)&vertexIndexOffset[faceIndexOffset[2]];
}

//function seems to call scratchpad addresses directly rather than through a struct
STATIC void DG_InitRVector( DG_OBJ *obj,  int idx )
{
    POLY_GT4     *pack;
    POLY_GT4     *org_pack;
    int           n_packs;

    org_pack = obj->packs[ idx ];
    *(short*)(SCRPAD_ADDR + 0x006) = obj->raise;

    while ( obj )
    {
        unsigned char *faceIndexOffset   = obj->model->vindices;
        SVECTOR       *vertexIndexOffset = obj->model->vertices;
        n_packs = obj->n_packs;
        pack = org_pack;

        for ( --n_packs ; n_packs >= 0 ; --n_packs )
        {
            int pack_raise = pack->tag & 0xFFFF;
            int pack_addr  = pack->tag >> 8;

            if ( ( *(unsigned int*)(SCRPAD_ADDR + 0x014) < pack_addr ) &&
                 ( pack_raise < *(int*)(SCRPAD_ADDR + 0x018) )         &&
                 ( *(int*)(SCRPAD_ADDR + 0x028) >= 4 ) )
            {
                *(short*)pack = 0;
                *(int*)(SCRPAD_ADDR + 0x01C) = (int)pack;

                if ( pack_addr & 0x100 )
                {
                    *(int*)(SCRPAD_ADDR + 0x00C) = -*(int*)(SCRPAD_ADDR + 0x010);
                }
                else
                {
                    *(int*)(SCRPAD_ADDR + 0x00C) = *(int*)(SCRPAD_ADDR + 0x010);
                }

                //loc_80019A04:
                *(int*)(SCRPAD_ADDR + 0x034) = (int)(SCRPAD_ADDR + 0x038);

                copy_verts( faceIndexOffset, vertexIndexOffset );

                LCOPY2(  &pack->x0 , (void*)(SCRPAD_ADDR + 0x044) , &pack->x1 , (void*)(SCRPAD_ADDR + 0x06C) );
                LCOPY2(  &pack->x2 , (void*)(SCRPAD_ADDR + 0x0BC) , &pack->x3 , (void*)(SCRPAD_ADDR + 0x0E4) );
                SCOPYL2( &pack->u0 , (void*)(SCRPAD_ADDR + 0x03E) , &pack->u1 , (void*)(SCRPAD_ADDR + 0x066) );
                SCOPYL2( &pack->u2 , (void*)(SCRPAD_ADDR + 0x0B6) , &pack->u3 , (void*)(SCRPAD_ADDR + 0x0DE) );
                LCOPY2(  &pack->r0 , (void*)(SCRPAD_ADDR + 0x040) , &pack->r1 , (void*)(SCRPAD_ADDR + 0x068) );
                LCOPY2(  &pack->r2 , (void*)(SCRPAD_ADDR + 0x0B8) , &pack->r3 , (void*)(SCRPAD_ADDR + 0x0E0) );

                gte_ldv3( (SCRPAD_ADDR + 0x060) , (SCRPAD_ADDR + 0x0B0) , (SCRPAD_ADDR + 0x0D8) );
                gte_rtpt();
                gte_stsz3( (SCRPAD_ADDR + 0x070) , (SCRPAD_ADDR + 0x0C0) , (SCRPAD_ADDR + 0x0E8) );

                gte_ldv0( (SCRPAD_ADDR + 0x038) );
                gte_rtps();
                gte_stsz( (SCRPAD_ADDR + 0x048) );

                DG_SetRVectorCode( (DG_RVECTOR*)(SCRPAD_ADDR + 0x038) );
                DG_SetRVectorCode( (DG_RVECTOR*)(SCRPAD_ADDR + 0x060) );
                DG_SetRVectorCode( (DG_RVECTOR*)(SCRPAD_ADDR + 0x0B0) );
                DG_SetRVectorCode( (DG_RVECTOR*)(SCRPAD_ADDR + 0x0D8) );
                DG_SubDivideRVectors();

            }
            else
            {
                if ( pack_raise )
                {
                    OT_TYPE *ot;
                    u_short raise;

                    ot    = GetDivideMem()->ot;
                    raise = GetDivideMem()->raise;

                    raise = pack_raise - raise;
                    pack_raise = raise;
                    ot = &ot[ ( u_char ) pack_raise ];

                    /* first radix pass: bucket by the low byte of the depth and
                     * park the high byte in the length field, which the second
                     * pass (DG_SortChanl) re-buckets by. PSY-Q packed both into
                     * one 32-bit tag; PSY-Z keeps addr and len apart. */
                    addPrim( ot, pack );
                    setlen( pack, pack_raise >> 8 );
                }
            }
            pack++;
            faceIndexOffset += 4;
        }

        obj = obj->extend;
        org_pack = pack;
    }
}

static inline void add_prim_mid( OT_TYPE *ot, POLY_GT4 *pack, int z_idx, int raise )
{
    OT_TYPE *temp;
    z_idx = (z_idx - raise);
    z_idx &= 0xFFFF;

    temp = &ot[ ( unsigned char ) z_idx ];

    /* see the note in DG_DividePrim: low byte selects the bucket, high byte is
     * stashed in the length field for the second radix pass */
    addPrim( temp, pack );
    setlen( pack, z_idx >> 8 );
}

STATIC void DG_AddSubdividedPrim( DG_OBJ *obj, int idx )
{
    POLY_GT4 *org_pack;
    POLY_GT4 *pack;
    int       raise;
    int       n_packs;
    OT_TYPE  *ot_temp;
    u_short   pack_raise;

    org_pack  = obj->packs[ idx ];
    raise = obj->raise; //t1

    while ( obj )
    {
        n_packs = obj->n_packs;
        pack = (POLY_GT4*)getaddr( &org_pack );

        /* was three lines reading the pointer parked at scratchpad offset 0;
         * that is exactly DG_DivideMem::ot, which lives there */
        ot_temp = GetDivideMem()->ot;

        for ( --n_packs ; n_packs >= 0 ; --n_packs )
        {
            pack_raise = pack->tag;
            if ( pack_raise )
            {
                add_prim_mid( ot_temp, pack, pack_raise, raise );
            }
            pack++;
        }
        obj = obj->extend;
        org_pack = pack;
    }
}

void DG_DivideChanl( DG_CHANL *chanl, int idx )
{
    int i, j, x;
    DG_OBJS **objs_queue;
    DG_OBJS  *objs;
    DG_OBJ   *obj;
    DG_DivideMem *divide_mem;

    if ( !DG_InitDividePacks( idx ) ) return;

    DG_Clip( &chanl->clip_rect, chanl->clip_distance );

    divide_mem = GetDivideMem();
    divide_mem->ot = ptr_800B1400;
    divide_mem->field_14 = 0x800;

    if ( chanl->clip_distance > 1000)
    {
        divide_mem->field_18 = 60000;
    }
    else
    {
        divide_mem->field_18 = 8192;
    }

    objs_queue = chanl->queue;
    //s6 = 1
    x = 1;
    for ( i = chanl->objs_index ; i > 0 ; --i )
    {
        objs = *objs_queue;
        objs_queue++;

        if ( !objs->bound_mode ) continue;

        obj = objs->objs;
        if ( !( objs->flag & DG_FLAG_GBOUND ) )
        {
            for ( j = objs->n_models ; j > 0 ; --j )
            {
                if (obj->bound_mode)
                {
                    gte_SetRotMatrix( &obj->screen );
                    gte_SetTransMatrix( &obj->screen );

                    if ( obj->model->flags & DG_MODEL_TRANS )
                    {
                        GetDivideMem()->field_10 = 0;
                    }
                    else
                    {
                        GetDivideMem()->field_10 = x;
                    }

                    SetSpadStack( SPAD_STACK_ADDR );
                    DG_InitRVector( obj, idx );
                    ResetSpadStack();

                }
                obj++;
            }
        }
        else
        {
            for ( j = objs->n_models ; j > 0 ; --j )
            {
                if (obj->bound_mode)
                {
                    DG_AddSubdividedPrim( obj, idx );
                }
                obj++;
            }
        }
    }
}

void DG_DivideEnd( void )
{
    /* do nothing */
}
