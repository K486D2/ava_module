#ifndef FIFO_H
#define FIFO_H

#include <stddef.h>
#ifndef __cplusplus
#include <stdatomic.h>
#define atomic_t(x) _Atomic x
#else
#include <atomic>
#define atomic_t(x)                    std::atomic<x>
#define atomic_store(a, v)             std::atomic_store(a, v)
#define atomic_load(a)                 std::atomic_load(a)
#define atomic_load_explicit(a, m)     std::atomic_load_explicit(a, m)
#define atomic_store_explicit(a, v, m) std::atomic_store_explicit(a, v, m)
#define atomic_compare_exchange_weak_explicit(a, o, n, s, f)                                       \
  std::atomic_compare_exchange_weak_explicit(a, o, n, s, f)

constexpr auto memory_order_relaxed = std::memory_order_relaxed;
constexpr auto memory_order_acquire = std::memory_order_acquire;
constexpr auto memory_order_release = std::memory_order_release;
constexpr auto memory_order_acq_rel = std::memory_order_acq_rel;
#endif

#include <pthread.h>
#include <string.h>

#include "../util/def.h"

#ifdef _MSC_VER
#include <intrin.h>
static inline u32 clzll(u64 x) {
  u32 index;
  if (_BitScanReverse64(&index, x))
    return 63 - index;
  return 64;
}
#else
static inline u32 clzll(u64 x) {
  return __builtin_clzll(x);
}
#endif

#define IS_POWER_OF_2(n)    ((n) != 0 && (((n) & ((n) - 1)) == 0))
#define ROUNDUP_POW_OF_2(n) ((n) == 0 ? 1 : (1ULL << (sizeof(n) * 8 - clzll((n) - 1))))

typedef enum {
  FIFO_POLICY_TRUNCATE,
  FIFO_POLICY_OVERWRITE,
  FIFO_POLICY_REJECT,
} fifo_policy_e;

typedef enum {
  FIFO_MODE_SPSC,
  FIFO_MODE_SPMC,
  FIFO_MODE_MPSC,
  FIFO_MODE_MPMC,
} fifo_mode_e;

typedef struct {
  fifo_policy_e e_policy;
  fifo_mode_e   e_mode;
  void         *buf;      // 缓冲区指针
  size_t        buf_size; // 缓冲区大小(2^n)
  atomic_t(size_t) in;    // 写入位置
  atomic_t(size_t) out;   // 读取位置
  atomic_t(size_t) com;   // 用于MP场景以防消费者读到未写完的数据
  pthread_mutex_t lock;
} fifo_t;

static inline void fifo_reset(fifo_t *fifo) {
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
  atomic_store(&fifo->com, 0);
}

static inline size_t fifo_get_avail(fifo_t *fifo) {
  return atomic_load(&fifo->in) - atomic_load(&fifo->out);
}

static inline size_t fifo_get_free(fifo_t *fifo) {
  return fifo->buf_size - fifo_get_avail(fifo);
}

static inline bool fifo_is_empty(fifo_t *fifo) {
  return fifo_get_avail(fifo) == 0;
}

static inline bool fifo_is_full(fifo_t *fifo) {
  return fifo_get_free(fifo) == 0;
}

static inline int
fifo_buf_init(fifo_t *fifo, size_t buf_size, fifo_mode_e e_mode, fifo_policy_e e_policy) {
  if (!IS_POWER_OF_2(buf_size))
    return -1;

  fifo->e_mode   = e_mode;
  fifo->e_policy = e_policy;
  fifo->buf_size = buf_size;
  pthread_mutex_init(&fifo->lock, NULL);
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
  atomic_store(&fifo->com, 0);
  return 0;
}

static inline int
fifo_init(fifo_t *fifo, void *buf, size_t buf_size, fifo_mode_e e_mode, fifo_policy_e e_policy) {
  if (!IS_POWER_OF_2(buf_size))
    return -1;

  fifo->buf = buf;
  fifo_buf_init(fifo, buf_size, e_mode, e_policy);
  return 0;
}

