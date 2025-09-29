#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "container/fifo.h"
#include "util/typedef.h"
#include "util/util.h"

#define LOGGER_META_BUF_SIZE 8192
#define LOGGER_DATA_BUF_SIZE 8192

typedef struct {
  atomic_t(size_t) wp;
  atomic_t(size_t) rp;
  size_t     head;
  size_t     offset[LOGGER_META_BUF_SIZE];
  atomic_int size[LOGGER_META_BUF_SIZE];
} logger_meta_t;

typedef struct {
  atomic_t(size_t) ptr;
  u8 buf[LOGGER_DATA_BUF_SIZE];
} logger_data_t;

typedef struct {
  logger_meta_t meta;
  logger_data_t data;
} logger_buf_t;

typedef enum {
  LOGGER_LEVEL_DATA,  // 数据
  LOGGER_LEVEL_DEBUG, // 调试
  LOGGER_LEVEL_INFO,  // 一般
  LOGGER_LEVEL_WARN,  // 警告
  LOGGER_LEVEL_ERROR, // 错误
} logger_level_e;

typedef enum {
  LOGGER_SYNC,
  LOGGER_ASYNC,
} logger_mode_e;

typedef struct {
  logger_mode_e  e_mode;
  logger_level_e e_level;
  fifo_policy_e  e_policy;
  char           end_sign;
  const char    *prefix;
  void          *fp;
  logger_buf_t  *buf;
  u8            *flush_buf;
  size_t         flush_buf_cap;
} logger_cfg_t;

typedef struct {
  bool busy;
} logger_lo_t;

typedef u64 (*logger_get_ts_f)(void);
typedef void (*logger_flush_f)(void *fp, const u8 *data, size_t size);

typedef struct {
  logger_get_ts_f f_get_ts;
  logger_flush_f  f_flush;
} logger_ops_t;

typedef struct {
  logger_cfg_t cfg;
  logger_lo_t  lo;
  logger_ops_t ops;
} logger_t;

#define DECL_LOGGER_PTRS(logger)                                                                   \
  logger_cfg_t *cfg = &(logger)->cfg;                                                              \
  logger_lo_t  *lo  = &(logger)->lo;                                                               \
  logger_ops_t *ops = &(logger)->ops;                                                              \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(lo);                                                                                  \
  ARG_UNUSED(ops);

#define DECL_LOGGER_PTR_RENAME(logger, name)                                                       \
  logger_t *name = (logger);                                                                       \
  ARG_UNUSED(name);

static inline size_t logger_used(logger_buf_t *buf) {
  size_t tail_ptr = atomic_load_explicit(&buf->data.ptr, memory_order_acquire);
  size_t head_idx = atomic_load_explicit(&buf->meta.rp, memory_order_acquire);
  if (head_idx == atomic_load_explicit(&buf->meta.wp, memory_order_relaxed))
    return 0;

  size_t oldest_idx    = head_idx % LOGGER_META_BUF_SIZE;
  size_t oldest_offset = buf->meta.offset[oldest_idx];
  return tail_ptr - oldest_offset;
}

static inline size_t logger_free(logger_buf_t *buf) {
  size_t used = logger_used(buf);
  if (used >= LOGGER_DATA_BUF_SIZE)
    return 0;
  return LOGGER_DATA_BUF_SIZE - used;
}

static inline void logger_meta_init(logger_meta_t *meta) {
  atomic_init(&meta->wp, 0);
  atomic_init(&meta->rp, 0);
  meta->head = 0;
  for (size_t i = 0; i < LOGGER_META_BUF_SIZE; ++i) {
    meta->offset[i] = 0;
    atomic_init(&meta->size[i], 0);
  }
}

static inline void logger_init(logger_t *logger, logger_cfg_t logger_cfg) {
  DECL_LOGGER_PTRS(logger);

  *cfg = logger_cfg;
  logger_meta_init(&cfg->buf->meta);
  atomic_init(&cfg->buf->data.ptr, 0);
}

static inline size_t logger_push_data(logger_data_t *buf, const u8 *data, size_t n) {
  size_t ptr    = atomic_fetch_add_explicit(&buf->ptr, n, memory_order_acq_rel);
  size_t offset = ptr % LOGGER_DATA_BUF_SIZE;
  if (offset + n <= LOGGER_DATA_BUF_SIZE) {
    memcpy(buf->buf + offset, data, n);
  } else {
    size_t first_part  = LOGGER_DATA_BUF_SIZE - offset;
    size_t second_part = n - first_part;
    memcpy(buf->buf + offset, data, first_part);
    memcpy(buf->buf, data + first_part, second_part);
  }
  return ptr;
}

static inline size_t logger_policy(logger_cfg_t *cfg, logger_buf_t *buf, size_t want) {
  size_t free = logger_free(buf);
  if (want <= free)
    return want;

  switch (cfg->e_policy) {
  case FIFO_POLICY_TRUNCATE:
    return free;
  case FIFO_POLICY_OVERWRITE: {
    size_t need     = want - free;
    size_t head_idx = atomic_load_explicit(&buf->meta.rp, memory_order_acquire);
    size_t tail_idx = atomic_load_explicit(&buf->meta.wp, memory_order_acquire);

    size_t freed = 0;
    while (head_idx != tail_idx && freed < need) {
      size_t idx       = head_idx % LOGGER_META_BUF_SIZE;
      int    slot_size = atomic_load_explicit(&buf->meta.size[idx], memory_order_acquire);
      if (slot_size <= 0) {
        head_idx++;
        continue;
      }
      freed += (size_t)slot_size;
      atomic_store_explicit(&buf->meta.size[idx], -1, memory_order_release);
      head_idx++;
    }
    atomic_store_explicit(&buf->meta.rp, head_idx, memory_order_release);

    size_t free_after = logger_free(buf);
    if (want <= free_after)
      return want;
    return free_after;
  }
  case FIFO_POLICY_REJECT:
    return 0;
  }
  return 0;
}

