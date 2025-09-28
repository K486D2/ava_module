#ifndef FIFO_H
#define FIFO_H

#include <string.h>

#include "container/list.h"
#include "util/mathdef.h"
#include "util/typedef.h"

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

/* only MP/MC */
#define FIFO_MAX_BLOCKS 10   // 最多10个块
#define FIFO_BLOCK_SIZE 1024 // 每个块的大小

typedef struct {
  list_head_t node;                  // 链表节点
  size_t      head;                  // 当前块的读取指针
  size_t      tail;                  // 当前块的写入指针
  u8          data[FIFO_BLOCK_SIZE]; // 数据区
} fifo_block_t;

typedef struct {
  fifo_mode_e   e_mode;   // 模式
  fifo_policy_e e_policy; // 写入数据超过剩余空间时的处理策略
  atomic_t(size_t) head;  // 读取位置
  atomic_t(size_t) tail;  // 写入位置
  void       *buf;        // 缓冲区
  size_t      cap;        // 缓冲区容量(2^n)
  list_head_t block_list; // 块链表
  bool        block_free[FIFO_MAX_BLOCKS];
} fifo_t;

static inline int
fifo_init(fifo_t *fifo, void *buf, size_t cap, fifo_mode_e e_mode, fifo_policy_e e_policy);
static inline int
fifo_init_buf(fifo_t *fifo, size_t cap, fifo_mode_e e_mode, fifo_policy_e e_policy);
static inline void          fifo_reset(fifo_t *fifo);
static inline bool          fifo_empty(fifo_t *fifo);
static inline bool          fifo_full(fifo_t *fifo);
static inline size_t        fifo_avail(fifo_t *fifo);
static inline size_t        fifo_free(fifo_t *fifo);
static inline fifo_block_t *fifo_get_block(fifo_t *fifo);
static inline void          fifo_release_block(fifo_t *fifo, fifo_block_t *block);
static inline size_t        fifo_policy(fifo_t *fifo, size_t tail, size_t head, size_t data_size);

static inline size_t fifo_write(fifo_t *fifo, const void *data, size_t data_size);
static inline size_t fifo_read(fifo_t *fifo, void *data, size_t data_size);
static inline size_t fifo_write_buf(fifo_t *fifo, void *buf, const void *data, size_t data_size);
static inline size_t fifo_read_buf(fifo_t *fifo, void *buf, void *data, size_t data_size);

static inline size_t fifo_spsc_write(fifo_t *fifo, void *buf, const void *data, size_t data_size);
static inline size_t fifo_spsc_read(fifo_t *fifo, void *buf, void *data, size_t data_size);
static inline size_t fifo_mpsc_write(fifo_t *fifo, void *buf, const void *data, size_t data_size);
static inline size_t fifo_mpsc_read(fifo_t *fifo, void *buf, void *data);
static inline size_t fifo_spmc_write(fifo_t *fifo, void *buf, const void *data, size_t data_size);
static inline size_t fifo_spmc_read(fifo_t *fifo, void *buf, void *data);
static inline size_t fifo_mpmc_write(fifo_t *fifo, void *buf, const void *data, size_t data_size);
static inline size_t fifo_mpmc_read(fifo_t *fifo, void *buf, void *data, size_t data_size);

/* -------------------------------------------------------------------------- */
/*                                     API                                    */
/* -------------------------------------------------------------------------- */

static inline int
fifo_init(fifo_t *fifo, void *buf, size_t cap, fifo_mode_e e_mode, fifo_policy_e e_policy) {
  int ret = fifo_init_buf(fifo, cap, e_mode, e_policy);
  if (ret != 0)
    return ret;

  fifo->buf = buf;

  fifo_block_t *first_block = fifo_get_block(fifo);
  if (!first_block)
    return -1;
  list_init(&fifo->block_list);
  list_add(&first_block->node, &fifo->block_list);

  return 0;
}

static inline int
fifo_init_buf(fifo_t *fifo, size_t cap, fifo_mode_e e_mode, fifo_policy_e e_policy) {
  // if (!IS_POWER_OF_2(cap))
  //   return -1;

  fifo->e_mode   = e_mode;
  fifo->e_policy = e_policy;
  fifo->cap      = cap;
  atomic_store(&fifo->tail, 0);
  atomic_store(&fifo->head, 0);

  for (size_t i = 0; i < cap; i++)
    fifo->block_free[i] = true;

  return 0;
}

static inline void fifo_reset(fifo_t *fifo) {
  atomic_store(&fifo->tail, 0);
  atomic_store(&fifo->head, 0);
}

static inline bool fifo_empty(fifo_t *fifo) {
  return fifo_avail(fifo) == 0;
}

static inline bool fifo_full(fifo_t *fifo) {
  return fifo_free(fifo) == 0;
}

static inline size_t fifo_avail(fifo_t *fifo) {
  return atomic_load(&fifo->tail) - atomic_load(&fifo->head);
}

static inline size_t fifo_free(fifo_t *fifo) {
  return fifo->cap - fifo_avail(fifo);
}

static inline fifo_block_t *fifo_get_block(fifo_t *fifo) {
  for (size_t i = 0; i < fifo->cap; ++i) {
    if (fifo->block_free[i]) {
      fifo->block_free[i] = false;
      return &((fifo_block_t *)fifo->buf)[i];
    }
  }
  return NULL;
}

static inline void fifo_release_block(fifo_t *fifo, fifo_block_t *block) {
  size_t idx = block - (fifo_block_t *)fifo->buf;
  if (idx < fifo->cap)
    fifo->block_free[idx] = true;
}

