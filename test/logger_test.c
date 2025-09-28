#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#define FIFO_NODE_SIZE 128
#include "container/fifo.h"
#include "logger/logger.h"

logger_t logger;

u8          LOGGER_TX_BUF[128];
fifo_node_t LOGGER_FIFO_BUF[1 * 1024];

static inline void logger_stdout(void *fp, const u8 *data, size_t size) {
  fwrite(data, size, 1, fp);
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
    logger_debug(&logger,
                 "Thread %5u, cnt: %llu, fifo_free: %zu\n",
                 (u32)GetCurrentThreadId(),
                 cnt++,
                 fifo_free(&logger.lo.fifo));
#else
    logger_debug(&logger,
                 "Thread %10u, cnt: %llu, fifo_free: %zu\n",
                 (u32)pthread_self(),
                 cnt++,
                 fifo_free(&logger.lo.fifo));
#endif

    delay_ms(1, YIELD);
  }
  return NULL;
}

#define THREAD_COUNT 1000

int main() {
  logger_cfg_t logger_cfg = {
      .e_logger_mode  = LOGGER_SYNC,
      .e_logger_level = LOGGER_LEVEL_DEBUG,
      .e_fifo_mode    = FIFO_MODE_MPMC,
      .e_fifo_policy  = FIFO_POLICY_REJECT,
      .end_sign       = '\n',
      .fp             = stdout,
      .fifo_buf       = (void *)LOGGER_FIFO_BUF,
      .fifo_buf_cap   = ARRAY_SIZE(LOGGER_FIFO_BUF),
      .tx_buf         = (void *)LOGGER_TX_BUF,
      .tx_buf_cap     = sizeof(LOGGER_TX_BUF),
  };
  logger.ops.f_tx     = logger_stdout;
  logger.ops.f_get_ts = get_mono_ts_us;
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
