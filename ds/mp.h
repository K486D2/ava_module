#ifndef MP_H
#define MP_H

#include <pthread.h>

#include "ds/list.h"
#include "util/marcodef.h"
#include "util/typedef.h"

#define MP_SIZE     (64 * 1024)
#define ALIGNMENT   (8)
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

typedef struct {
        list_head_t block;
        usz         cap;
} mp_block_t;

typedef struct {
        list_head_t     free;
        u8              pool[MP_SIZE];
        usz             offset;
        pthread_mutex_t lock;
} mp_t;

HAPI void
mp_init(mp_t *mp)
{
        pthread_mutex_init(&mp->lock, NULL);
        mp->offset = 0;
        list_init(&mp->free);
}

HAPI void *
mp_alloc(mp_t *mp, usz cap)
{
        pthread_mutex_lock(&mp->lock);

        usz req_cap = ALIGN(cap);
        usz total   = req_cap + ALIGN(sizeof(mp_block_t));

        list_head_t *pos;
        LIST_FOR_EACH(pos, &mp->free)
        {
                mp_block_t *block = CONTAINER_OF(pos, mp_block_t, block);
                if (block->cap >= total) {
                        if (block->cap >= total + ALIGN(sizeof(mp_block_t)) + ALIGNMENT) {
                                list_del(pos);

                                mp_block_t *split = (mp_block_t *)((u8 *)block + total);
                                split->cap        = block->cap - total;
                                block->cap        = total;

                                list_head_t *iter = mp->free.next;
                                while (iter != &mp->free && (u8 *)CONTAINER_OF(iter, mp_block_t, block) < (u8 *)split) {
                                        iter = iter->next;
                                }
                                __list_add(&split->block, iter->prev, iter);
                        } else {
                                list_del(pos);
                        }
                        void *ptr = (void *)((u8 *)block + ALIGN(sizeof(mp_block_t)));
                        pthread_mutex_unlock(&mp->lock);
                        return ptr;
                }
        }

        if (mp->offset + total > MP_SIZE) {
                pthread_mutex_unlock(&mp->lock);
                return NULL;
        }

        mp_block_t *block  = (mp_block_t *)&mp->pool[mp->offset];
        block->cap         = total;
        mp->offset        += total;
        void *ptr          = (void *)((u8 *)block + ALIGN(sizeof(mp_block_t)));
        pthread_mutex_unlock(&mp->lock);
        return ptr;
}

HAPI void
mp_free(mp_t *mp, void *ptr)
{
        if (!ptr)
                return;

        pthread_mutex_lock(&mp->lock);

        mp_block_t *block = (mp_block_t *)((u8 *)ptr - ALIGN(sizeof(mp_block_t)));

        // 按地址顺序插入空闲链表
        list_head_t *pos = mp->free.next;
        while (pos != &mp->free && (u8 *)CONTAINER_OF(pos, mp_block_t, block) < (u8 *)block) {
                pos = pos->next;
        }
        __list_add(&block->block, pos->prev, pos);

        pthread_mutex_unlock(&mp->lock);
}

HAPI void
mp_reset(mp_t *mp)
{
        pthread_mutex_lock(&mp->lock);
        mp->offset = 0;
        list_init(&mp->free);
        pthread_mutex_unlock(&mp->lock);
}

#endif // !MP_H
