#ifndef FIFO_H
#define FIFO_H

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

#include <string.h>

#include "../util/def.h"

#ifdef _MSC_VER
#include <intrin.h>
static inline unsigned int clzll(unsigned long long x) {
  unsigned long index;
  if (_BitScanReverse64(&index, x))
    return 63 - index;
  return 64;
}
#else
static inline unsigned int clzll(unsigned long long x) {
  return __builtin_clzll(x);
}
#endif

#define IS_POWER_OF_2(n)    ((n) != 0 && (((n) & ((n) - 1)) == 0))
#define ROUNDUP_POW_OF_2(n) ((n) == 0 ? 1 : (1ULL << (sizeof(n) * 8 - clzll((n) - 1))))

typedef struct {
  void  *buf;           // 缓冲区指针
  size_t buf_size;      // 缓冲区大小(必须是2的幂)
  atomic_t(size_t) in;  // 写入位置
  atomic_t(size_t) out; // 读取位置
} fifo_t;

static inline void fifo_reset(fifo_t *fifo) {
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
}

static inline size_t fifo_len(fifo_t *fifo) {
  return atomic_load(&fifo->in) - atomic_load(&fifo->out);
}

static inline size_t fifo_avail(fifo_t *fifo) {
  return fifo->buf_size - fifo_len(fifo);
}

static inline bool fifo_is_empty(fifo_t *fifo) {
  return fifo_len(fifo) == 0;
}

static inline bool fifo_is_full(fifo_t *fifo) {
  return fifo_len(fifo) == fifo->buf_size;
}

static inline void fifo_init(fifo_t *fifo, void *buf, size_t buf_size) {
  if (!IS_POWER_OF_2(buf_size))
    buf_size = ROUNDUP_POW_OF_2(buf_size);

  fifo->buf      = buf;
  fifo->buf_size = buf_size;
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
}

static inline void fifo_buf_init(fifo_t *fifo, size_t buf_size) {
  if (!IS_POWER_OF_2(buf_size))
    buf_size = ROUNDUP_POW_OF_2(buf_size);

  fifo->buf_size = buf_size;
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
}

