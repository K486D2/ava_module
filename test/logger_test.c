#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "../logger/logger.h"

logger_t logger;

static inline void print(FILE *file, char *str, u32 len) {
  printf("%s", str);
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

#define THREAD_COUNT 100

int main() {
  logger_cfg_t logger_cfg = {
      .level         = LOGGER_LEVEL_DEBUG,
      .new_line_sign = '\n',
      .prefix        = "",
      .file          = stdout,
  };
  logger.ops.f_print = print;
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
