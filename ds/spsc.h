#ifndef SPSC_H
#define SPSC_H

#include <string.h>

#include "../util/mathdef.h"
#include "../util/typedef.h"

/* 写入数据超过剩余空间时的处理策略 */
typedef enum {
        SPSC_POLICY_TRUNCATE,  // 截断
        SPSC_POLICY_OVERWRITE, // 覆盖
        SPSC_POLICY_REJECT,    // 拒绝
} spsc_policy_e;

typedef struct {
        spsc_policy_e e_policy; // 写入数据超过剩余空间时的处理策略
        void         *buf;      // 缓冲区
        usz           cap;      // 缓冲区容量(2^n)
        ATOMIC(usz) wp;         // 写入位置
        ATOMIC(usz) rp;         // 读取位置
} spsc_t;

HAPI int  spsc_init(spsc_t *spsc, void *buf, usz cap, spsc_policy_e e_policy);
HAPI int  spsc_init_buf(spsc_t *spsc, usz cap, spsc_policy_e e_policy);
HAPI void spsc_reset(spsc_t *spsc);
HAPI bool spsc_empty(spsc_t *spsc);
HAPI bool spsc_full(spsc_t *spsc);
HAPI usz  spsc_avail(spsc_t *spsc);
HAPI usz  spsc_free(spsc_t *spsc);
HAPI usz  spsc_policy(spsc_t *spsc, usz wp, usz rp, usz size);

HAPI usz spsc_write(spsc_t *spsc, const void *src, usz size);
HAPI usz spsc_read(spsc_t *spsc, void *dst, usz size);
HAPI usz spsc_write_buf(spsc_t *spsc, void *buf, const void *src, usz size);
HAPI usz spsc_read_buf(spsc_t *spsc, void *buf, void *dst, usz size);

/* -------------------------------------------------------------------------- */
/*                                     API                                    */
/* -------------------------------------------------------------------------- */

HAPI int
spsc_init(spsc_t *spsc, void *buf, const usz cap, const spsc_policy_e e_policy)
{
        const int ret = spsc_init_buf(spsc, cap, e_policy);
        if (ret != 0)
                return ret;

        spsc->buf = buf;
        return 0;
}

HAPI int
spsc_init_buf(spsc_t *spsc, const usz cap, const spsc_policy_e e_policy)
{
        if (!IS_POWER_OF_2(cap))
                return -1;

        spsc->e_policy = e_policy;
        spsc->cap      = cap;
        ATOMIC_STORE(&spsc->rp, 0);
        ATOMIC_STORE(&spsc->wp, 0);
        return 0;
}

HAPI void
spsc_reset(spsc_t *spsc)
{
        ATOMIC_STORE(&spsc->rp, 0);
        ATOMIC_STORE(&spsc->wp, 0);
}

HAPI bool
spsc_empty(spsc_t *spsc)
{
        return spsc_avail(spsc) == 0;
}

HAPI bool
spsc_full(spsc_t *spsc)
{
        return spsc_free(spsc) == 0;
}

HAPI usz
spsc_avail(spsc_t *spsc)
{
        return ATOMIC_LOAD(&spsc->wp) - ATOMIC_LOAD(&spsc->rp);
}

HAPI usz
spsc_free(spsc_t *spsc)
{
        return spsc->cap - spsc_avail(spsc);
}

HAPI usz
spsc_policy(spsc_t *spsc, const usz wp, const usz rp, usz size)
{
        const usz free = spsc->cap - (wp - rp);
        if (size <= free)
                return size;

        switch (spsc->e_policy) {
                case SPSC_POLICY_TRUNCATE: {
                        size = free;
                        return size;
                }
                case SPSC_POLICY_OVERWRITE: {
                        atomic_fetch_add_explicit(&spsc->rp, size - free, memory_order_acq_rel);
                        return size;
                }
                case SPSC_POLICY_REJECT:
                        return 0;
        }
        return 0;
}

HAPI usz
spsc_write(spsc_t *spsc, const void *src, const usz size)
{
        return spsc_write_buf(spsc, spsc->buf, src, size);
}

HAPI usz
spsc_read(spsc_t *spsc, void *dst, const usz size)
{
        return spsc_read_buf(spsc, spsc->buf, dst, size);
}

HAPI usz
spsc_write_buf(spsc_t *spsc, void *buf, const void *src, usz size)
{
        const usz wp = ATOMIC_LOAD_EXPLICIT(&spsc->wp, memory_order_relaxed);
        const usz rp = ATOMIC_LOAD_EXPLICIT(&spsc->rp, memory_order_acquire);

        size = spsc_policy(spsc, wp, rp, size);
        if (size == 0)
                return 0;

        const usz mask  = spsc->cap - 1;
        const usz off   = wp & mask;
        const usz first = MIN(size, spsc->cap - off);
        memcpy((u8 *)buf + off, src, first);
        memcpy((u8 *)buf, (u8 *)src + first, size - first);

        ATOMIC_STORE_EXPLICIT(&spsc->wp, wp + size, memory_order_release);
        return size;
}

HAPI usz
spsc_read_buf(spsc_t *spsc, void *buf, void *dst, usz size)
{
        const usz rp = ATOMIC_LOAD_EXPLICIT(&spsc->rp, memory_order_relaxed);
        const usz wp = ATOMIC_LOAD_EXPLICIT(&spsc->wp, memory_order_acquire);

        const usz avail = wp - rp;
        if (size > avail)
                size = avail;
        if (size == 0)
                return 0;

        const usz mask  = spsc->cap - 1;
        const usz off   = rp & mask;
        const usz first = MIN(size, spsc->cap - off);
        memcpy(dst, (u8 *)buf + off, first);
        memcpy((u8 *)dst + first, (u8 *)buf, size - first);

        ATOMIC_STORE_EXPLICIT(&spsc->rp, rp + size, memory_order_release);
        return size;
}

#endif // !SPSC_H
