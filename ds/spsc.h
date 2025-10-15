#ifndef SPSC_H
#define SPSC_H

#include <string.h>

#include "util/mathdef.h"
#include "util/typedef.h"

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

static inline int  spsc_init(spsc_t *spsc, void *buf, usz cap, spsc_policy_e e_policy);
static inline int  spsc_init_buf(spsc_t *spsc, usz cap, spsc_policy_e e_policy);
static inline void spsc_reset(spsc_t *spsc);
static inline bool spsc_empty(spsc_t *spsc);
static inline bool spsc_full(spsc_t *spsc);
static inline usz  spsc_avail(spsc_t *spsc);
static inline usz  spsc_free(spsc_t *spsc);
static inline usz  spsc_policy(spsc_t *spsc, usz wp, usz rp, usz nbytes);

static inline usz spsc_write(spsc_t *spsc, const void *src, usz nbytes);
static inline usz spsc_read(spsc_t *spsc, void *dst, usz nbytes);
static inline usz spsc_write_buf(spsc_t *spsc, void *buf, const void *src, usz nbytes);
static inline usz spsc_read_buf(spsc_t *spsc, void *buf, void *dst, usz nbytes);

/* -------------------------------------------------------------------------- */
/*                                     API                                    */
/* -------------------------------------------------------------------------- */

static inline int spsc_init(spsc_t *spsc, void *buf, usz cap, spsc_policy_e e_policy) {
        int ret = spsc_init_buf(spsc, cap, e_policy);
        if (ret != 0)
                return ret;

        spsc->buf = buf;
        return 0;
}

static inline int spsc_init_buf(spsc_t *spsc, usz cap, spsc_policy_e e_policy) {
        if (!IS_POWER_OF_2(cap))
                return -1;

        spsc->e_policy = e_policy;
        spsc->cap      = cap;
        ATOMIC_STORE(&spsc->rp, 0);
        ATOMIC_STORE(&spsc->wp, 0);
        return 0;
}

static inline void spsc_reset(spsc_t *spsc) {
        ATOMIC_STORE(&spsc->rp, 0);
        ATOMIC_STORE(&spsc->wp, 0);
}

static inline bool spsc_empty(spsc_t *spsc) { return spsc_avail(spsc) == 0; }

static inline bool spsc_full(spsc_t *spsc) { return spsc_free(spsc) == 0; }

static inline usz spsc_avail(spsc_t *spsc) {
        return ATOMIC_LOAD(&spsc->wp) - ATOMIC_LOAD(&spsc->rp);
}

static inline usz spsc_free(spsc_t *spsc) { return spsc->cap - spsc_avail(spsc); }

static inline usz spsc_policy(spsc_t *spsc, usz wp, usz rp, usz nbytes) {
        usz free = spsc->cap - (wp - rp);
        if (nbytes <= free)
                return nbytes;

        switch (spsc->e_policy) {
        case SPSC_POLICY_TRUNCATE: {
                nbytes = free;
                return nbytes;
        }
        case SPSC_POLICY_OVERWRITE: {
                atomic_fetch_add_explicit(&spsc->rp, nbytes - free, memory_order_acq_rel);
                return nbytes;
        }
        case SPSC_POLICY_REJECT:
                return 0;
        }
        return 0;
}

static inline usz spsc_write(spsc_t *spsc, const void *src, usz nbytes) {
        return spsc_write_buf(spsc, spsc->buf, src, nbytes);
}

static inline usz spsc_read(spsc_t *spsc, void *dst, usz nbytes) {
        return spsc_read_buf(spsc, spsc->buf, dst, nbytes);
}

static inline usz spsc_write_buf(spsc_t *spsc, void *buf, const void *src, usz nbytes) {
        usz wp = ATOMIC_LOAD_EXPLICIT(&spsc->wp, memory_order_relaxed);
        usz rp = ATOMIC_LOAD_EXPLICIT(&spsc->rp, memory_order_acquire);

        nbytes = spsc_policy(spsc, wp, rp, nbytes);
        if (nbytes == 0)
                return 0;

        usz mask  = spsc->cap - 1;
        usz off   = wp & mask;
        usz first = MIN(nbytes, spsc->cap - off);
        memcpy((u8 *)buf + off, src, first);
        memcpy((u8 *)buf, (u8 *)src + first, nbytes - first);

        ATOMIC_STORE_EXPLICIT(&spsc->wp, wp + nbytes, memory_order_release);
        return nbytes;
}

static inline usz spsc_read_buf(spsc_t *spsc, void *buf, void *dst, usz nbytes) {
        usz rp = ATOMIC_LOAD_EXPLICIT(&spsc->rp, memory_order_relaxed);
        usz wp = ATOMIC_LOAD_EXPLICIT(&spsc->wp, memory_order_acquire);

        usz avail = wp - rp;
        if (nbytes > avail)
                nbytes = avail;
        if (nbytes == 0)
                return 0;

        usz mask  = spsc->cap - 1;
        usz off   = rp & mask;
        usz first = MIN(nbytes, spsc->cap - off);
        memcpy(dst, (u8 *)buf + off, first);
        memcpy((u8 *)dst + first, (u8 *)buf, nbytes - first);

        ATOMIC_STORE_EXPLICIT(&spsc->rp, rp + nbytes, memory_order_release);
        return nbytes;
}

#endif // !SPSC_H
