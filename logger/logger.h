#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../container/fifo.h"
#include "../util/util.h"

#define LOGGER_TIMESTAMP_SIZE 32
#define LOGGER_LEVEL_SIZE     16
#define LOGGER_MSG_BUF_SIZE   (64 - sizeof(u32))
#define LOGGER_FIFO_BUF_SIZE  (1024 * 1024)

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
  FILE          *file;
} logger_cfg_t;

typedef struct {
  u32 len;
  u8  buf[LOGGER_MSG_BUF_SIZE];
} logger_msg_t;

typedef struct {
  fifo_t fifo;
  u8     buf[LOGGER_FIFO_BUF_SIZE];
} logger_lo_t;

typedef void (*logger_print_f)(FILE *file, char *str, u32 len);
typedef u64 (*logger_get_ts_f)(void);

typedef struct {
  logger_get_ts_f f_get_ts;
  logger_print_f  f_print;
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

  fifo_init(&lo->fifo, lo->buf, sizeof(lo->buf), FIFO_POLICY_DISCARD);
}

static inline void logger_flush(logger_t *logger) {
  DECL_LOGGER_PTRS(logger);

  logger_msg_t msg;
  while (fifo_mpmc_out(&lo->fifo, &msg, sizeof(msg)))
    ops->f_print(cfg->file, (char *)msg.buf, msg.len);
}

static inline void logger_write(logger_t *logger, const char *format, ...) {
  DECL_LOGGER_PTRS(logger);

  logger_msg_t msg;
  va_list      args;
  va_start(args, format);
  msg.len = vsnprintf((char *)msg.buf, sizeof(msg.buf), format, args);
  va_end(args);

  if (msg.len > sizeof(msg.buf))
    msg.len = sizeof(msg.buf);

  fifo_mpmc_in(&lo->fifo, &msg, sizeof(msg));
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
