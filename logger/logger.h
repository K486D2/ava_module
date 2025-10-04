#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "container/mpsc.h"
#include "util/typedef.h"
#include "util/util.h"

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
  u64 ts;
  usz id;
  usz msg_nbytes;
} logger_entry_t;

typedef struct {
  logger_mode_e  e_mode;
  logger_level_e e_level;
  char           end_sign;
  void          *fp;
  void          *buf;
  size_t         cap;
  u8            *flush_buf;
  size_t         flush_cap;
  mpsc_p_t      *producers;
  size_t         nproducers;
} logger_cfg_t;

typedef struct {
  mpsc_t mpsc;
  bool   busy;
} logger_lo_t;

typedef u64 (*logger_get_ts_f)(void);
typedef void (*logger_flush_f)(void *fp, const u8 *src, size_t nbytes);

typedef struct {
  logger_get_ts_f f_get_ts;
  logger_flush_f  f_flush;
} logger_ops_t;

typedef struct {
  logger_cfg_t cfg;
  logger_lo_t  lo;
  logger_ops_t ops;
} logger_t;

static inline void logger_init(logger_t *logger, logger_cfg_t logger_cfg);
static inline void logger_write(logger_t *logger, usz id, const char *fmt, va_list args);
static inline void logger_flush(logger_t *logger);

static inline void logger_data(logger_t *logger, usz id, const char *fmt, ...);
static inline void logger_debug(logger_t *logger, usz id, const char *fmt, ...);
static inline void logger_info(logger_t *logger, usz id, const char *fmt, ...);
static inline void logger_warn(logger_t *logger, usz id, const char *fmt, ...);
static inline void logger_error(logger_t *logger, usz id, const char *fmt, ...);

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

static inline void logger_init(logger_t *logger, logger_cfg_t logger_cfg) {
  DECL_LOGGER_PTRS(logger);

  *cfg = logger_cfg;
  mpsc_init(&lo->mpsc, cfg->buf, cfg->cap, cfg->producers, cfg->nproducers);
}

static inline void logger_write(logger_t *logger, usz id, const char *fmt, va_list args) {
  DECL_LOGGER_PTRS(logger);

  logger_entry_t entry = {
      .ts         = ops->f_get_ts(),
      .id         = id,
      .msg_nbytes = (usz)vsnprintf(NULL, 0, fmt, args),
  };

  usz total_nbytes = sizeof(entry) + entry.msg_nbytes;
  if (total_nbytes > cfg->cap)
    return;

  mpsc_p_t *p   = mpsc_reg(&lo->mpsc, id);
  isz       off = mpsc_acquire(&lo->mpsc, p, total_nbytes);
  if (off < 0) {
    mpsc_unreg(p);
    return;
  }

  u8 *buf = (u8 *)lo->mpsc.buf + (usz)off;
  memcpy(buf, &entry, sizeof(entry));
  int msg_nbytes = vsnprintf((char *)buf + sizeof(entry), entry.msg_nbytes, fmt, args);
  if (msg_nbytes != (int)entry.msg_nbytes) {
    mpsc_push(p);
    mpsc_unreg(p);
    return;
  }

  mpsc_push(p);
  mpsc_unreg(p);
}

static inline void logger_flush(logger_t *logger) {
  DECL_LOGGER_PTRS(logger);

  while (!lo->busy) {
    logger_entry_t entry  = {0};
    usz            nbytes = mpsc_read(&lo->mpsc, &entry, sizeof(entry));
    if (nbytes == 0)
      break;

    usz total_nbytes =
        snprintf((char *)cfg->flush_buf, cfg->flush_cap, "[%llu][%zu]", entry.ts, entry.id);
    total_nbytes += mpsc_read(&lo->mpsc, cfg->flush_buf + total_nbytes, entry.msg_nbytes);
    if (entry.msg_nbytes + sizeof(entry) > total_nbytes) {
      mpsc_release(&lo->mpsc, total_nbytes);
      break;
    }

    ops->f_flush(cfg->fp, cfg->flush_buf, total_nbytes);
    lo->busy = (cfg->e_mode == LOGGER_ASYNC);
  }
}

static inline void logger_data(logger_t *logger, usz id, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_DATA)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, id, fmt, args);
  va_end(args);
}

static inline void logger_debug(logger_t *logger, usz id, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_DEBUG)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, id, fmt, args);
  va_end(args);
}

static inline void logger_info(logger_t *logger, usz id, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_INFO)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, id, fmt, args);
  va_end(args);
}

static inline void logger_warn(logger_t *logger, usz id, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_WARN)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, id, fmt, args);
  va_end(args);
}

static inline void logger_error(logger_t *logger, usz id, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_ERROR)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, id, fmt, args);
  va_end(args);
}

#endif // !LOGGER_H
