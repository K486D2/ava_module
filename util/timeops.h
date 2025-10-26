#ifndef TIMEOPS_H
#define TIMEOPS_H

#ifdef __linux__
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <time.h>

#include "macrodef.h"
#include "typedef.h"

#define NANO_PER_SEC      (1000000000ULL) // 10^9
#define MICRO_PER_SEC     (1000000ULL)    // 10^6
#define MILLI_PER_SEC     (1000ULL)       // 10^3

#define WIN_TO_UNIX_EPOCH (116444736000000000ULL)

#define NS2US(ns)         ((ns) / 1000.0F)
#define NS2MS(ns)         ((ns) / 1000000.0F)
#define NS2S(ns)          ((ns) / 1000000000.0F)
#define US2NS(us)         ((us) * 1000U)
#define US2MS(us)         ((us) / 1000.0F)
#define US2S(us)          ((us) / 1000000.0F)
#define MS2NS(ms)         ((ms) * 1000000U)
#define MS2US(ms)         ((ms) * 1000U)
#define MS2S(ms)          ((ms) / 1000.0F)
#define S2NS(s)           ((s) * 1000000000U)
#define S2US(s)           ((s) * 1000000U)
#define S2MS(s)           ((s) * 1000U)

#define HZ2S(hz)          (1.0F / (hz))
#define HZ2MS(hz)         (1.0F / (hz) * 1000U)
#define HZ2US(hz)         (1.0F / (hz) * 1000000U)

HAPI u64
get_mono_ts_ns(void)
{
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

HAPI u64
get_mono_ts_us(void)
{
        return get_mono_ts_ns() / 1000u;
}

HAPI u64
get_mono_ts_ms(void)
{
        return get_mono_ts_ns() / 1000000u;
}

HAPI u64
get_mono_ts_s(void)
{
        return get_mono_ts_ns() / 1000000000u;
}

HAPI u64
get_real_ts_ns(void)
{
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

HAPI u64
get_real_ts_us(void)
{
        return get_real_ts_ns() / 1000u;
}

HAPI u64
get_real_ts_ms(void)
{
        return get_real_ts_ns() / 1000000u;
}

HAPI u64
get_real_ts_s(void)
{
        return get_real_ts_ns() / 1000000000u;
}

typedef enum {
        SPIN,
        YIELD,
} delay_e;

HAPI void
spin(u32 us)
{
        u64 start = get_mono_ts_us();
        while ((get_mono_ts_us() - start) < us) {
                asm volatile("nop" ::: "memory");
        }
}

#ifdef __linux__
HAPI void
yield(u32 ms)
{
        usleep(1000u * ms);
}
#elif defined(_WIN32)
#include <windows.h>
HAPI void
yield(u32 ms)
{
        Sleep(ms);
}
#else
HAPI void
yield(u32 ms)
{
        spin(MS2US(ms));
}
#endif

HAPI void
delay_us(u64 us)
{
        spin(us);
}

HAPI void
delay_ms(u64 ms, delay_e e_delay)
{
        switch (e_delay) {
                case SPIN: {
                        spin(MS2US(ms));
                } break;
                case YIELD: {
                        yield(ms);
                } break;
                default:
                        break;
        }
}

HAPI void
delay_s(u64 s, delay_e e_delay)
{
        switch (e_delay) {
                case SPIN: {
                        spin(S2US(s));
                } break;
                case YIELD: {
                        yield(S2MS(s));
                } break;
                default:
                        break;
        }
}

#endif // !TIMEOPS_H
