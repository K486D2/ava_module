#ifndef UTIL_H
#define UTIL_H

#include "benchmark.h"
#include "def.h"
#include "printutil.h"

#define AT(x) __attribute__((section(x)))

#define ATOMIC_EXEC(code)                                                                          \
  do {                                                                                             \
    volatile u32 _primask = __get_PRIMASK();                                                       \
    __disable_irq();                                                                               \
    {code};                                                                                        \
    __set_PRIMASK(_primask);                                                                       \
  } while (0)

#define ARG_UNUSED(x) (void)(x)

#define ARG_CHECK(x)                                                                               \
  do {                                                                                             \
    if ((x) == NULL)                                                                               \
      return -MEINVAL;                                                                             \
  } while (0)

#define CFG_CHECK(p, f_init)                                                                       \
  do {                                                                                             \
    if (memcmp(&((p)->cfg), &((p)->lo.cfg), sizeof((p)->cfg)) != 0)                                \
      f_init((p), (p)->cfg);                                                                       \
  } while (0)

#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int : -!!(e); }))
#define __same_type(a, b)    __builtin_types_compatible_p(typeof(a), typeof(b))
#define __must_be_array(a)   BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#define ARRAY_SIZE(arr)      (sizeof(arr) / sizeof(arr[0]) + __must_be_array(arr))

#define RESET_OUT(ptr)       memset(&(ptr)->out, 0, sizeof((ptr)->out))

static inline void swap_byte_order(void *data, const u32 size) {
  u32 *p = (u32 *)data;
  for (u32 i = 0; i < size / 4; ++i) {
    u32 val = p[i];
    p[i]    = (val >> 24) | ((val >> 8) & 0x0000FF00) | ((val << 8) & 0x00FF0000) | (val << 24);
  }
}

static inline i32 format_ts_ms(u64 ts_ms, char *buf, u32 len) {
  time_t    sec = ts_ms / 1000u;
  int       ms  = ts_ms % 1000u;
  struct tm tm_time;

#ifdef __linux__
  localtime_r(&sec, &tm_time);
#elif defined(_WIN32)
  localtime_s(&tm_time, &sec);
#else
  struct tm *tmp = localtime(&sec);
  if (tmp)
    tm_time = *tmp;
#endif

  // "YYYY-MM-DDTHH:MM:SS.mmm"
  return snprintf(buf,
                  len,
                  "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                  tm_time.tm_year + 1900u,
                  tm_time.tm_mon + 1u,
                  tm_time.tm_mday,
                  tm_time.tm_hour,
                  tm_time.tm_min,
                  tm_time.tm_sec,
                  ms);
}

#endif // !UTIL_H
