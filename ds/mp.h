#ifndef MP_H
#define MP_H

#include "ds/list.h"
#include "util/marcodef.h"
#include "util/typedef.h"

#define MP_SIZE     (64 * 1024)
#define ALIGNMENT   (8)
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

typedef struct {
        usz         cap;
        list_head_t list;
} mp_block_t;

typedef struct {
        u8          pool[MP_SIZE];
        usz         offset;
        list_head_t free;
} mp_t;

HAPI void
mp_init(mp_t *mp)
{
        mp->offset = 0;
        list_init(&mp->free);
}

HAPI void *
mp_alloc(mp_t *mp, usz cap)
{
        usz req_size = ALIGN(cap);
        usz total    = req_size + ALIGN(sizeof(mp_block_t));

        list_head_t *pos;
        LIST_FOR_EACH(pos, &mp->free)
        {
                mp_block_t *block = CONTAINER_OF(pos, mp_block_t, list);
                if (block->cap >= total) {
                        if (block->cap >= total + ALIGN(sizeof(mp_block_t)) + ALIGNMENT) {
                                list_del(pos);

                                mp_block_t *split = (mp_block_t *)((u8 *)block + total);
                                split->cap        = block->cap - total;
                                block->cap        = total;

                                list_head_t *iter;
                                list_head_t *ins_before = NULL;
                                LIST_FOR_EACH(iter, &mp->free)
                                {
                                        mp_block_t *curr = CONTAINER_OF(iter, mp_block_t, list);
                                        if ((u8 *)split < (u8 *)curr) {
                                                ins_before = iter;
                                                break;
                                        }
                                }
                                if (ins_before)
                                        __list_add(&split->list, ins_before->prev, ins_before);
                                else
                                        list_add_tail(&split->list, &mp->free);
                        } else
                                list_del(pos);

                        return (void *)((u8 *)block + ALIGN(sizeof(mp_block_t)));
                }
        }

        if (mp->offset + total > MP_SIZE)
                return NULL;

        mp_block_t *block  = (mp_block_t *)&mp->pool[mp->offset];
        block->cap         = total;
        mp->offset        += total;
        return (void *)((u8 *)block + ALIGN(sizeof(mp_block_t)));
}

HAPI void
mp_free(mp_t *mp, void *ptr)
{
        if (!ptr)
                return;
        mp_block_t *block = (mp_block_t *)((u8 *)ptr - ALIGN(sizeof(mp_block_t)));

        list_head_t *iter;
        list_head_t *ins_before = NULL;
        LIST_FOR_EACH(iter, &mp->free)
        {
                mp_block_t *curr = CONTAINER_OF(iter, mp_block_t, list);
                if ((u8 *)block < (u8 *)curr) {
                        ins_before = iter;
                        break;
                }
        }
        if (ins_before)
                __list_add(&block->list, ins_before->prev, ins_before);
        else
                list_add_tail(&block->list, &mp->free);

        list_head_t *next_pos = block->list.next;
        if (next_pos != &mp->free) {
                mp_block_t *next_block = CONTAINER_OF(next_pos, mp_block_t, list);
                if ((u8 *)block + block->cap == (u8 *)next_block) {
                        block->cap += next_block->cap;
                        list_del(&next_block->list);
                }
        }

        list_head_t *prev_pos = block->list.prev;
        if (prev_pos != &mp->free) {
                mp_block_t *prev_block = CONTAINER_OF(prev_pos, mp_block_t, list);
                if ((u8 *)prev_block + prev_block->cap == (u8 *)block) {
                        prev_block->cap += block->cap;
                        list_del(&block->list);
                }
        }
}

HAPI void
mp_reset(mp_t *mp)
{
        mp->offset = 0;
        list_init(&mp->free);
}

#endif // !MP_H