#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "../logger/logger.h"

logger_t logger;

u8 LOGGER_FIFO_BUF[4 * 1024];
u8 LOGGER_LINE_BUF[128];

static inline void logger_stdout(void *fp, const u8 *data, size_t size) {
  fwrite(data, 1, size, fp);
}

void *flush_thread_func(void *arg) {
  logger_t *logger = (logger_t *)arg;
  while (1) {
    logger_flush(logger);

#ifdef _WIN32
    Sleep(1);
#else
    usleep(1000);
#endif
  }
  return NULL;
}

void *write_thread_func(void *arg) {
  u64 *p_cnt = (u64 *)arg;
  u64  cnt   = *p_cnt;

  while (true) {
#ifdef _WIN32
    logger_debug(&logger, "Thread %lu: cnt: %llu\n", (unsigned long)GetCurrentThreadId(), cnt++);
#else
    logger_debug(&logger, "Thread %lu: cnt: %llu\n", (unsigned long)pthread_self(), cnt++);
#endif

#ifdef _WIN32
    Sleep(1);
#else
    usleep(1000);
#endif
  }
  return NULL;
}

#define THREAD_COUNT 10

int main() {
  logger_cfg_t logger_cfg = {
      .level         = LOGGER_LEVEL_DEBUG,
      .new_line_sign = '\n',
      .fp            = stdout,
      .fifo_buf      = LOGGER_FIFO_BUF,
      .fifo_buf_size = sizeof(LOGGER_FIFO_BUF),
      .line_buf      = LOGGER_LINE_BUF,
      .line_buf_size = sizeof(LOGGER_LINE_BUF),
  };
  logger.ops.f_flush = logger_stdout;
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