static inline size_t logger_push(logger_buf_t *buf, const u8 *data, size_t n, logger_cfg_t *cfg) {
  size_t will_write = logger_policy(cfg, buf, n);
  if (will_write == 0)
    return 0;

  size_t data_offset = logger_push_data(&buf->data, data, will_write);

  size_t index = atomic_fetch_add_explicit(&buf->meta.wp, 1, memory_order_acq_rel);
  size_t slot  = index % LOGGER_META_BUF_SIZE;

  buf->meta.offset[slot] = data_offset;
  atomic_store_explicit(&buf->meta.size[slot], (int)will_write, memory_order_release);

  return will_write;
}

static inline ssize_t logger_get_idx(logger_buf_t *buf, size_t *out_offset) {
  logger_meta_t *meta = &buf->meta;
  size_t         t    = atomic_load_explicit(&meta->wp, memory_order_acquire);
  if (t == meta->head)
    return 0;

  if ((t - meta->head) >= LOGGER_META_BUF_SIZE) {
    size_t idx = meta->head % LOGGER_META_BUF_SIZE;
    atomic_store_explicit(&meta->size[idx], -1, memory_order_release);
    meta->head++;
    atomic_store_explicit(&meta->rp, meta->head, memory_order_release);
    return -1;
  }

  size_t idx = meta->head % LOGGER_META_BUF_SIZE;
  int    sz  = atomic_load_explicit(&meta->size[idx], memory_order_acquire);
  if (sz <= 0) {
    return 0;
  }

  *out_offset = meta->offset[idx];

  atomic_store_explicit(&meta->size[idx], -1, memory_order_release);

  meta->head++;
  atomic_store_explicit(&meta->rp, meta->head, memory_order_release);

  return (ssize_t)sz;
}

static inline ssize_t logger_pop(logger_buf_t *buf, u8 *output, size_t n) {
  size_t  queue_offset;
  ssize_t sz = logger_get_idx(buf, &queue_offset);
  if (sz <= 0)
    return sz;

  size_t size = (size_t)sz;
  if (size > n)
    size = n;

  logger_data_t *data = &buf->data;

  size_t ptr_before = atomic_load_explicit(&data->ptr, memory_order_acquire);
  if (queue_offset + (size_t)LOGGER_DATA_BUF_SIZE < ptr_before)
    return -1;

  size_t offset = queue_offset % LOGGER_DATA_BUF_SIZE;
  if (offset + size <= LOGGER_DATA_BUF_SIZE) {
    memcpy(output, data->buf + offset, size);
  } else {
    size_t first  = LOGGER_DATA_BUF_SIZE - offset;
    size_t second = size - first;
    memcpy(output, data->buf + offset, first);
    memcpy(output + first, data->buf, second);
  }

  size_t ptr_after = atomic_load_explicit(&data->ptr, memory_order_acquire);
  if (queue_offset + (size_t)LOGGER_DATA_BUF_SIZE < ptr_after)
    return -1;

  return (ssize_t)size;
}

static inline void logger_write(logger_t *logger, const char *fmt, va_list args) {
  DECL_LOGGER_PTRS(logger);

  u8  tmp[256];
  int n = snprintf((char *)tmp, sizeof(tmp), "[%llu] ", (u64)logger->ops.f_get_ts());
  if (n < 0)
    return;
  if ((size_t)n >= sizeof(tmp))
    n = (int)sizeof(tmp) - 1;

  int appended = vsnprintf((char *)tmp + n, sizeof(tmp) - n, fmt, args);
  if (appended < 0)
    return;
  if ((size_t)(n + appended) > sizeof(tmp))
    appended = (int)sizeof(tmp) - n;

  logger_push(cfg->buf, tmp, (size_t)(n + appended), cfg);
}

static inline void logger_flush(logger_t *logger) {
  DECL_LOGGER_PTRS(logger);

  while (true) {
    ssize_t r = logger_pop(cfg->buf, cfg->flush_buf, cfg->flush_buf_cap ? cfg->flush_buf_cap : 128);
    if (r == 0)
      break;
    if (r < 0)
      continue;
    ops->f_flush(cfg->fp, cfg->flush_buf, (size_t)r);
    lo->busy = (cfg->e_mode == LOGGER_ASYNC);
  }
  lo->busy = false;
}

static inline void logger_data(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_DATA)
    return;

  va_list arg;
  va_start(arg, fmt);
  logger_write(logger, fmt, arg);
  va_end(arg);
}

static inline void logger_debug(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_DEBUG)
    return;

  va_list arg;
  va_start(arg, fmt);
  logger_write(logger, fmt, arg);
  va_end(arg);
}

static inline void logger_info(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_INFO)
    return;

  va_list arg;
  va_start(arg, fmt);
  logger_write(logger, fmt, arg);
  va_end(arg);
}

static inline void logger_warn(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_WARN)
    return;

  va_list arg;
  va_start(arg, fmt);
  logger_write(logger, fmt, arg);
  va_end(arg);
}

static inline void logger_error(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_ERROR)
    return;

  va_list arg;
  va_start(arg, fmt);
  logger_write(logger, fmt, arg);
  va_end(arg);
}

#endif // !LOGGER_H
