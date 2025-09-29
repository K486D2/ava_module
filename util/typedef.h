#ifndef TYPEDEF_H
#define TYPEDEF_H

typedef unsigned char u8;
typedef signed char   i8;

typedef unsigned short u16;
typedef signed short   i16;

typedef unsigned int u32;
typedef signed int   i32;

//* on Windows, (unsigned long) is 32bit
typedef unsigned long long u64;
typedef signed long long   i64;

typedef float  f32;
typedef double f64;

#ifndef bool
#define bool u8
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

/* 原子操作 */
#ifndef __cplusplus
#include <stdatomic.h>
#define atomic_t(x) _Atomic x
#else
#include <atomic>
#define atomic_t(x)                    std::atomic<x>
#define atomic_store(a, v)             std::atomic_store(a, v)
#define atomic_load(a)                 std::atomic_load(a)
#define atomic_load_explicit(a, m)     std::atomic_load_explicit(a, m)
#define atomic_store_explicit(a, v, m) std::atomic_store_explicit(a, v, m)
#define atomic_compare_exchange_weak_explicit(a, o, n, s, f)                                       \
  std::atomic_compare_exchange_weak_explicit(a, o, n, s, f)

constexpr auto memory_order_relaxed = std::memory_order_relaxed;
constexpr auto memory_order_acquire = std::memory_order_acquire;
constexpr auto memory_order_release = std::memory_order_release;
constexpr auto memory_order_acq_rel = std::memory_order_acq_rel;
#endif

#define SPINLOCK_BACKOFF_MIN 4
#define SPINLOCK_BACKOFF_MAX 128
#if defined(__x86_64__)
#define SPINLOCK_BACKOFF_HOOK __asm volatile("pause" ::: "memory")
#else
#define SPINLOCK_BACKOFF_HOOK
#endif
#define SPINLOCK_BACKOFF(count)                                                                    \
  do {                                                                                             \
    for (int __i = (count); __i != 0; __i--)                                                       \
      SPINLOCK_BACKOFF_HOOK;                                                                       \
    if ((count) < SPINLOCK_BACKOFF_MAX)                                                            \
      (count) += (count);                                                                          \
  } while (0);

typedef struct {
  u32 u;
  u32 v;
  u32 w;
} u32_uvw_t;

typedef struct {
  i32 u;
  i32 v;
  i32 w;
} i32_uvw_t;

typedef struct {
  f32 u;
  f32 v;
  f32 w;
} f32_uvw_t;

typedef struct {
  f32 a;
  f32 b;
} f32_ab_t;

typedef struct {
  f32 d;
  f32 q;
} f32_dq_t;

typedef struct {
  u32 npp;
  f32 ld;
  f32 lq;
  f32 rs;
  f32 psi; // Wb
  f32 wc;  // 电流环带宽
  f32 j;   // 转子惯量
  f32 cur2tor[4], tor2cur[4], max_tor;
} motor_cfg_t;

typedef struct {
  /* ADC */
  u32 adc_full_cnt;
  u32 adc_cali_cnt_max, theta_cali_cnt_max;
  f32 cur_range, vbus_range;
  f32 adc2cur, adc2vbus;
  u32 cur_gain;
  f32 cur_offset;

  /* PWM */
  u32 timer_freq;
  u32 pwm_freq;
  u32 pwm_full_cnt;
  f32 mi;
  f32 f32_pwm_min, f32_pwm_max;
} periph_cfg_t;

#endif // !TYPEDEF_H