static inline size_t
fifo_policy(fifo_t *fifo, size_t *data_size, size_t in, size_t out, size_t buf_size) {
  size_t free = buf_size - (in - out);
  if (*data_size <= free)
    return *data_size;

  switch (fifo->e_policy) {
  case FIFO_POLICY_TRUNCATE: {
    *data_size = free;
    return *data_size;
  }
  case FIFO_POLICY_OVERWRITE: {
    size_t delta = *data_size - free;
    atomic_fetch_add_explicit(&fifo->out, delta, memory_order_acq_rel);
    return *data_size;
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
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_relaxed);
  size_t out = atomic_load_explicit(&fifo->out, memory_order_acquire);

  data_size = fifo_policy(fifo, &data_size, in, out, fifo->buf_size);
  if (data_size == 0)
    return 0;

  size_t offset = in & (fifo->buf_size - 1);
  size_t size   = MIN(data_size, fifo->buf_size - offset);
  memcpy((u8 *)buf + offset, data, size);
  memcpy((u8 *)buf, (u8 *)data + size, data_size - size);

  atomic_store_explicit(&fifo->in, in + data_size, memory_order_release);
  return data_size;
}

static inline size_t fifo_spsc_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t out = atomic_load_explicit(&fifo->out, memory_order_relaxed);
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_acquire);

  size_t avail = in - out;
  if (data_size > avail)
    data_size = avail;
  if (data_size == 0)
    return 0;

  size_t offset = out & (fifo->buf_size - 1);
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
  size_t size = fifo_spsc_in(fifo, buf, data, data_size);
  return size;
}

static inline size_t fifo_spmc_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  pthread_mutex_lock(&fifo->lock);
  size_t size = fifo_spsc_out(fifo, buf, data, data_size);
  pthread_mutex_unlock(&fifo->lock);
  return size;
}

/* -------------------------------------------------------------------------- */
/*                                    MPSC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_mpsc_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  pthread_mutex_lock(&fifo->lock);
  size_t size = fifo_spsc_in(fifo, buf, data, data_size);
  pthread_mutex_unlock(&fifo->lock);
  return size;
}

static inline size_t fifo_mpsc_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t size = fifo_spsc_out(fifo, buf, data, data_size);
  return size;
}

/* -------------------------------------------------------------------------- */
/*                                    MPMC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_mpmc_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  pthread_mutex_lock(&fifo->lock);
  size_t size = fifo_spsc_in(fifo, buf, data, data_size);
  pthread_mutex_unlock(&fifo->lock);
  return size;
}

static inline size_t fifo_mpmc_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  pthread_mutex_lock(&fifo->lock);
  size_t size = fifo_spsc_out(fifo, buf, data, data_size);
  pthread_mutex_unlock(&fifo->lock);
  return size;
}

static inline size_t fifo_in(fifo_t *fifo, const void *data, size_t data_size) {
  switch (fifo->e_mode) {
  case FIFO_MODE_SPSC: {
    return fifo_spsc_in(fifo, fifo->buf, data, data_size);
  }
  case FIFO_MODE_SPMC: {
    return fifo_spmc_in(fifo, fifo->buf, data, data_size);
  }
  case FIFO_MODE_MPSC: {
    return fifo_mpsc_in(fifo, fifo->buf, data, data_size);
  }
  case FIFO_MODE_MPMC: {
    return fifo_mpmc_in(fifo, fifo->buf, data, data_size);
  }
  }
  return 0;
}

static inline size_t fifo_out(fifo_t *fifo, void *data, size_t data_size) {
  switch (fifo->e_mode) {
  case FIFO_MODE_SPSC: {
    return fifo_spsc_out(fifo, fifo->buf, data, data_size);
  }
  case FIFO_MODE_SPMC: {
    return fifo_spmc_out(fifo, fifo->buf, data, data_size);
  }
  case FIFO_MODE_MPSC: {
    return fifo_mpsc_out(fifo, fifo->buf, data, data_size);
  }
  case FIFO_MODE_MPMC: {
    return fifo_mpmc_out(fifo, fifo->buf, data, data_size);
  }
  }
  return 0;
}

static inline size_t fifo_buf_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  switch (fifo->e_mode) {
  case FIFO_MODE_SPSC: {
    return fifo_spsc_in(fifo, buf, data, data_size);
  }
  case FIFO_MODE_SPMC: {
    return fifo_spmc_in(fifo, buf, data, data_size);
  }
  case FIFO_MODE_MPSC: {
    return fifo_mpsc_in(fifo, buf, data, data_size);
  }
  case FIFO_MODE_MPMC: {
    return fifo_mpmc_in(fifo, buf, data, data_size);
  }
  }
  return 0;
}

static inline size_t fifo_buf_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  switch (fifo->e_mode) {
  case FIFO_MODE_SPSC: {
    return fifo_spsc_out(fifo, buf, data, data_size);
  }
  case FIFO_MODE_SPMC: {
    return fifo_spmc_out(fifo, buf, data, data_size);
  }
  case FIFO_MODE_MPSC: {
    return fifo_mpsc_out(fifo, buf, data, data_size);
  }
  case FIFO_MODE_MPMC: {
    return fifo_mpmc_out(fifo, buf, data, data_size);
  }
  }
  return 0;
}

#endif // !FIFO_H
