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
  size_t        cap;      // 缓冲区容量(2^n)
  atomic_t(size_t) wp;    // 写入位置
  atomic_t(size_t) rp;    // 读取位置
} spsc_t;

static inline int    spsc_init(spsc_t *spsc, void *buf, size_t cap, spsc_policy_e e_policy);
static inline int    spsc_init_buf(spsc_t *spsc, size_t cap, spsc_policy_e e_policy);
static inline void   spsc_reset(spsc_t *spsc);
static inline bool   spsc_empty(spsc_t *spsc);
static inline bool   spsc_full(spsc_t *spsc);
static inline size_t spsc_avail(spsc_t *spsc);
static inline size_t spsc_free(spsc_t *spsc);
static inline size_t spsc_policy(spsc_t *spsc, size_t wp, size_t rp, size_t size);

static inline size_t spsc_push(spsc_t *spsc, const void *data, size_t size);
static inline size_t spsc_pop(spsc_t *spsc, void *data, size_t size);
static inline size_t spsc_push_buf(spsc_t *spsc, void *buf, const void *data, size_t size);
static inline size_t spsc_pop_buf(spsc_t *spsc, void *buf, void *data, size_t size);

/* -------------------------------------------------------------------------- */
/*                                     API                                    */
/* -------------------------------------------------------------------------- */

static inline int spsc_init(spsc_t *spsc, void *buf, size_t cap, spsc_policy_e e_policy) {
  int ret = spsc_init_buf(spsc, cap, e_policy);
  if (ret != 0)
    return ret;

  spsc->buf = buf;
  return 0;
}

static inline int spsc_init_buf(spsc_t *spsc, size_t cap, spsc_policy_e e_policy) {
  if (!IS_POWER_OF_2(cap))
    return -1;

  spsc->e_policy = e_policy;
  spsc->cap      = cap;
  atomic_store(&spsc->rp, 0);
  atomic_store(&spsc->wp, 0);
  return 0;
}

static inline void spsc_reset(spsc_t *spsc) {
  atomic_store(&spsc->rp, 0);
  atomic_store(&spsc->wp, 0);
}

static inline bool spsc_empty(spsc_t *spsc) {
  return spsc_avail(spsc) == 0;
}

static inline bool spsc_full(spsc_t *spsc) {
  return spsc_free(spsc) == 0;
}

static inline size_t spsc_avail(spsc_t *spsc) {
  return atomic_load(&spsc->wp) - atomic_load(&spsc->rp);
}

static inline size_t spsc_free(spsc_t *spsc) {
  return spsc->cap - spsc_avail(spsc);
}

static inline size_t spsc_policy(spsc_t *spsc, size_t wp, size_t rp, size_t size) {
  size_t free = spsc->cap - (wp - rp);
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

static inline size_t spsc_push(spsc_t *spsc, const void *data, size_t size) {
  return spsc_push_buf(spsc, spsc->buf, data, size);
}

static inline size_t spsc_pop(spsc_t *spsc, void *data, size_t size) {
  return spsc_pop_buf(spsc, spsc->buf, data, size);
}

static inline size_t spsc_push_buf(spsc_t *spsc, void *buf, const void *data, size_t size) {
  size_t wp = atomic_load_explicit(&spsc->wp, memory_order_relaxed);
  size_t rp = atomic_load_explicit(&spsc->rp, memory_order_acquire);

  size = spsc_policy(spsc, wp, rp, size);
  if (size == 0)
    return 0;

  size_t mask   = spsc->cap - 1;
  size_t offset = wp & mask;
  size_t first  = MIN(size, spsc->cap - offset);
  memcpy((u8 *)buf + offset, data, first);
  memcpy((u8 *)buf, (u8 *)data + first, size - first);

  atomic_store_explicit(&spsc->wp, wp + size, memory_order_release);
  return size;
}

static inline size_t spsc_pop_buf(spsc_t *spsc, void *buf, void *data, size_t size) {
  size_t rp = atomic_load_explicit(&spsc->rp, memory_order_relaxed);
  size_t wp = atomic_load_explicit(&spsc->wp, memory_order_acquire);

  size_t avail = wp - rp;
  if (size > avail)
    size = avail;
  if (size == 0)
    return 0;

  size_t mask   = spsc->cap - 1;
  size_t offset = rp & mask;
  size_t first  = MIN(size, spsc->cap - offset);
  memcpy(data, (u8 *)buf + offset, first);
  memcpy((u8 *)data + first, (u8 *)buf, size - first);

  atomic_store_explicit(&spsc->rp, rp + size, memory_order_release);
  return size;
}

#endif // !SPSC_H
