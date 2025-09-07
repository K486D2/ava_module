#ifndef FIFO_H
#define FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../util/def.h"

#include <stdatomic.h>
#include <string.h>

#define IS_POWER_OF_2(n)    ((n) != 0 && (((n) & ((n) - 1)) == 0))
#define ROUNDUP_POW_OF_2(n) ((n) == 0 ? 1 : (1ULL << (sizeof(n) * 8 - __builtin_clzll((n) - 1))))

typedef struct {
  void          *buf;  // 缓冲区指针
  size_t         size; // 缓冲区大小(必须是2的幂)
  _Atomic size_t in;   // 写入位置
  _Atomic size_t out;  // 读取位置
} fifo_t;

static void fifo_reset(fifo_t *fifo) {
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
}

static size_t fifo_len(fifo_t *fifo) {
  return atomic_load(&fifo->in) - atomic_load(&fifo->out);
}

static size_t fifo_avail(fifo_t *fifo) {
  return fifo->size - fifo_len(fifo);
}

static bool fifo_is_empty(fifo_t *fifo) {
  return fifo_len(fifo) == 0;
}

static bool fifo_is_full(fifo_t *fifo) {
  return fifo_len(fifo) == fifo->size;
}

static void fifo_init(fifo_t *fifo, void *buf, size_t buf_size) {
  if (!IS_POWER_OF_2(buf_size))
    buf_size = ROUNDUP_POW_OF_2(buf_size);

  fifo->buf  = buf;
  fifo->size = buf_size;
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
}

static void fifo_buf_init(fifo_t *fifo, size_t buf_size) {
  if (!IS_POWER_OF_2(buf_size))
    buf_size = ROUNDUP_POW_OF_2(buf_size);

  fifo->size = buf_size;
  atomic_store(&fifo->in, 0);
  atomic_store(&fifo->out, 0);
}

/* -------------------------------------------------------------------------- */
/*                                    MPMC                                    */
/* -------------------------------------------------------------------------- */

static size_t fifo_mpmc_in(fifo_t *fifo, const void *data, size_t size) {
  size_t pos, newpos;
  do {
    pos         = atomic_load_explicit(&fifo->in, memory_order_acquire);
    size_t free = fifo->size - (pos - atomic_load_explicit(&fifo->out, memory_order_acquire));
    if (size > free)
      size = free;
    if (size == 0)
      return 0;

    newpos = pos + size;
  } while (!atomic_compare_exchange_weak_explicit(
      &fifo->in, &pos, newpos, memory_order_acq_rel, memory_order_acquire));

  size_t off = pos & (fifo->size - 1);
  size_t len = MIN(size, fifo->size - off);
  memcpy((u8 *)fifo->buf + off, data, len);
  memcpy((u8 *)fifo->buf, (u8 *)data + len, size - len);

  return size;
}

static size_t fifo_mpmc_out(fifo_t *fifo, void *data, size_t size) {
  size_t pos, newpos;
  do {
    pos          = atomic_load_explicit(&fifo->out, memory_order_acquire);
    size_t avail = atomic_load_explicit(&fifo->in, memory_order_acquire) - pos;
    if (size > avail)
      size = avail;
    if (size == 0)
      return 0;

    newpos = pos + size;
  } while (!atomic_compare_exchange_weak_explicit(
      &fifo->out, &pos, newpos, memory_order_acq_rel, memory_order_acquire));

  size_t off = pos & (fifo->size - 1);
  size_t len = MIN(size, fifo->size - off);
  memcpy(data, (u8 *)fifo->buf + off, len);
  memcpy((u8 *)data + len, (u8 *)fifo->buf, size - len);

  return size;
}

