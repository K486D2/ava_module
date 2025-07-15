#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <string.h>
#include <time.h>

#include "benchmark.h"
#include "def.h"
#include "printutil.h"

#define ATOMIC_EXEC(code)                                                                          \
  do {                                                                                             \
    volatile u32 _primask = __get_PRIMASK();                                                       \
    __disable_irq();                                                                               \
    {code};                                                                                        \
    __set_PRIMASK(_primask);                                                                       \
  } while (0)

#define ARG_UNUSED(x)        (void)(x)

#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int : -!!(e); }))
#define __same_type(a, b)    __builtin_types_compatible_p(typeof(a), typeof(b))
#define __must_be_array(a)   BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#define ARRAY_SIZE(arr)      (sizeof(arr) / sizeof(arr[0]) + __must_be_array(arr))

#define RESET_OUT(ptr)       memset(&(ptr)->out, 0, sizeof((ptr)->out))

static inline void swap_byte_order(void *data, const u32 len) {
  // u32 *p = (u32 *)data;
  for (u32 i = 0; i < len / 4; ++i) {
    // p[i] = ntohl(p[i]);
  }
}

static inline u64 get_mono_ts_ns(void) {
#ifdef __linux__
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return ts.tv_sec * NANO_PER_SEC + ts.tv_nsec;
#elif defined(_WIN32)
  LARGE_INTEGER frequency, counter;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&counter);
  return counter.QuadPart * 1000000000ull / frequency.QuadPart;
#endif
  return 0ull;
}

static inline u64 get_mono_ts_us(void) {
  return get_mono_ts_ns() / 1000u;
}
static inline u64 get_mono_ts_ms(void) {
  return get_mono_ts_ns() / 1000000u;
}
static inline u64 get_mono_ts_s(void) {
  return get_mono_ts_ns() / 1000000000u;
}

static inline u64 get_real_ts_ns(void) {
#ifdef __linux__
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec * NANO_PER_SEC + ts.tv_nsec;
#elif defined(_WIN32)
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  ULARGE_INTEGER uli;
  uli.LowPart  = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;

  return (uli.QuadPart - WIN_TO_UNIX_EPOCH) * 100u;
#endif
  return 0ull;
}

static inline u64 get_real_ts_us(void) {
  return get_real_ts_ns() / 1000u;
}
static inline u64 get_real_ts_ms(void) {
  return get_real_ts_ns() / 1000000u;
}
static inline u64 get_real_ts_s(void) {
  return get_real_ts_ns() / 1000000000u;
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

#ifdef __cplusplus
}
#endif

#endif // !UTIL_H
