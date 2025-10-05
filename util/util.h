#ifndef UTIL_H
#define UTIL_H

#include "benchmark.h"
#include "def.h"
#include "printops.h"
#include "timeops.h"

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

#define RESET_OUT(ptr) memset(&(ptr)->out, 0, sizeof((ptr)->out))

#ifdef __MINGW32__
#define IS_SAME_TYPE(a, b) __mingw_types_compatible_p(__typeof__(a), __typeof__(b))
#else
#define IS_SAME_TYPE(a, b) __builtin_types_compatible_p(__typeof__(a), __typeof__(b))
#endif

#ifdef __cplusplus
#define BUILD_BUG_ON_ZERO(e) ((sizeof(char[1 - 2 * !!(e)])) - 1)
#else
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int : -!!(e); }))
#endif

#define MUST_BE_ARRAY(a) BUILD_BUG_ON_ZERO(IS_SAME_TYPE((a), &(a)[0]))
#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof(arr[0]) + MUST_BE_ARRAY(arr))

#endif // !UTIL_H
