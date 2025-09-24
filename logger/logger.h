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
  logger_level_e level;         // 最低级别
  char           new_line_sign; // 换行符
  const char    *prefix;        // 前缀
  void          *fp;
  u8            *buf;
  size_t         buf_size;
} logger_cfg_t;

typedef struct {
  fifo_t fifo;
  u8    *buf;
  bool   flush_flag;
} logger_lo_t;

typedef u64 (*logger_get_ts_f)(void);
typedef void (*logger_putchar_f)(void *fp, u8 c);

typedef struct {
  logger_get_ts_f  f_get_ts;
  logger_putchar_f f_putchar;
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

  lo->buf = cfg->buf;

  fifo_init(&lo->fifo, lo->buf, cfg->buf_size, FIFO_POLICY_DISCARD);
}

static inline void logger_flush(logger_t *logger) {
  DECL_LOGGER_PTRS(logger);

  if (!lo->flush_flag)
    return;

  u8 c;
  while (fifo_mpmc_out(&lo->fifo, &c, sizeof(u8)))
    ops->f_putchar(cfg->fp, c);
}

static inline void logger_write(logger_t *logger, const char *format, ...) {
  DECL_LOGGER_PTRS(logger);

  u8      buf[128];
  va_list args;
  va_start(args, format);
  int size = vsnprintf((char *)buf, sizeof(buf), format, args);
  va_end(args);

  if (size < 0)
    return;
  if (size > sizeof(buf))
    size = sizeof(buf);

  fifo_mpmc_in(&lo->fifo, buf, size);

  if (memchr(buf, cfg->new_line_sign, size) || fifo_is_full(&lo->fifo))
    lo->flush_flag = true;
}

static inline void logger_data(logger_t *logger, const char *format, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_DATA)
    return;

  va_list args;
  va_start(args, format);
  logger_write(logger, format, args);
  va_end(args);
}

static inline void logger_debug(logger_t *logger, const char *format, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_DEBUG)
    return;

  va_list args;
  va_start(args, format);
  logger_write(logger, format, args);
  va_end(args);
}

static inline void logger_info(logger_t *logger, const char *format, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_INFO)
    return;

  va_list args;
  va_start(args, format);
  logger_write(logger, format, args);
  va_end(args);
}

static inline void logger_warn(logger_t *logger, const char *format, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_WARN)
    return;

  va_list args;
  va_start(args, format);
  logger_write(logger, format, args);
  va_end(args);
}

static inline void logger_error(logger_t *logger, const char *format, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->level > LOGGER_LEVEL_ERROR)
    return;

  va_list args;
  va_start(args, format);
  logger_write(logger, format, args);
  va_end(args);
}

#endif // !LOGGER_H
