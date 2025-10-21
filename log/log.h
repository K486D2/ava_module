#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ds/mpsc.h"
#include "util/marcodef.h"
#include "util/typedef.h"

typedef enum {
        LOG_LEVEL_DATA,  // 数据
        LOG_LEVEL_DEBUG, // 调试
        LOG_LEVEL_INFO,  // 一般
        LOG_LEVEL_WARN,  // 警告
        LOG_LEVEL_ERROR, // 错误
} log_level_e;

typedef enum {
        LOG_MODE_SYNC,
        LOG_MODE_ASYNC,
} log_mode_e;

typedef struct {
        u64 ts;
        usz id;
        usz msg_nbytes;
} log_entry_t;

typedef struct {
        log_mode_e  e_mode;
        log_level_e e_level;
        char        end_sign;
        void       *fp;
        void       *buf;
        size_t      cap;
        u8         *flush_buf;
        size_t      flush_cap;
        mpsc_p_t   *producers;
        size_t      nproducers;
} log_cfg_t;

typedef struct {
        mpsc_t mpsc;
        bool   busy;
} log_lo_t;

typedef u64 (*log_get_ts_f)(void);
typedef void (*log_flush_f)(void *fp, const u8 *src, size_t nbytes);

typedef struct {
        log_get_ts_f f_get_ts;
        log_flush_f  f_flush;
} log_ops_t;

typedef struct {
        log_cfg_t cfg;
        log_lo_t  lo;
        log_ops_t ops;
} log_t;

HAPI void log_init(log_t *log, log_cfg_t log_cfg);
HAPI void log_write(log_t *log, usz id, const char *fmt, va_list args);
HAPI void log_flush(log_t *log);

HAPI void log_data(log_t *log, usz id, const char *fmt, ...);
HAPI void log_debug(log_t *log, usz id, const char *fmt, ...);
HAPI void log_info(log_t *log, usz id, const char *fmt, ...);
HAPI void log_warn(log_t *log, usz id, const char *fmt, ...);
HAPI void log_error(log_t *log, usz id, const char *fmt, ...);

HAPI void
log_init(log_t *log, log_cfg_t log_cfg)
{
        DECL_PTRS(log, cfg, lo);

        *cfg = log_cfg;
        mpsc_init(&lo->mpsc, cfg->buf, cfg->cap, cfg->producers, cfg->nproducers);
}

HAPI void
log_write(log_t *log, usz id, const char *fmt, va_list args)
{
        DECL_PTRS(log, cfg, ops, lo);

        va_list args_entry;
        va_copy(args_entry, args);
        log_entry_t entry = {
            .ts         = ops->f_get_ts(),
            .id         = id,
            .msg_nbytes = (usz)vsnprintf(NULL, 0, fmt, args_entry) + 1,
        };
        va_end(args_entry);

        usz total_nbytes = sizeof(entry) + entry.msg_nbytes;
        if (total_nbytes > cfg->cap)
                return;

        mpsc_p_t *p      = mpsc_reg(&lo->mpsc, id);
        isz       offset = mpsc_acquire(&lo->mpsc, p, total_nbytes);
        if (offset < 0) {
                mpsc_unreg(p);
                return;
        }

        u8 *buf = (u8 *)lo->mpsc.buf + (usz)offset;
        memcpy(buf, &entry, sizeof(entry));

        va_list args_msg;
        va_copy(args_msg, args);
        int msg_nbytes = vsnprintf((char *)buf + sizeof(entry), entry.msg_nbytes, fmt, args_msg);
        va_end(args_msg);

        mpsc_push(p);
        mpsc_unreg(p);

        if (msg_nbytes != (int)entry.msg_nbytes)
                return;
}

HAPI void
log_flush(log_t *log)
{
        DECL_PTRS(log, cfg, ops, lo);

        while (!lo->busy) {
                log_entry_t entry        = {0};
                usz         entry_nbytes = mpsc_read(&lo->mpsc, &entry, sizeof(entry));
                if (entry_nbytes == 0)
                        break;

#ifdef MCU
                usz total_nbytes = snprintf((char *)cfg->flush_buf, cfg->flush_cap, "[%llu][%u]", entry.ts, entry.id);
#else
                usz total_nbytes = snprintf((char *)cfg->flush_buf, cfg->flush_cap, "[%llu][%llu]", entry.ts, entry.id);
#endif

                total_nbytes += mpsc_read(&lo->mpsc, cfg->flush_buf + total_nbytes, entry.msg_nbytes);

                ops->f_flush(cfg->fp, cfg->flush_buf, total_nbytes);
                lo->busy = (cfg->e_mode == LOG_MODE_ASYNC);
        }
}

HAPI void
log_data(log_t *log, usz id, const char *fmt, ...)
{
        DECL_PTRS(log, cfg);

        if (cfg->e_level > LOG_LEVEL_DATA)
                return;

        va_list args;
        va_start(args, fmt);
        log_write(log, id, fmt, args);
        va_end(args);
}

HAPI void
log_debug(log_t *log, usz id, const char *fmt, ...)
{
        DECL_PTRS(log, cfg);

        if (cfg->e_level > LOG_LEVEL_DEBUG)
                return;

        va_list args;
        va_start(args, fmt);
        log_write(log, id, fmt, args);
        va_end(args);
}

HAPI void
log_info(log_t *log, usz id, const char *fmt, ...)
{
        DECL_PTRS(log, cfg);

        if (cfg->e_level > LOG_LEVEL_INFO)
                return;

        va_list args;
        va_start(args, fmt);
        log_write(log, id, fmt, args);
        va_end(args);
}

HAPI void
log_warn(log_t *log, usz id, const char *fmt, ...)
{
        DECL_PTRS(log, cfg);

        if (cfg->e_level > LOG_LEVEL_WARN)
                return;

        va_list args;
        va_start(args, fmt);
        log_write(log, id, fmt, args);
        va_end(args);
}

HAPI void
log_error(log_t *log, usz id, const char *fmt, ...)
{
        DECL_PTRS(log, cfg);

        if (cfg->e_level > LOG_LEVEL_ERROR)
                return;

        va_list args;
        va_start(args, fmt);
        log_write(log, id, fmt, args);
        va_end(args);
}

#endif // !LOGGER_H