static size_t fifo_mpmc_buf_in(fifo_t *fifo, void *buf, const void *data, size_t size) {
  size_t pos, newpos;
  do {
    pos         = atomic_load_explicit(&fifo->in, memory_order_acquire);
    size_t free = fifo->size - (pos - atomic_load_explicit(&fifo->out, memory_order_acquire));
    if (size > free)
      size = free;
    if (size == 0)
      return 0;

    newpos = pos + size;
  } while (!atomic_compare_exchange_weak_explicit(
      &fifo->in, &pos, newpos, memory_order_acq_rel, memory_order_acquire));

  size_t off = pos & (fifo->size - 1);
  size_t len = MIN(size, fifo->size - off);
  memcpy((u8 *)buf + off, data, len);
  memcpy((u8 *)buf, (u8 *)data + len, size - len);

  return size;
}

static size_t fifo_mpmc_buf_out(fifo_t *fifo, void *buf, void *data, size_t size) {
  size_t pos, newpos;
  do {
    pos          = atomic_load_explicit(&fifo->out, memory_order_acquire);
    size_t avail = atomic_load_explicit(&fifo->in, memory_order_acquire) - pos;
    if (size > avail)
      size = avail;
    if (size == 0)
      return 0;

    newpos = pos + size;
  } while (!atomic_compare_exchange_weak_explicit(
      &fifo->out, &pos, newpos, memory_order_acq_rel, memory_order_acquire));

  size_t off = pos & (fifo->size - 1);
  size_t len = MIN(size, fifo->size - off);
  memcpy(data, (u8 *)buf + off, len);
  memcpy((u8 *)data + len, (u8 *)buf, size - len);

  return size;
}

/* -------------------------------------------------------------------------- */
/*                                    SPSC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_spsc_in(fifo_t *fifo, const void *data, size_t size) {
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_relaxed);
  size_t out = atomic_load_explicit(&fifo->out, memory_order_acquire);

  size_t free = fifo->size - (in - out);
  if (size > free)
    size = free;
  if (size == 0)
    return 0;

  size_t off = in & (fifo->size - 1);
  size_t len = MIN(size, fifo->size - off);

  memcpy((u8 *)fifo->buf + off, data, len);
  memcpy((u8 *)fifo->buf, (u8 *)data + len, size - len);

  atomic_store_explicit(&fifo->in, in + size, memory_order_release);

  return size;
}

static inline size_t fifo_spsc_out(fifo_t *fifo, void *data, size_t size) {
  size_t out = atomic_load_explicit(&fifo->out, memory_order_relaxed);
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_acquire);

  size_t avail = in - out;
  if (size > avail)
    size = avail;
  if (size == 0)
    return 0;

  size_t off = out & (fifo->size - 1);
  size_t len = MIN(size, fifo->size - off);

  memcpy(data, (u8 *)fifo->buf + off, len);
  memcpy((u8 *)data + len, (u8 *)fifo->buf, size - len);

  atomic_store_explicit(&fifo->out, out + size, memory_order_release);

  return size;
}

static inline size_t fifo_spsc_buf_in(fifo_t *fifo, void *buf, const void *data, size_t size) {
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_relaxed);
  size_t out = atomic_load_explicit(&fifo->out, memory_order_acquire);

  size_t free = fifo->size - (in - out);
  if (size > free)
    size = free;
  if (size == 0)
    return 0;

  size_t off = in & (fifo->size - 1);
  size_t len = MIN(size, fifo->size - off);

  memcpy((u8 *)buf + off, data, len);
  memcpy((u8 *)buf, (u8 *)data + len, size - len);

  atomic_store_explicit(&fifo->in, in + size, memory_order_release);

  return size;
}

static inline size_t fifo_spsc_buf_out(fifo_t *fifo, void *buf, void *data, size_t size) {
  size_t out = atomic_load_explicit(&fifo->out, memory_order_relaxed);
  size_t in  = atomic_load_explicit(&fifo->in, memory_order_acquire);

  size_t avail = in - out;
  if (size > avail)
    size = avail;
  if (size == 0)
    return 0;

  size_t off = out & (fifo->size - 1);
  size_t len = MIN(size, fifo->size - off);

  memcpy(data, (u8 *)buf + off, len);
  memcpy((u8 *)data + len, (u8 *)buf, size - len);

  atomic_store_explicit(&fifo->out, out + size, memory_order_release);

  return size;
}

#ifdef __cplusplus
}
#endif

#endif // !FIFO_H
