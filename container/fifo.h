#ifndef FIFO_H
#define FIFO_H

#include <pthread.h>
#include <string.h>

#include "../util/def.h"

typedef enum {
  FIFO_MODE_SPSC, // 单生产者单消费者
  FIFO_MODE_SPMC, // 单生产者多消费者
  FIFO_MODE_MPSC, // 多生产者单消费者
  FIFO_MODE_MPMC, // 多生产者多消费者
} fifo_mode_e;

/* 写入数据超过剩余空间时的处理策略 */
typedef enum {
  FIFO_POLICY_TRUNCATE,  // 截断
  FIFO_POLICY_OVERWRITE, // 覆盖
  FIFO_POLICY_REJECT,    // 拒绝
} fifo_policy_e;

typedef struct {
  atomic_t(size_t) seq;
  u8 data;
} fifo_node_t;

typedef struct {
  fifo_mode_e   e_mode;   // FIFO模式
  fifo_policy_e e_policy; // 写入数据超过剩余空间时的处理策略
  atomic_t(size_t) in;    // 写入位置
  atomic_t(size_t) out;   // 读取位置
  void  *buf;             // 缓冲区
  size_t buf_size;        // 缓冲区大小(2^n)
} fifo_t;

static inline void fifo_reset(fifo_t *fifo) {
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
}

static inline size_t fifo_avail(fifo_t *fifo) {
  return atomic_load(&fifo->in) - atomic_load(&fifo->out);
}

static inline size_t fifo_free(fifo_t *fifo) {
  return fifo->buf_size - fifo_avail(fifo);
}

static inline bool fifo_empty(fifo_t *fifo) {
  return fifo_avail(fifo) == 0;
}

static inline bool fifo_full(fifo_t *fifo) {
  return fifo_free(fifo) == 0;
}

static inline int
fifo_init_buf(fifo_t *fifo, size_t buf_size, fifo_mode_e e_mode, fifo_policy_e e_policy) {
  if (!IS_POWER_OF_2(buf_size))
    return -1;

  fifo->e_mode   = e_mode;
  fifo->e_policy = e_policy;
  fifo->buf_size = buf_size;
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);

  return 0;
}

static inline int
fifo_init(fifo_t *fifo, void *buf, size_t buf_size, fifo_mode_e e_mode, fifo_policy_e e_policy) {
  if (!IS_POWER_OF_2(buf_size))
    return -1;

  fifo->buf = buf;
  fifo_init_buf(fifo, buf_size, e_mode, e_policy);

  for (size_t i = 0; i < buf_size; i++)
    atomic_store(&((fifo_node_t *)buf)[i].seq, i);

  return 0;
}

static inline size_t fifo_policy(fifo_t *fifo, size_t data_size, size_t in, size_t out) {
  size_t free = fifo->buf_size - (in - out);
  if (data_size <= free)
    return data_size;

  switch (fifo->e_policy) {
  case FIFO_POLICY_TRUNCATE: {
    data_size = free;
    return data_size;
  }
  case FIFO_POLICY_OVERWRITE: {
    size_t delta = data_size - free;
    atomic_fetch_add_explicit(&fifo->out, delta, memory_order_acq_rel);
    return data_size;
  }
  case FIFO_POLICY_REJECT: {
    return 0;
  }
  }
  return 0;
}

