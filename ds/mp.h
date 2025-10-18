#ifndef MP_H
#define MP_H

#include "ds/list.h"
#include "util/marcodef.h"
#include "util/typedef.h"

#define MP_SIZE     (64 * 1024)
#define ALIGNMENT   (8)
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

typedef struct {
        usz         size;
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
        list_for_each(pos, &mp->free)
        {
                mp_block_t *block = CONTAINER_OF(pos, mp_block_t, list);
                if (block->size >= total) {
                        if (block->size >= total + ALIGN(sizeof(mp_block_t)) + ALIGNMENT) {
                                mp_block_t *split = (mp_block_t *)((u8 *)block + total);
                                split->size       = block->size - total;
                                block->size       = total;

                                list_replace(block->list.next, &split->list);
                                list_del(&block->list);

                                list_head_t *iter;
                                list_head_t *ins_before = NULL;
                                list_for_each(iter, &mp->free)
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
        block->size        = total;
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
        list_for_each(iter, &mp->free)
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
                if ((u8 *)block + block->size == (u8 *)next_block) {
                        block->size += next_block->size;
                        list_del(&next_block->list);
                }
        }

        list_head_t *prev_pos = block->list.prev;
        if (prev_pos != &mp->free) {
                mp_block_t *prev_block = CONTAINER_OF(prev_pos, mp_block_t, list);
                if ((u8 *)prev_block + prev_block->size == (u8 *)block) {
                        prev_block->size += block->size;
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