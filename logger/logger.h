#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../container/fifo.h"
#include "../util/util.h"

typedef enum {
  LOGGER_LEVEL_DATA,  // 数据
  LOGGER_LEVEL_DEBUG, // 调试
  LOGGER_LEVEL_INFO,  // 一般
  LOGGER_LEVEL_WARN,  // 警告
  LOGGER_LEVEL_ERROR, // 错误
} logger_level_e;

typedef struct {
  logger_level_e level;
  char           new_line_sign;
  const char    *prefix;
  void          *fp;
  u8            *fifo_buf, *line_buf;
  size_t         fifo_buf_size, line_buf_size;
} logger_cfg_t;

typedef struct {
  fifo_t fifo;
  u8    *fifo_buf;
  u8    *line_buf;
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

#define DECL_LOGGER_PTRS_PREFIX(logger, name)                                                      \
  logger_t *name = (logger);                                                                       \
  ARG_UNUSED(name);

static inline void logger_init(logger_t *logger, logger_cfg_t logger_cfg) {
  DECL_LOGGER_PTRS(logger);

  *cfg = logger_cfg;

  lo->fifo_buf = cfg->fifo_buf;
  lo->line_buf = cfg->line_buf;

  fifo_init(&lo->fifo, lo->fifo_buf, cfg->fifo_buf_size, FIFO_POLICY_DISCARD);
}

static inline void logger_flush(logger_t *logger) {
  DECL_LOGGER_PTRS(logger);

  size_t size = 0;
  u8     c;

  while (size < cfg->line_buf_size && fifo_mpmc_out(&lo->fifo, &c, sizeof(c)) != 0) {
    lo->line_buf[size++] = c;
    if (c == cfg->new_line_sign)
      break;
  }

  if (size > 0)
    ops->f_flush(cfg->fp, lo->line_buf, size);
}

static inline void logger_write(logger_t *logger, const char *fmt, va_list args) {
  DECL_LOGGER_PTRS(logger);

  u8 buf[128];

  int size = vsnprintf((char *)buf, sizeof(buf), fmt, args);
  if (size < 0)
    return;
  if ((size_t)size > sizeof(buf))
    size = sizeof(buf);

  fifo_mpmc_in(&lo->fifo, buf, size);
}

static inline void logger_data(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_DATA)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

static inline void logger_debug(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_DEBUG)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

static inline void logger_info(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_INFO)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

static inline void logger_warn(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_WARN)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

static inline void logger_error(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_ERROR)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

#endif // !LOGGER_H