static inline size_t fifo_policy(fifo_t *fifo, size_t tail, size_t head, size_t data_size) {
  size_t free = fifo->cap - (tail - head);
  if (data_size <= free)
    return data_size;

  switch (fifo->e_policy) {
  case FIFO_POLICY_TRUNCATE: {
    data_size = free;
    return data_size;
  }
  case FIFO_POLICY_OVERWRITE: {
    atomic_fetch_add_explicit(&fifo->head, data_size - free, memory_order_acq_rel);
    return data_size;
  }
  case FIFO_POLICY_REJECT:
    return 0;
  }

  return 0;
}

static inline size_t fifo_write(fifo_t *fifo, const void *data, size_t data_size) {
  return fifo_write_buf(fifo, fifo->buf, data, data_size);
}

static inline size_t fifo_read(fifo_t *fifo, void *data, size_t data_size) {
  return fifo_read_buf(fifo, fifo->buf, data, data_size);
}

static inline size_t fifo_write_buf(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  switch (fifo->e_mode) {
  case FIFO_MODE_SPSC:
    return fifo_spsc_write(fifo, buf, data, data_size);
  case FIFO_MODE_SPMC:
    return fifo_spmc_write(fifo, buf, data, data_size);
  case FIFO_MODE_MPSC:
    return fifo_mpsc_write(fifo, buf, data, data_size);
  case FIFO_MODE_MPMC:
    return fifo_mpmc_write(fifo, buf, data, data_size);
  }
  return 0;
}

static inline size_t fifo_read_buf(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  switch (fifo->e_mode) {
  case FIFO_MODE_SPSC:
    return fifo_spsc_read(fifo, buf, data, data_size);
  case FIFO_MODE_SPMC:
    return fifo_spmc_read(fifo, buf, data);
  case FIFO_MODE_MPSC:
    return fifo_mpsc_read(fifo, buf, data);
  case FIFO_MODE_MPMC:
    return fifo_mpmc_read(fifo, buf, data, data_size);
  }
  return 0;
}

/* -------------------------------------------------------------------------- */
/*                                    SPSC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_spsc_write(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  size_t mask = fifo->cap - 1;
  size_t tail = atomic_load_explicit(&fifo->tail, memory_order_relaxed);
  size_t head = atomic_load_explicit(&fifo->head, memory_order_acquire);

  data_size = fifo_policy(fifo, tail, head, data_size);
  if (data_size == 0)
    return 0;

  size_t offset = tail & mask;
  size_t first  = MIN(data_size, fifo->cap - offset);
  memcpy((u8 *)buf + offset, data, first);
  memcpy((u8 *)buf, (u8 *)data + first, data_size - first);

  atomic_store_explicit(&fifo->tail, tail + data_size, memory_order_release);
  return data_size;
}

static inline size_t fifo_spsc_read(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t mask = fifo->cap - 1;
  size_t head = atomic_load_explicit(&fifo->head, memory_order_relaxed);
  size_t tail = atomic_load_explicit(&fifo->tail, memory_order_acquire);

  size_t avail = tail - head;
  if (data_size > avail)
    data_size = avail;
  if (data_size == 0)
    return 0;

  size_t offset = head & mask;
  size_t first  = MIN(data_size, fifo->cap - offset);
  memcpy(data, (u8 *)buf + offset, first);
  memcpy((u8 *)data + first, (u8 *)buf, data_size - first);

  atomic_store_explicit(&fifo->head, head + data_size, memory_order_release);
  return data_size;
}

/* -------------------------------------------------------------------------- */
/*                                    SPMC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_spmc_write(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
}

static inline size_t fifo_spmc_read(fifo_t *fifo, void *buf, void *data) {
}

/* -------------------------------------------------------------------------- */
/*                                    MPSC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_mpsc_write(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
}

static inline size_t fifo_mpsc_read(fifo_t *fifo, void *buf, void *data) {
}

/* -------------------------------------------------------------------------- */
/*                                    MPMC                                    */
/* -------------------------------------------------------------------------- */

static inline size_t fifo_mpmc_write(fifo_t *fifo, void *buf, const void *data, size_t data_size) {
  size_t written = 0;

  // 获取队列中的第一个块
  fifo_block_t *block = list_first_entry(&fifo->block_list, fifo_block_t, node);

  while (data_size > 0) {
    size_t avail    = sizeof(block->data) - block->tail;
    size_t to_write = MIN(avail, data_size);

    // 写入数据
    memcpy(block->data + block->tail, data + written, to_write);
    block->tail += to_write;
    written += to_write;
    data_size -= to_write;

    // 如果块写满了，分配一个新块
    if (block->tail == sizeof(block->data)) {
      fifo_block_t *new_block = fifo_get_block(fifo);
      if (!new_block)
        return written; // 如果没有空闲块，则返回已写入的数据量

      list_add_tail(&new_block->node, &fifo->block_list);
      block = new_block; // 更新为新块
    }
  }

  return written;
}

static inline size_t fifo_mpmc_read(fifo_t *fifo, void *buf, void *data, size_t data_size) {
  size_t read = 0;

  // 获取队列中的第一个块
  fifo_block_t *block = list_first_entry(&fifo->block_list, fifo_block_t, node);

  while (data_size > 0) {
    size_t avail   = block->tail - block->head;
    size_t to_read = MIN(avail, data_size);

    // 读取数据
    memcpy(data + read, block->data + block->head, to_read);
    block->head += to_read;
    read += to_read;
    data_size -= to_read;

    // 如果当前块已经读取完，移除该块并继续读取下一个块
    if (block->head == block->tail) {
      list_del(&block->node);
      fifo_release_block(fifo, block);
      if (!list_empty(&fifo->block_list)) {
        block = list_first_entry(&fifo->block_list, fifo_block_t, node);
      } else {
        break; // 队列为空
      }
    }
  }

  return read;
}

#endif // !FIFO_H
