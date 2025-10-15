#ifndef TIMEDEF_H
#define TIMEDEF_H

#ifdef __linux__
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <string.h>
#include <time.h>

#include "mathdef.h"
#include "typedef.h"

#define NANO_PER_SEC      1000000000ULL // 10^9
#define MICRO_PER_SEC     1000000ULL    // 10^6
#define MILLI_PER_SEC     1000ULL       // 10^3

#define WIN_TO_UNIX_EPOCH 116444736000000000ULL

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

static inline u64 get_mono_ts_us(void) { return get_mono_ts_ns() / 1000u; }

static inline u64 get_mono_ts_ms(void) { return get_mono_ts_ns() / 1000000u; }

static inline u64 get_mono_ts_s(void) { return get_mono_ts_ns() / 1000000000u; }

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

static inline u64 get_real_ts_us(void) { return get_real_ts_ns() / 1000u; }

static inline u64 get_real_ts_ms(void) { return get_real_ts_ns() / 1000000u; }

static inline u64 get_real_ts_s(void) { return get_real_ts_ns() / 1000000000u; }

typedef enum {
        SPIN,
        YIELD,
} delay_e;

static inline void spin(u32 us) {
        u64 start = get_mono_ts_us();
        while ((get_mono_ts_us() - start) < us) {
                asm volatile("nop" ::: "memory");
        }
}

#ifdef __linux__
static inline void yield(u32 ms) { usleep(1000u * ms); }
#elif defined(_WIN32)
#include <windows.h>
static inline void yield(u32 ms) { Sleep(ms); }
#else
static inline void yield(u32 ms) { spin(MS_TO_US(ms)); }
#endif

static inline void delay_us(u64 us) { spin(us); }

static inline void delay_ms(u64 ms, delay_e e_delay) {
        switch (e_delay) {
        case SPIN: {
                spin(MS_TO_US(ms));
        } break;
        case YIELD: {
                yield(ms);
        } break;
        default:
                break;
        }
}

static inline void delay_s(u64 s, delay_e e_delay) {
        switch (e_delay) {
        case SPIN: {
                spin(S_TO_US(s));
        } break;
        case YIELD: {
                yield(S_TO_MS(s));
        } break;
        default:
                break;
        }
}

#endif // !TIMEDEF_H
