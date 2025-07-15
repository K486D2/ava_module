#ifndef FIFO_H
#define FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../util/def.h"

#if defined(__linux) || defined(_WIN32)
#include <pthread.h>
typedef pthread_spinlock_t spinlock_t;
#else
typedef u32 spinlock_t;
#endif

#include <string.h>

#define IS_POWER_OF_2(n)    (n != 0) && ((n & (n - 1)) == 0)
#define ROUNDUP_POW_OF_2(n) ((n == 0) ? 1 : (1 << (sizeof(n) * 8 - __builtin_clz(n - 1))))

static inline void spin_lock(spinlock_t *lock) {

#if defined(__linux) || defined(_WIN32)
  pthread_spin_lock(lock);
#else
  while (__sync_lock_test_and_set(lock, 1)) {
    while (*lock)
      __asm__ __volatile__("pause" ::: "memory");
  }
#endif
}

static inline void spin_unlock(spinlock_t *lock) {
#if defined(__linux) || defined(_WIN32)
  pthread_spin_unlock(lock);
#else
  __sync_lock_release(lock);
#endif
}

typedef struct {
  u8        *data; // 缓冲区指针
  size_t     size; // 缓冲区大小(必须是2的幂)
  size_t     in;   // 写入位置索引
  size_t     out;  // 读取位置索引
  spinlock_t lock; // 自旋锁
} fifo_t;

static void fifo_init(fifo_t *fifo, void *buf, size_t size) {
  if (!IS_POWER_OF_2(size))
    size = ROUNDUP_POW_OF_2(size);

  fifo->data = (u8 *)buf;
  fifo->size = size;
  fifo->in = fifo->out = 0;
  fifo->lock           = 0;

#if defined(__linux__) || defined(_WIN32)
  pthread_spin_init(&fifo->lock, PTHREAD_PROCESS_PRIVATE);
#endif
}

static void fifo_reset(fifo_t *fifo) {
  fifo->in = fifo->out = 0;
}

static size_t fifo_len(fifo_t *fifo) {
  return fifo->in - fifo->out;
}

static size_t fifo_avail(fifo_t *fifo) {
  return fifo->size - fifo_len(fifo);
}

static bool fifo_is_empty(fifo_t *fifo) {
  return fifo->in == fifo->out;
}

static bool fifo_is_full(fifo_t *fifo) {
  return fifo_len(fifo) == fifo->size;
}

static size_t fifo_peek(fifo_t *fifo, void *buf, size_t size) {
  size       = MIN(size, fifo_len(fifo));
  size_t off = fifo->out & (fifo->size - 1);

  size_t len = MIN(size, fifo->size - off);
  memcpy(buf, fifo->data + off, len);

  memcpy(buf + len, fifo->data, size - len);
  return size;
}

static size_t __fifo_in(fifo_t *fifo, const void *buf, size_t size) {
  size = MIN(size, fifo->size - fifo->in + fifo->out);
  __sync_synchronize();

  size_t len = MIN(size, fifo->size - (fifo->in & (fifo->size - 1)));
  memcpy(fifo->data + (fifo->in & (fifo->size - 1)), buf, len);

  memcpy(fifo->data, buf + len, size - len);
  __sync_synchronize();

  fifo->in += size;
  return size;
}

static size_t __fifo_out(fifo_t *fifo, void *buf, size_t size) {
  size = MIN(size, fifo->in - fifo->out);
  __sync_synchronize();

  size_t len = MIN(size, fifo->size - (fifo->out & (fifo->size - 1)));
  memcpy(buf, fifo->data + (fifo->out & (fifo->size - 1)), len);

  memcpy(buf + len, fifo->data, size - len);
  __sync_synchronize();

  fifo->out += size;
  return size;
}

static size_t fifo_in(fifo_t *fifo, const void *buf, size_t size) {
  spin_lock(&fifo->lock);
  size_t ret = __fifo_in(fifo, buf, size);
  spin_unlock(&fifo->lock);
  return ret;
}

static size_t fifo_out(fifo_t *fifo, void *buf, size_t size) {
  spin_lock(&fifo->lock);
  size_t ret = __fifo_out(fifo, buf, size);
  spin_unlock(&fifo->lock);
  return ret;
}

#ifdef __cplusplus
}
#endif

#endif // !FIFO_H
