#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
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

typedef enum {
  LOGGER_SYNC_SPSC,
  LOGGER_ASYNC_SPSC,
  LOGGER_SYNC_MPMC,
  LOGGER_ASYNC_MPMC,
} logger_mode_e;

typedef struct {
  logger_mode_e  e_mode;
  fifo_policy_e  e_policy;
  logger_level_e e_level;
  char           end_sign;
  const char    *prefix;
  void          *fp;
  u8            *fifo_buf, *tx_buf;
  size_t         fifo_buf_size, tx_buf_size;
} logger_cfg_t;

typedef struct {
  fifo_t fifo;
  u8    *fifo_buf;
  u8    *tx_buf;
  bool   busy;
} logger_lo_t;

typedef u64 (*logger_get_ts_f)(void);
typedef void (*logger_tx_f)(void *fp, const u8 *data, size_t size);

typedef struct {
  logger_get_ts_f f_get_ts;
  logger_tx_f     f_tx;
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
  lo->tx_buf   = cfg->tx_buf;

  fifo_init(&lo->fifo, lo->fifo_buf, cfg->fifo_buf_size, cfg->e_policy);
}

static inline void logger_flush(logger_t *logger) {
  DECL_LOGGER_PTRS(logger);

  u32 size = 0;
  u8  c;
  while (!lo->busy && ((cfg->e_mode == LOGGER_SYNC_SPSC || cfg->e_mode == LOGGER_ASYNC_SPSC)
                           ? fifo_spsc_out(&lo->fifo, &c, sizeof(c))
                           : fifo_mpmc_out(&lo->fifo, &c, sizeof(c))) != 0) {
    lo->tx_buf[size++] = c;
    if (c == cfg->end_sign || size == cfg->tx_buf_size) {
      ops->f_tx(cfg->fp, lo->tx_buf, size);
      size = 0;
      lo->busy =
          (cfg->e_mode == LOGGER_ASYNC_SPSC || cfg->e_mode == LOGGER_ASYNC_MPMC) ? true : false;
    }
  }
}

static inline void logger_write(logger_t *logger, const char *fmt, va_list args) {
  DECL_LOGGER_PTRS(logger);

  u8 buf[128];

  int size = snprintf((char *)buf, sizeof(buf), "[%llu] ", ops->f_get_ts());
  if (size < 0)
    return;

  if ((size_t)size >= sizeof(buf))
    size = sizeof(buf) - 1;

  int appended = vsnprintf((char *)buf + size, sizeof(buf) - size, fmt, args);
  if (appended < 0)
    return;

  if ((size_t)(size + appended) > sizeof(buf))
    appended = sizeof(buf) - size;

  (cfg->e_mode == LOGGER_SYNC_SPSC || cfg->e_mode == LOGGER_ASYNC_SPSC)
      ? fifo_spsc_in(&lo->fifo, buf, size + appended)
      : fifo_mpmc_in(&lo->fifo, buf, size + appended);
}

static inline void logger_data(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_DATA)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

static inline void logger_debug(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_DEBUG)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

static inline void logger_info(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_INFO)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

static inline void logger_warn(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_WARN)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

static inline void logger_error(logger_t *logger, const char *fmt, ...) {
  DECL_LOGGER_PTRS(logger);

  if (cfg->e_level > LOGGER_LEVEL_ERROR)
    return;

  va_list args;
  va_start(args, fmt);
  logger_write(logger, fmt, args);
  va_end(args);
}

#endif // !LOGGER_H
