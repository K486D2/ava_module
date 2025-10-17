#ifndef MARCO_H
#define MARCO_H

#define AT(addr) __attribute__((section(addr)))
#define OPTNONE  __attribute__((optnone))

#define ATOMIC_EXEC(code)                                \
        do {                                             \
                volatile u32 _primask = __get_PRIMASK(); \
                __disable_irq();                         \
                {code};                                  \
                __set_PRIMASK(_primask);                 \
        } while (0)

#define ARG_UNUSED(arg) (void)(arg)

#define ARG_CHECK(arg)                   \
        do {                             \
                if ((arg) == NULL)       \
                        return -MEINVAL; \
        } while (0)

#define CFG_CHECK(p, f_init)                                                    \
        do {                                                                    \
                if (memcmp(&((p)->cfg), &((p)->lo.cfg), sizeof((p)->cfg)) != 0) \
                        f_init((p), (p)->cfg);                                  \
        } while (0)

#define RESET_OUT(ptr) memset(&(ptr)->out, 0, sizeof((ptr)->out))

#define DECL_PTRS_1(p, a1)              \
        typeof((p)->a1) *a1 = &(p)->a1; \
        ARG_UNUSED(a1);
#define DECL_PTRS_2(p, a1, a2)                               DECL_PTRS_1(p, a1) DECL_PTRS_1(p, a2)
#define DECL_PTRS_3(p, a1, a2, a3)                           DECL_PTRS_2(p, a1, a2) DECL_PTRS_1(p, a3)
#define DECL_PTRS_4(p, a1, a2, a3, a4)                       DECL_PTRS_3(p, a1, a2, a3) DECL_PTRS_1(p, a4)
#define DECL_PTRS_5(p, a1, a2, a3, a4, a5)                   DECL_PTRS_4(p, a1, a2, a3, a4) DECL_PTRS_1(p, a5)
#define DECL_PTRS_6(p, a1, a2, a3, a4, a5, a6)               DECL_PTRS_5(p, a1, a2, a3, a4, a5) DECL_PTRS_1(p, a6)
#define DECL_PTRS_7(p, a1, a2, a3, a4, a5, a6, a7)           DECL_PTRS_6(p, a1, a2, a3, a4, a5, a6) DECL_PTRS_1(p, a7)
#define DECL_PTRS_8(p, a1, a2, a3, a4, a5, a6, a7, a8)       DECL_PTRS_7(p, a1, a2, a3, a4, a5, a6, a7) DECL_PTRS_1(p, a8)

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME

#define DUMMY(...)
#define DECL_PTRS(p, ...)      \
        GET_MACRO(__VA_ARGS__, \
                  DECL_PTRS_8, \
                  DECL_PTRS_7, \
                  DECL_PTRS_6, \
                  DECL_PTRS_5, \
                  DECL_PTRS_4, \
                  DECL_PTRS_3, \
                  DECL_PTRS_2, \
                  DECL_PTRS_1, \
                  DUMMY)       \
        (p, __VA_ARGS__)

#define DECL_PTR_RENAME(p, name) \
        typeof((p)) name = p;    \
        ARG_UNUSED(name);

#define HAPI static inline

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

#define MUST_BE_ARRAY(a)                BUILD_BUG_ON_ZERO(IS_SAME_TYPE((a), &(a)[0]))
#define ARRAY_SIZE(arr)                 (sizeof(arr) / sizeof(arr[0]) + MUST_BE_ARRAY(arr))

#define CONTAINER_OF(ptr, type, member) (type *)((char *)(ptr) - offsetof(type, member))

#endif // !MARCO_H
