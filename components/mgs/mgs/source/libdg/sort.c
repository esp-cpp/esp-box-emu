#include "libdg.h"

typedef struct _SCRATCHPAD_UNK {
    OT_TYPE *buf;   /* the 256-bucket ordering table of the first radix pass */
    OT_TYPE *ot;
    int      len;
} SCRATCHPAD_UNK;

extern OT_TYPE ptr_800B1400[256];

static inline SCRATCHPAD_UNK * get_scratch(void)
{
    return (SCRATCHPAD_UNK *)(SCRPAD_ADDR + 0x000);
}

static inline OT_TYPE * get_buf(void)
{
    return ((SCRATCHPAD_UNK *)(SCRPAD_ADDR + 0x000))->buf;
}

static inline int DG_GetCurrentGroupID(void)
{
    return DG_CurrentGroupID;
}

void DG_SortChanl( DG_CHANL *chanl, int idx )
{
    void    *list;
    void    *list_next;
    OT_TYPE *buf;

    OT_TYPE *ot;
    OT_TYPE *ot2;
    OT_TYPE *indexed_ot;
    OT_TYPE *ot_ptr;

    int i;

    int index2;
    unsigned int len;

    int group_id;

    int prim_size;
    int prim_count;

    void **pQueue;
    DG_PRIM *pPrim;
    char *prim;

    SCRATCHPAD_UNK *pad = get_scratch();

    pad->buf = ptr_800B1400;
    pad->ot = chanl->ot[idx] + 1;

    buf = get_buf();
    ot = pad->ot;

    /* Second pass of a two-pass radix sort on the 16-bit depth. DG_DivideChanl
     * bucketed by the low byte into `buf` and parked the high byte in the tag's
     * length field; here each chain is re-bucketed by that high byte, and the
     * length is finally set to the real POLY_GT4 payload size.
     *
     * On PSY-Q both halves shared one packed 32-bit tag (24-bit next pointer +
     * 8-bit length). PSY-Z splits them into separate words because a host
     * pointer does not fit in 24 bits, so the same operation is expressed as
     * addPrim() plus setlen(). */
    for (i = 256; i > 0; i--)
    {
        list = (void *)getaddr(buf);
        buf++;

        while (list != 0)
        {
            list_next = (void *)getaddr(list);
            ot_ptr = &ot[getlen(list)];
            addPrim(ot_ptr, list);
            setlen(list, 0x0c);
            list = list_next;
        }
    }

    pQueue = (void **)&chanl->queue[chanl->prim_index];
    group_id = DG_GetCurrentGroupID();

    for (i = chanl->queue_size - chanl->prim_index; i > 0; i--)
    {
        pPrim = *pQueue++;

        if (pPrim->type & DG_PRIM_INVISIBLE)
        {
            continue;
        }

        if (pPrim->group_id && !(pPrim->group_id & group_id))
        {
            continue;
        }

        pad->len = pPrim->raise;

        prim_count = pPrim->prim_count;
        prim = (char *)pPrim->packs[idx];
        prim_size = (short)pPrim->psize;

        /* both were read back through raw scratchpad addresses; they are the
         * same two fields the struct already names */
        ot2 = pad->ot;
        len = (unsigned int)pad->len;

        while (--prim_count >= 0)
        {
            index2 = *((u_short *)prim);

            if (index2 > 0)
            {
                index2 -= len;

                if (index2 < 0)
                {
                    index2 = 0;
                }

                index2 >>= 8;

                indexed_ot = &ot2[index2];
                addPrim(indexed_ot, prim);
            }

            prim += prim_size;
        }
    }
}
