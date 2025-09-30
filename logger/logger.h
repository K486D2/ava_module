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
  logger_mode_e  e_mode;
  logger_level_e e_level;
  // mpsc_policy_e  e_policy;
  char        end_sign;
  const char *prefix;
  void       *fp;
  void       *buf;
  size_t      cap;
  mpsc_p_t   *producers;
  size_t      nproducers;
  u8         *flush_buf;
  size_t      flush_cap;
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

  u8  tmp[128];
  int nbytes = vsnprintf((char *)tmp, sizeof(tmp), fmt, args);
  if (nbytes < 0)
    return;
  if (nbytes > (int)sizeof(tmp))
    nbytes = sizeof(tmp);

  mpsc_write(&lo->mpsc, mpsc_reg(&lo->mpsc, id), tmp, (usz)nbytes);
}

static inline void logger_flush(logger_t *logger) {
  DECL_LOGGER_PTRS(logger);

  while (!lo->busy) {
    usz nbytes = mpsc_read(&lo->mpsc, cfg->flush_buf, cfg->flush_cap);
    if (nbytes == 0)
      break;

    ops->f_flush(cfg->fp, cfg->flush_buf, nbytes);
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
