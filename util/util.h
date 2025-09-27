#ifndef UTIL_H
#define UTIL_H

#include "benchmark.h"
#include "def.h"
#include "printutil.h"

#define AT(addr) __attribute__((section(addr)))

#define ATOMIC_EXEC(code)                                                                          \
  do {                                                                                             \
    volatile u32 _primask = __get_PRIMASK();                                                       \
    __disable_irq();                                                                               \
    {code};                                                                                        \
    __set_PRIMASK(_primask);                                                                       \
  } while (0)

#define ARG_UNUSED(arg) (void)(arg)

#define ARG_CHECK(arg)                                                                             \
  do {                                                                                             \
    if ((arg) == NULL)                                                                             \
      return -MEINVAL;                                                                             \
  } while (0)

#define CFG_CHECK(p, f_init)                                                                       \
  do {                                                                                             \
    if (memcmp(&((p)->cfg), &((p)->lo.cfg), sizeof((p)->cfg)) != 0)                                \
      f_init((p), (p)->cfg);                                                                       \
  } while (0)

#define RESET_OUT(ptr)       memset(&(ptr)->out, 0, sizeof((ptr)->out))

#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int : -!!(e); }))
#define __same_type(a, b)    __builtin_types_compatible_p(typeof(a), typeof(b))
#define __must_be_array(a)   BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#define ARRAY_SIZE(arr)      (sizeof(arr) / sizeof(arr[0]) + __must_be_array(arr))

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

constexpr auto    memory_order_relaxed = std::memory_order_relaxed;
constexpr auto    memory_order_acquire = std::memory_order_acquire;
constexpr auto    memory_order_release = std::memory_order_release;
constexpr auto    memory_order_acq_rel = std::memory_order_acq_rel;
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_BitScanReverse64)
static inline u32 clz64(u64 x) {
  unsigned long index;
  if (_BitScanReverse64(&index, x))
    return 63 - index; // x != 0 时返回前导零数
  return 64;           // x == 0 时返回 64
}
#elif defined(__GNUC__) || defined(__clang__)
static inline u32 clz64(u64 x) {
  if (x == 0)
    return 64;
  return __builtin_clzll(x);
}
#elif defined(__ARMCC_VERSION) || defined(__ARM_COMPILER)
#include <arm_acle.h>
static inline u32 clz64(u64 x) {
  if (x == 0)
    return 64;
  return __clz(x >> 32) * (x >> 32 ? 1 : 0) + __clz(x & 0xFFFFFFFF);
}
#else
static inline u32 clz64(u64 x) {
  if (x == 0)
    return 64;
  u32 n = 0;
  for (u8 i = 63; i >= 0; i--) {
    if ((x >> i) & 1)
      break;
    n++;
  }
  return n;
}
#endif

static inline void swap_byte_order(void *data, const u32 size) {
  u32 *p = (u32 *)data;
  for (u32 i = 0; i < size / 4; ++i) {
    u32 val = p[i];
    p[i]    = (val >> 24) | ((val >> 8) & 0x0000FF00) | ((val << 8) & 0x00FF0000) | (val << 24);
  }
}

#endif // !UTIL_H
