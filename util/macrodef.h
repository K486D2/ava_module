#ifndef MACRODEF_H
#define MACRODEF_H

#include <stddef.h>
#include <string.h>

#define AT(addr) __attribute__((section(addr)))
#define OPTNONE  __attribute__((optnone))

#define ATOMIC_EXEC(code)                               \
        do {                                            \
                volatile u32 primask = __get_PRIMASK(); \
                __disable_irq();                        \
                {code};                                 \
                __set_PRIMASK(primask);                 \
        } while (0)

#define HAPI            static inline

#define ARG_UNUSED(arg) (void)(arg)

#define ARG_CHECK(arg)                   \
        do {                             \
                if ((arg) == NULL)       \
                        return -MEINVAL; \
        } while (0)

#define CFG_CHECK(ptr, f_init)                                                        \
        do {                                                                          \
                if (memcmp(&((ptr)->cfg), &((ptr)->lo.cfg), sizeof((ptr)->cfg)) != 0) \
                        f_init((ptr), (ptr)->cfg);                                    \
        } while (0)

#define RESET_OUT(ptr) memset(&(ptr)->out, 0, sizeof((ptr)->out))

#define DECL_PTRS_1(ptr, a1)                \
        typeof((ptr)->a1) *a1 = &(ptr)->a1; \
        ARG_UNUSED(a1);
#define DECL_PTRS_2(ptr, a1, a2)                             DECL_PTRS_1(ptr, a1) DECL_PTRS_1(ptr, a2)
#define DECL_PTRS_3(ptr, a1, a2, a3)                         DECL_PTRS_2(ptr, a1, a2) DECL_PTRS_1(ptr, a3)
#define DECL_PTRS_4(ptr, a1, a2, a3, a4)                     DECL_PTRS_3(ptr, a1, a2, a3) DECL_PTRS_1(ptr, a4)
#define DECL_PTRS_5(ptr, a1, a2, a3, a4, a5)                 DECL_PTRS_4(ptr, a1, a2, a3, a4) DECL_PTRS_1(ptr, a5)
#define DECL_PTRS_6(ptr, a1, a2, a3, a4, a5, a6)             DECL_PTRS_5(ptr, a1, a2, a3, a4, a5) DECL_PTRS_1(ptr, a6)
#define DECL_PTRS_7(ptr, a1, a2, a3, a4, a5, a6, a7)         DECL_PTRS_6(ptr, a1, a2, a3, a4, a5, a6) DECL_PTRS_1(ptr, a7)
#define DECL_PTRS_8(ptr, a1, a2, a3, a4, a5, a6, a7, a8)     DECL_PTRS_7(ptr, a1, a2, a3, a4, a5, a6, a7) DECL_PTRS_1(ptr, a8)

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME

#define DECL_PTRS(ptr, ...)    \
        GET_MACRO(__VA_ARGS__, \
                  DECL_PTRS_8, \
                  DECL_PTRS_7, \
                  DECL_PTRS_6, \
                  DECL_PTRS_5, \
                  DECL_PTRS_4, \
                  DECL_PTRS_3, \
                  DECL_PTRS_2, \
                  DECL_PTRS_1, \
                  ...)         \
        (ptr, __VA_ARGS__)

#define DECL_PTR_RENAME(ptr, name) \
        typeof((ptr)) name = ptr;  \
        ARG_UNUSED(name);

/* 原子操作 */
#ifdef __cplusplus
#include <atomic>
#define ATOMIC(type)                              std::atomic<type>
#define ATOMIC_LOAD(a)                            std::atomic_load(a)
#define ATOMIC_LOAD_EXPLICIT(a, m)                std::atomic_load_explicit(a, m)
#define ATOMIC_STORE(a, v)                        std::atomic_store(a, v)
#define ATOMIC_STORE_EXPLICIT(a, v, m)            std::atomic_store_explicit(a, v, m)
#define ATOMIC_CAS_WEAK_EXPLICIT(a, o, n, s, f)   std::atomic_compare_exchange_weak_explicit(a, o, n, s, f)
#define ATOMIC_CAS_STRONG_EXPLICIT(a, o, n, s, f) std::atomic_compare_exchange_strong_explicit(a, o, n, s, f)
#define ATOMIC_EXCHANGE(a, v)                     std::atomic_exchange(a, v)
constexpr auto memory_order_relaxed = std::memory_order_relaxed;
constexpr auto memory_order_acquire = std::memory_order_acquire;
constexpr auto memory_order_release = std::memory_order_release;
constexpr auto memory_order_acq_rel = std::memory_order_acq_rel;
#else
#include <stdatomic.h>
#define ATOMIC(type)                              _Atomic type
#define ATOMIC_LOAD(a)                            atomic_load(a)
#define ATOMIC_LOAD_EXPLICIT(a, m)                atomic_load_explicit(a, m)
#define ATOMIC_STORE(a, v)                        atomic_store(a, v)
#define ATOMIC_STORE_EXPLICIT(a, v, m)            atomic_store_explicit(a, v, m)
#define ATOMIC_CAS_WEAK_EXPLICIT(a, o, n, s, f)   atomic_compare_exchange_weak_explicit(a, o, n, s, f)
#define ATOMIC_CAS_STRONG_EXPLICIT(a, o, n, s, f) atomic_compare_exchange_strong_explicit(a, o, n, s, f)
#define ATOMIC_EXCHANGE(a, v)                     atomic_exchange(a, v)
#endif

#define SPINLOCK_BACKOFF_MIN (4)
#define SPINLOCK_BACKOFF_MAX (128)
#if defined(__x86_64__)
#define SPINLOCK_BACKOFF_HOOK __asm volatile("pause" ::: "memory")
#else
#define SPINLOCK_BACKOFF_HOOK
#endif
#define SPINLOCK_BACKOFF(cnt)                     \
        do {                                      \
                for (int i = (cnt); i != 0; i--)  \
                        SPINLOCK_BACKOFF_HOOK;    \
                if ((cnt) < SPINLOCK_BACKOFF_MAX) \
                        (cnt) += (cnt);           \
        } while (0);

#define SPIN_LOCK(lock_ptr)                                                                                                 \
        do {                                                                                                                \
                u32 expected;                                                                                               \
                int backoff = SPINLOCK_BACKOFF_MIN;                                                                         \
                for (;;) {                                                                                                  \
                        expected = 0;                                                                                       \
                        if (ATOMIC_CAS_WEAK_EXPLICIT((lock_ptr), &expected, 1, memory_order_acquire, memory_order_relaxed)) \
                                break;                                                                                      \
                        SPINLOCK_BACKOFF(backoff);                                                                          \
                }                                                                                                           \
        } while (0)

#define SPIN_UNLOCK(lock_ptr) ATOMIC_STORE_EXPLICIT((lock_ptr), 0, memory_order_release)

#define IS_SAME_TYPE(a, b)    __builtin_types_compatible_p(__typeof__(a), __typeof__(b))

#ifdef __cplusplus
#define BUILD_BUG_ON_ZERO(e) ((sizeof(char[1 - 2 * !!(e)])) - 1)
#else
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int : -!!(e); }))
#endif

#define MUST_BE_ARRAY(arr)              BUILD_BUG_ON_ZERO(IS_SAME_TYPE((arr), &(arr)[0]))
#define ARRAY_LEN(arr)                  (sizeof(arr) / sizeof(arr[0]) + MUST_BE_ARRAY(arr))

#define CONTAINER_OF(ptr, type, member) (type *)((char *)(ptr) - offsetof(type, member))

#endif // !MACRODEF_H
