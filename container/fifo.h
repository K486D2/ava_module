#ifndef FIFO_H
#define FIFO_H

#include <string.h>

#include "util/mathdef.h"
#include "util/typedef.h"

/* 写入数据超过剩余空间时的处理策略 */
typedef enum {
  FIFO_POLICY_TRUNCATE,  // 截断
  FIFO_POLICY_OVERWRITE, // 覆盖
  FIFO_POLICY_REJECT,    // 拒绝
} fifo_policy_e;

typedef struct {
  fifo_policy_e e_policy; // 写入数据超过剩余空间时的处理策略
  void  *buf;             // 缓冲区
  size_t cap;             // 缓冲区容量(2^n)
  atomic_t(size_t) rp;  // 读取位置
  atomic_t(size_t) wp;  // 写入位置
} fifo_t;

static inline int    fifo_init(fifo_t *fifo, void *buf, size_t cap, fifo_policy_e e_policy);
static inline int    fifo_init_buf(fifo_t *fifo, size_t cap, fifo_policy_e e_policy);
static inline void   fifo_reset(fifo_t *fifo);
static inline bool   fifo_empty(fifo_t *fifo);
static inline bool   fifo_full(fifo_t *fifo);
static inline size_t fifo_avail(fifo_t *fifo);
static inline size_t fifo_free(fifo_t *fifo);
static inline size_t fifo_policy(fifo_t *fifo, size_t wp, size_t rp, size_t size);

static inline size_t fifo_write(fifo_t *fifo, const void *data, size_t size);
static inline size_t fifo_read(fifo_t *fifo, void *data, size_t size);
static inline size_t fifo_write_buf(fifo_t *fifo, void *buf, const void *data, size_t size);
static inline size_t fifo_read_buf(fifo_t *fifo, void *buf, void *data, size_t size);

/* -------------------------------------------------------------------------- */
/*                                     API                                    */
/* -------------------------------------------------------------------------- */

static inline int fifo_init(fifo_t *fifo, void *buf, size_t cap, fifo_policy_e e_policy) {
  int ret = fifo_init_buf(fifo, cap, e_policy);
  if (ret != 0)
    return ret;

  fifo->buf = buf;
  return 0;
}

static inline int fifo_init_buf(fifo_t *fifo, size_t cap, fifo_policy_e e_policy) {
  if (!IS_POWER_OF_2(cap))
    return -1;

  fifo->e_policy = e_policy;
  fifo->cap      = cap;
  atomic_store(&fifo->rp, 0);
  atomic_store(&fifo->wp, 0);
  return 0;
}

static inline void fifo_reset(fifo_t *fifo) {
  atomic_store(&fifo->rp, 0);
  atomic_store(&fifo->wp, 0);
}

static inline bool fifo_empty(fifo_t *fifo) {
  return fifo_avail(fifo) == 0;
}

static inline bool fifo_full(fifo_t *fifo) {
  return fifo_free(fifo) == 0;
}

static inline size_t fifo_avail(fifo_t *fifo) {
  return atomic_load(&fifo->wp) - atomic_load(&fifo->rp);
}

static inline size_t fifo_free(fifo_t *fifo) {
  return fifo->cap - fifo_avail(fifo);
}

static inline size_t fifo_policy(fifo_t *fifo, size_t wp, size_t rp, size_t size) {
  size_t free = fifo->cap - (wp - rp);
  if (size <= free)
    return size;

  switch (fifo->e_policy) {
  case FIFO_POLICY_TRUNCATE: {
    size = free;
    return size;
  }
  case FIFO_POLICY_OVERWRITE: {
    atomic_fetch_add_explicit(&fifo->rp, size - free, memory_order_acq_rel);
    return size;
  }
  case FIFO_POLICY_REJECT:
    return 0;
  }
  return 0;
}

static inline size_t fifo_write(fifo_t *fifo, const void *data, size_t size) {
  return fifo_write_buf(fifo, fifo->buf, data, size);
}

static inline size_t fifo_read(fifo_t *fifo, void *data, size_t size) {
  return fifo_read_buf(fifo, fifo->buf, data, size);
}

static inline size_t fifo_write_buf(fifo_t *fifo, void *buf, const void *data, size_t size) {
  size_t mask = fifo->cap - 1;
  size_t wp = atomic_load_explicit(&fifo->wp, memory_order_relaxed);
  size_t rp = atomic_load_explicit(&fifo->rp, memory_order_acquire);

  size = fifo_policy(fifo, wp, rp, size);
  if (size == 0)
    return 0;

  size_t offset = wp & mask;
  size_t first  = MIN(size, fifo->cap - offset);
  memcpy((u8 *)buf + offset, data, first);
  memcpy((u8 *)buf, (u8 *)data + first, size - first);

  atomic_store_explicit(&fifo->wp, wp + size, memory_order_release);
  return size;
}

static inline size_t fifo_read_buf(fifo_t *fifo, void *buf, void *data, size_t size) {
  size_t mask = fifo->cap - 1;
  size_t rp = atomic_load_explicit(&fifo->rp, memory_order_relaxed);
  size_t wp = atomic_load_explicit(&fifo->wp, memory_order_acquire);

  size_t avail = wp - rp;
  if (size > avail)
    size = avail;
  if (size == 0)
    return 0;

  size_t offset = rp & mask;
  size_t first  = MIN(size, fifo->cap - offset);
  memcpy(data, (u8 *)buf + offset, first);
  memcpy((u8 *)data + first, (u8 *)buf, size - first);

  atomic_store_explicit(&fifo->rp, rp + size, memory_order_release);
  return size;
}

#endif // !FIFO_H
