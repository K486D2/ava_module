#ifndef MP_H
#define MP_H

#include <string.h>

#include "../ds/list.h"
#include "../util/macrodef.h"
#include "../util/typedef.h"

#define MP_SIZE              (64 * 1024)
#define MP_ALIGN             (8)
#define MP_ALIGN_UP(sz)      (((sz) + (MP_ALIGN - 1)) & ~(MP_ALIGN - 1))
#define MP_BLOCK_HEADER_SIZE MP_ALIGN_UP(sizeof(mp_blk_t))

typedef struct {
        list_head_t blk_node; // 链表节点
        usz         size;     // 块大小 (包含头部)
} mp_blk_t;

typedef struct {
        list_head_t blk_root;     // 空闲块链表头
        u8          buf[MP_SIZE]; // 内存池缓冲区
        usz         off;          // 当前已分配偏移
        ATOMIC(u32) lock;
} mp_t;

/* -------------------------------------------------------------------------- */
/*                                   声明                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief 内存池初始化
 *
 * @param mp
 */
HAPI void mp_init(mp_t *mp);

/**
 * @brief 从内存池分配 cap 字节
 *
 * @param mp
 * @param cap
 * @return 指向分配内存的指针，失败返回 NULL
 */
HAPI void *mp_alloc(mp_t *mp, usz cap);

/**
 * @brief 从内存池分配 cap 字节并初始化为 0
 *
 * @param mp
 * @param cap
 * @return 指向分配内存的指针，失败返回 NULL
 */
HAPI void *mp_calloc(mp_t *mp, usz cap);

/**
 * @brief 释放内存池中的内存块
 *
 * @param mp
 * @param ptr
 */
HAPI void mp_free(mp_t *mp, void *ptr);

/* -------------------------------------------------------------------------- */
/*                                   定义                                     */
/* -------------------------------------------------------------------------- */

HAPI void
mp_init(mp_t *mp)
{
        list_init(&mp->blk_root);
}

HAPI void *
mp_alloc(mp_t *mp, usz cap)
{
        const usz block_size = MP_BLOCK_HEADER_SIZE + MP_ALIGN_UP(cap);

        SPIN_LOCK(&mp->lock);

        // 查找能满足申请大小 block_size 的空闲块
        list_head_t *node;
        LIST_FOR_EACH(node, &mp->blk_root)
        {
                mp_blk_t *blk = CONTAINER_OF(node, mp_blk_t, blk_node);
                if (blk->size < block_size) // 当前块容量不足则检查下一个
                        continue;

                if (blk->size >= block_size + MP_BLOCK_HEADER_SIZE + MP_ALIGN) {
                        // 能切分出一个新的空闲块: 先移除原块
                        list_del(node);

                        // 从剩余空间生成 split_blk 并按地址顺序插回空闲链表
                        mp_blk_t *split_blk = (mp_blk_t *)((u8 *)blk + block_size);
                        split_blk->size     = blk->size - block_size;
                        blk->size           = block_size;

                        list_head_t *iter = mp->blk_root.next;
                        while (iter != &mp->blk_root && (u8 *)CONTAINER_OF(iter, mp_blk_t, blk_node) < (u8 *)split_blk)
                                iter = iter->next;

                        __list_add(&split_blk->blk_node, iter->prev, iter);
                } else
                        list_del(node); // 无法切分时直接整块取出

                SPIN_UNLOCK(&mp->lock);

                // 返回指向用户数据区域的指针 (跳过块头)
                return (u8 *)blk + MP_BLOCK_HEADER_SIZE;
        }

        // 空闲链表没有合适块，则尝试向后线性分配
        if (mp->off + block_size > MP_SIZE) {
                SPIN_UNLOCK(&mp->lock);
                return NULL; // 内存池耗尽
        }

        mp_blk_t *fresh_blk  = (mp_blk_t *)&mp->buf[mp->off];
        fresh_blk->size      = block_size;
        mp->off             += block_size;

        SPIN_UNLOCK(&mp->lock);
        return (u8 *)fresh_blk + MP_BLOCK_HEADER_SIZE;
}

HAPI void *
mp_calloc(mp_t *mp, const usz cap)
{
        void *ptr = mp_alloc(mp, cap);
        if (!ptr)
                return NULL;

        memset(ptr, 0, cap);
        return ptr;
}

HAPI void
mp_free(mp_t *mp, void *ptr)
{
        if (!ptr)
                return;

        // 回退到块头部，恢复块结构
        mp_blk_t *block = (mp_blk_t *)((u8 *)ptr - MP_BLOCK_HEADER_SIZE);

        SPIN_LOCK(&mp->lock);

        // 将释放块按地址顺序插入空闲链表，方便后续合并
        list_head_t *pos = mp->blk_root.next;
        while (pos != &mp->blk_root && (u8 *)CONTAINER_OF(pos, mp_blk_t, blk_node) < (u8 *)block)
                pos = pos->next;

        __list_add(&block->blk_node, pos->prev, pos);

        SPIN_UNLOCK(&mp->lock);
}

HAPI void
mp_reset(mp_t *mp)
{
        SPIN_LOCK(&mp->lock);
        mp->off = 0;              // 回收整块缓冲区
        list_init(&mp->blk_root); // 清空空闲链表
        SPIN_UNLOCK(&mp->lock);
}

#endif // !MP_H