/* -------------------------------------------------------------------------- */
/*                                    SPSC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_spsc_in(fifo_t *fifo, const void *data, size_t data_size) {
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_relaxed);
  size_t out = atomic_load_explicit(&fifo->out, memory_order_acquire);

  size_t free = fifo->buf_size - (in - out);
  if (data_size > free)
    data_size = free;
  if (data_size == 0)
    return 0;

  size_t off = in & (fifo->buf_size - 1);
  size_t len = MIN(data_size, fifo->buf_size - off);

  memcpy((u8 *)fifo->buf + off, data, len);
  memcpy((u8 *)fifo->buf, (u8 *)data + len, data_size - len);

  atomic_store_explicit(&fifo->in, in + data_size, memory_order_release);

  return data_size;
}

static inline size_t fifo_spsc_out(fifo_t *fifo, void *data, size_t data_size) {
  size_t out = atomic_load_explicit(&fifo->out, memory_order_relaxed);
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_acquire);

  size_t avail = in - out;
  if (data_size > avail)
    data_size = avail;
  if (data_size == 0)
    return 0;

  size_t off = out & (fifo->buf_size - 1);
  size_t len = MIN(data_size, fifo->buf_size - off);

  memcpy(data, (u8 *)fifo->buf + off, len);
  memcpy((u8 *)data + len, (u8 *)fifo->buf, data_size - len);

  atomic_store_explicit(&fifo->out, out + data_size, memory_order_release);

  return data_size;
}

static inline size_t fifo_spsc_buf_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_relaxed);
  size_t out = atomic_load_explicit(&fifo->out, memory_order_acquire);

  size_t free = fifo->buf_size - (in - out);
  if (data_size > free)
    data_size = free;
  if (data_size == 0)
    return 0;

  size_t off = in & (fifo->buf_size - 1);
  size_t len = MIN(data_size, fifo->buf_size - off);

  memcpy((u8 *)buf + off, data, len);
  memcpy((u8 *)buf, (u8 *)data + len, data_size - len);

  atomic_store_explicit(&fifo->in, in + data_size, memory_order_release);

  return data_size;
}

static inline size_t fifo_spsc_buf_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t out = atomic_load_explicit(&fifo->out, memory_order_relaxed);
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_acquire);

  size_t avail = in - out;
  if (data_size > avail)
    data_size = avail;
  if (data_size == 0)
    return 0;

  size_t off = out & (fifo->buf_size - 1);
  size_t len = MIN(data_size, fifo->buf_size - off);

  memcpy(data, (u8 *)buf + off, len);
  memcpy((u8 *)data + len, (u8 *)buf, data_size - len);

  atomic_store_explicit(&fifo->out, out + data_size, memory_order_release);

  return data_size;
}

/* -------------------------------------------------------------------------- */
/*                                    MPMC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_mpmc_in(fifo_t *fifo, const void *data, size_t data_size) {
  size_t pos, newpos;
  do {
    pos         = atomic_load_explicit(&fifo->in, memory_order_acquire);
    size_t free = fifo->buf_size - (pos - atomic_load_explicit(&fifo->out, memory_order_acquire));
    if (data_size > free)
      data_size = free;
    if (data_size == 0)
      return 0;

    newpos = pos + data_size;
  } while (!atomic_compare_exchange_weak_explicit(
      &fifo->in, &pos, newpos, memory_order_acq_rel, memory_order_acquire));

  size_t off = pos & (fifo->buf_size - 1);
  size_t len = MIN(data_size, fifo->buf_size - off);
  memcpy((u8 *)fifo->buf + off, data, len);
  memcpy((u8 *)fifo->buf, (u8 *)data + len, data_size - len);

  return data_size;
}

static inline size_t fifo_mpmc_out(fifo_t *fifo, void *data, size_t data_size) {
  size_t pos, newpos;
  do {
    pos          = atomic_load_explicit(&fifo->out, memory_order_acquire);
    size_t avail = atomic_load_explicit(&fifo->in, memory_order_acquire) - pos;
    if (data_size > avail)
      data_size = avail;
    if (data_size == 0)
      return 0;

    newpos = pos + data_size;
  } while (!atomic_compare_exchange_weak_explicit(
      &fifo->out, &pos, newpos, memory_order_acq_rel, memory_order_acquire));

  size_t off = pos & (fifo->buf_size - 1);
  size_t len = MIN(data_size, fifo->buf_size - off);
  memcpy(data, (u8 *)fifo->buf + off, len);
  memcpy((u8 *)data + len, (u8 *)fifo->buf, data_size - len);

  return data_size;
}

static inline size_t fifo_mpmc_buf_in(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  size_t pos, newpos;
  do {
    pos         = atomic_load_explicit(&fifo->in, memory_order_acquire);
    size_t free = fifo->buf_size - (pos - atomic_load_explicit(&fifo->out, memory_order_acquire));
    if (data_size > free)
      data_size = free;
    if (data_size == 0)
      return 0;

    newpos = pos + data_size;
  } while (!atomic_compare_exchange_weak_explicit(
      &fifo->in, &pos, newpos, memory_order_acq_rel, memory_order_acquire));

  size_t off = pos & (fifo->buf_size - 1);
  size_t len = MIN(data_size, fifo->buf_size - off);
  memcpy((u8 *)buf + off, data, len);
  memcpy((u8 *)buf, (u8 *)data + len, data_size - len);

  return data_size;
}

static inline size_t fifo_mpmc_buf_out(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t pos, newpos;
  do {
    pos          = atomic_load_explicit(&fifo->out, memory_order_acquire);
    size_t avail = atomic_load_explicit(&fifo->in, memory_order_acquire) - pos;
    if (data_size > avail)
      data_size = avail;
    if (data_size == 0)
      return 0;

    newpos = pos + data_size;
  } while (!atomic_compare_exchange_weak_explicit(
      &fifo->out, &pos, newpos, memory_order_acq_rel, memory_order_acquire));

  size_t off = pos & (fifo->buf_size - 1);
  size_t len = MIN(data_size, fifo->buf_size - off);
  memcpy(data, (u8 *)buf + off, len);
  memcpy((u8 *)data + len, (u8 *)buf, data_size - len);

  return data_size;
}

#endif // !FIFO_H
