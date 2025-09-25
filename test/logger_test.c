#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "../logger/logger.h"

logger_t logger;

u8 LOGGER_TX_BUF[1024];
u8 LOGGER_FIFO_BUF[1024 * 1024];

static inline void logger_stdout(void *fp, const u8 *data, size_t size) {
  fwrite(data, size, 1, fp);
  fflush(fp);
}

void *flush_thread_func(void *arg) {
  logger_t *logger = (logger_t *)arg;
  while (true)
    logger_flush(logger);

  delay_ms(1, YIELD);
  return NULL;
}

void *write_thread_func(void *arg) {
  u64 cnt = *(u64 *)arg;

  while (true) {
#ifdef _WIN32
    logger_debug(&logger, "Thread %lu,\tcnt: %llu\n", (unsigned long)GetCurrentThreadId(), cnt++);
#else
    logger_debug(&logger, "Thread %lu,\tcnt: %llu\n", (unsigned long)pthread_self(), cnt++);
#endif

    delay_ms(1, YIELD);
  }
  return NULL;
}

#define THREAD_COUNT 100

int main() {
  logger_cfg_t logger_cfg = {
      .e_mode        = LOGGER_SYNC,
      .e_level       = LOGGER_LEVEL_DEBUG,
      .end_sign      = '\n',
      .fp            = stdout,
      .fifo_buf      = LOGGER_FIFO_BUF,
      .fifo_buf_size = sizeof(LOGGER_FIFO_BUF),
      .tx_buf        = LOGGER_TX_BUF,
      .tx_buf_size   = sizeof(LOGGER_TX_BUF),
  };
  logger.ops.f_tx = logger_stdout;
  logger_init(&logger, logger_cfg);

  pthread_t flush_thread;
  pthread_create(&flush_thread, NULL, flush_thread_func, &logger);

  pthread_t threads[THREAD_COUNT];
  u64       thread_cnts[THREAD_COUNT] = {0};

  for (int i = 0; i < THREAD_COUNT; i++)
    pthread_create(&threads[i], NULL, write_thread_func, &thread_cnts[i]);

  for (int i = 0; i < THREAD_COUNT; i++)
    pthread_join(threads[i], NULL);

  pthread_join(flush_thread, NULL);

  return 0;
}
