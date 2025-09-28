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

#define RESET_OUT(ptr)       memset(&(ptr)->out, 0, sizeof((ptr)->out))

#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int : -!!(e); }))
#define __same_type(a, b)    __builtin_types_compatible_p(typeof(a), typeof(b))
#define __must_be_array(a)   BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#define ARRAY_SIZE(arr)      (sizeof(arr) / sizeof(arr[0]) + __must_be_array(arr))

#endif // !UTIL_H
