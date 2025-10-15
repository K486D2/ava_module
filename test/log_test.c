#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "log/log.h"

log_t g_log;

#define WRITE_THREAD_NUM 1000
u64 PRODUCERS_CNTS[WRITE_THREAD_NUM];

u8       LOGGER_FLUSH_BUF[128];
u8       LOGGER_BUF[1024 * 1024];
mpsc_p_t PRODUCERS[WRITE_THREAD_NUM];

static inline void log_stdout(void *fp, const u8 *src, size_t nbytes) {
        fwrite(src, nbytes, 1, fp);
        fflush(fp);
}

void *flush_thread_func(void *arg) {
        ARG_UNUSED(arg);

        while (true)
                log_flush(&g_log);
        return NULL;
}

void *write_thread_func(void *arg) {
        u64 id = *(u64 *)arg;

        while (true) {
#ifdef _WIN32
                log_debug(&log,
                          "Thread %10u, cnt: %10llu\n",
                          (u32)GetCurrentThreadId(),
                          PRODUCERS_CNTS[id]++);
#else
                log_debug(&g_log,
                          id,
                          "Thread %10u, cnt: %10llu\n",
                          (u32)pthread_self(),
                          PRODUCERS_CNTS[id]++);
#endif

                delay_ms(1, YIELD);
        }
        return NULL;
}

int main() {
        log_cfg_t log_cfg = {
            .e_mode     = LOG_MODE_SYNC,
            .e_level    = LOG_LEVEL_DEBUG,
            .end_sign   = '\n',
            .fp         = stdout,
            .buf        = (void *)LOGGER_BUF,
            .cap        = sizeof(LOGGER_BUF),
            .flush_buf  = LOGGER_FLUSH_BUF,
            .flush_cap  = sizeof(LOGGER_FLUSH_BUF),
            .producers  = (mpsc_p_t *)&PRODUCERS,
            .nproducers = ARRAY_SIZE(PRODUCERS),
        };
        g_log.ops.f_flush  = log_stdout;
        g_log.ops.f_get_ts = get_mono_ts_us;
        log_init(&g_log, log_cfg);

        pthread_t flush_thread;
        pthread_create(&flush_thread, NULL, flush_thread_func, NULL);

        u64       thread_ids[WRITE_THREAD_NUM];
        pthread_t write_thread[WRITE_THREAD_NUM];
        for (u32 i = 0; i < WRITE_THREAD_NUM - 1; i++) {
                thread_ids[i] = i;
                pthread_create(&write_thread[i], NULL, write_thread_func, &thread_ids[i]);
        }

        for (u32 i = 0; i < WRITE_THREAD_NUM - 1; i++)
                pthread_join(write_thread[i], NULL);

        pthread_join(flush_thread, NULL);

        return 0;
}