/* -------------------------------------------------------------------------- */
/*                                    SPSC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_spsc_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  size_t mask = fifo->buf_size - 1;
  size_t in   = atomic_load_explicit(&fifo->in, memory_order_relaxed);
  size_t out  = atomic_load_explicit(&fifo->out, memory_order_acquire);

  data_size = fifo_policy(fifo, data_size, in, out);
  if (data_size == 0)
    return 0;

  size_t offset = in & mask;
  size_t size   = MIN(data_size, fifo->buf_size - offset);
  memcpy((u8 *)buf + offset, data, size);
  memcpy((u8 *)buf, (u8 *)data + size, data_size - size);

  atomic_store_explicit(&fifo->in, in + data_size, memory_order_release);
  return data_size;
}

static inline size_t fifo_spsc_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t mask = fifo->buf_size - 1;
  size_t out  = atomic_load_explicit(&fifo->out, memory_order_relaxed);
  size_t in   = atomic_load_explicit(&fifo->in, memory_order_acquire);

  size_t avail = in - out;
  if (data_size > avail)
    data_size = avail;
  if (data_size == 0)
    return 0;

  size_t offset = out & mask;
  size_t size   = MIN(data_size, fifo->buf_size - offset);
  memcpy(data, (u8 *)buf + offset, size);
  memcpy((u8 *)data + size, (u8 *)buf, data_size - size);

  atomic_store_explicit(&fifo->out, out + data_size, memory_order_release);
  return data_size;
}

/* -------------------------------------------------------------------------- */
/*                                    SPMC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_spmc_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  return fifo_spsc_in(fifo, buf, data, data_size);
}

static inline size_t fifo_spmc_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t mask = fifo->buf_size - 1;
  while (true) {
    size_t       out  = atomic_load_explicit(&fifo->out, memory_order_relaxed);
    fifo_node_t *node = &((fifo_node_t *)buf)[out & mask];
    size_t       seq  = atomic_load_explicit(&node->seq, memory_order_acquire);
    intptr_t     diff = (intptr_t)seq - (intptr_t)(out + 1);

    if (diff == 0) {
      if (atomic_compare_exchange_weak_explicit(
              &fifo->out, &out, out + 1, memory_order_acq_rel, memory_order_relaxed)) {
        memcpy(data, &node->data, data_size);
        atomic_store_explicit(&node->seq, out + fifo->buf_size, memory_order_release);
        return data_size;
      }
    } else if (diff < 0)
      return 0;
  }
}

/* -------------------------------------------------------------------------- */
/*                                    MPSC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_mpsc_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  size_t mask = fifo->buf_size - 1;
  while (true) {
    size_t       in   = atomic_load_explicit(&fifo->in, memory_order_relaxed);
    fifo_node_t *node = &((fifo_node_t *)buf)[in & mask];
    size_t       seq  = atomic_load_explicit(&node->seq, memory_order_acquire);
    intptr_t     diff = (intptr_t)seq - (intptr_t)in;

    if (diff == 0) {
      if (atomic_compare_exchange_weak_explicit(
              &fifo->in, &in, in + 1, memory_order_acq_rel, memory_order_relaxed)) {
        memcpy(&node->data, data, data_size);
        atomic_store_explicit(&node->seq, in + 1, memory_order_release);
        return data_size;
      }
    } else if (diff < 0)
      return 0;
  }
}

static inline size_t fifo_mpsc_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  return fifo_spsc_out(fifo, buf, data, data_size);
}

/* -------------------------------------------------------------------------- */
/*                                    MPMC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_mpmc_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  size_t mask = fifo->buf_size - 1;
  while (true) {
    size_t       in   = atomic_load_explicit(&fifo->in, memory_order_relaxed);
    fifo_node_t *node = &((fifo_node_t *)buf)[in & mask];
    size_t       seq  = atomic_load_explicit(&node->seq, memory_order_acquire);
    intptr_t     diff = (intptr_t)seq - (intptr_t)in;

    if (diff == 0) {
      if (atomic_compare_exchange_weak_explicit(
              &fifo->in, &in, in + 1, memory_order_acq_rel, memory_order_relaxed)) {
        memcpy(&node->data, data, data_size);
        atomic_store_explicit(&node->seq, in + 1, memory_order_release);
        return data_size;
      }
    } else if (diff < 0)
      return 0;
  }
}

static inline size_t fifo_mpmc_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t mask = fifo->buf_size - 1;
  while (true) {
    size_t       out  = atomic_load_explicit(&fifo->out, memory_order_relaxed);
    fifo_node_t *node = &((fifo_node_t *)buf)[out & mask];
    size_t       seq  = atomic_load_explicit(&node->seq, memory_order_acquire);
    intptr_t     diff = (intptr_t)seq - (intptr_t)(out + 1);

    if (diff == 0) {
      if (atomic_compare_exchange_weak_explicit(
              &fifo->out, &out, out + 1, memory_order_acq_rel, memory_order_relaxed)) {
        memcpy(data, &node->data, data_size);
        atomic_store_explicit(&node->seq, out + fifo->buf_size, memory_order_release);
        return data_size;
      }
    } else if (diff < 0)
      return 0;
  }
}

/* -------------------------------------------------------------------------- */
/*                                     API                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_in_buf(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  switch (fifo->e_mode) {
  case FIFO_MODE_SPSC:
    return fifo_spsc_in(fifo, buf, data, data_size);
  case FIFO_MODE_SPMC:
    return fifo_spmc_in(fifo, buf, data, data_size);
  case FIFO_MODE_MPSC:
    return fifo_mpsc_in(fifo, buf, data, data_size);
  case FIFO_MODE_MPMC:
    return fifo_mpmc_in(fifo, buf, data, data_size);
  }
  return 0;
}

static inline size_t fifo_out_buf(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  switch (fifo->e_mode) {
  case FIFO_MODE_SPSC:
    return fifo_spsc_out(fifo, buf, data, data_size);
  case FIFO_MODE_SPMC:
    return fifo_spmc_out(fifo, buf, data, data_size);
  case FIFO_MODE_MPSC:
    return fifo_mpsc_out(fifo, buf, data, data_size);
  case FIFO_MODE_MPMC:
    return fifo_mpmc_out(fifo, buf, data, data_size);
  }
  return 0;
}

static inline size_t fifo_in(fifo_t *fifo, const void *data, size_t data_size) {
  return fifo_in_buf(fifo, fifo->buf, data, data_size);
}

static inline size_t fifo_out(fifo_t *fifo, void *data, size_t data_size) {
  return fifo_out_buf(fifo, fifo->buf, data, data_size);
}

#endif // !FIFO_H
