#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "logger/logger.h"

logger_t logger;

#define THREAD_COUNT 10000

u8       LOGGER_FLUSH_BUF[128];
u8       LOGGER_BUF[1024];
mpsc_p_t PRODUCERS[THREAD_COUNT];

static inline void logger_stdout(void *fp, const u8 *src, size_t nbytes) {
  fwrite(src, nbytes, 1, fp);
}

void *flush_thread_func(void *arg) {
  logger_t *logger = (logger_t *)arg;
  while (true)
    logger_flush(logger);
  return NULL;
}

void *write_thread_func(void *arg) {
  u64 cnt = *(u64 *)arg;

  while (true) {
#ifdef _WIN32
    logger_debug(&logger, "Thread %5u, cnt: %llu\n", (u32)GetCurrentThreadId(), cnt);
#else
    logger_debug(&logger, cnt, "Thread %10u, cnt: %llu\n", (u32)pthread_self(), cnt);
#endif

    delay_ms(1, YIELD);
  }
  return NULL;
}

int main() {
  logger_cfg_t logger_cfg = {
      .e_mode  = LOGGER_SYNC,
      .e_level = LOGGER_LEVEL_DEBUG,
      // .e_policy      = SPSC_POLICY_REJECT,
      .end_sign       = '\n',
      .fp             = stdout,
      .logger_buf     = &LOGGER_BUF,
      .logger_buf_cap = sizeof(LOGGER_BUF),
      .producers      = &PRODUCERS,
      .nproducers     = ARRAY_SIZE(PRODUCERS),
      .flush_buf      = LOGGER_FLUSH_BUF,
      .flush_buf_cap  = sizeof(LOGGER_FLUSH_BUF),
  };
  logger.ops.f_flush  = logger_stdout;
  logger.ops.f_get_ts = get_mono_ts_us;
  logger_init(&logger, logger_cfg);

  pthread_t flush_thread;
  pthread_create(&flush_thread, NULL, flush_thread_func, &logger);

  pthread_t threads[THREAD_COUNT];
  u64       thread_cnts[THREAD_COUNT] = {0};

  for (int i = 0; i < THREAD_COUNT; i++)
    pthread_create(&threads[i], NULL, write_thread_func, &i);

  for (int i = 0; i < THREAD_COUNT; i++)
    pthread_join(threads[i], NULL);

  pthread_join(flush_thread, NULL);

  return 0;
}
