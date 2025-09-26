#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../container/fifo.h"

#define BUFFER_SIZE           4096
#define TEST_DATA_SIZE        100
#define NUM_WRITERS           10
#define NUM_READERS           2
#define OPERATIONS_PER_THREAD 1000

typedef struct {
  int  thread_id;
  long sequence;
  char data[TEST_DATA_SIZE - sizeof(int) - sizeof(long)];
} test_data_t;

// 全局统计
atomic_t(long) total_writes = 0;
atomic_t(long) total_reads  = 0;
atomic_t(long) write_errors = 0;
atomic_t(long) read_errors  = 0;

// 测试配置
typedef struct {
  fifo_mode_e   mode;
  fifo_policy_e policy;
  int           num_writers;
  int           num_readers;
  int           operations_per_thread;
} test_config_t;

void *writer_thread(void *arg) {
  fifo_t     *fifo = (fifo_t *)arg;
  test_data_t data;
  int         thread_id = __sync_fetch_and_add(&total_writes, 0); // 简单的ID分配

  for (int i = 0; i < OPERATIONS_PER_THREAD; i++) {
    data.thread_id = thread_id;
    data.sequence  = i;
    snprintf(data.data, sizeof(data.data), "Writer%d_Seq%d", thread_id, i);

    size_t written = fifo_in(fifo, &data, sizeof(data));
    if (written != sizeof(data)) {
      __sync_fetch_and_add(&write_errors, 1);
      // 写入失败，短暂休眠后重试
      usleep(1000);
    } else {
      __sync_fetch_and_add(&total_writes, 1);
    }

    // 随机延迟，模拟真实场景
    usleep(rand() % 100);
  }
  return NULL;
}

// 在reader_thread中添加更详细的调试信息
void *reader_thread(void *arg) {
  fifo_t     *fifo = (fifo_t *)arg;
  test_data_t data;
  long        last_sequence[NUM_WRITERS] = {0};
  int         error_count                = 0;
  long        total_read                 = 0;

  while (total_read < (NUM_WRITERS * OPERATIONS_PER_THREAD)) {
    size_t read = fifo_out(fifo, &data, sizeof(data));
    if (read == sizeof(data)) {
      total_read++;
      __sync_fetch_and_add(&total_reads, 1);

      // 验证数据完整性
      if (data.thread_id < 0 || data.thread_id >= NUM_WRITERS) {
        printf("ERROR: Invalid thread_id %d at read %ld\n", data.thread_id, total_read);
        error_count++;
        continue;
      }

      // 检查序列号连续性
      if (last_sequence[data.thread_id] != 0 &&
          data.sequence != last_sequence[data.thread_id] + 1) {
        printf("ERROR: Thread %d sequence jump: %ld -> %ld (read %ld, avail=%zu)\n",
               data.thread_id,
               last_sequence[data.thread_id],
               data.sequence,
               total_read,
               fifo_get_avail(fifo));
        error_count++;
      }
      last_sequence[data.thread_id] = data.sequence;

    } else if (read == 0) {
      // 没有数据时显示状态信息
      printf("No data available, fifo avail=%zu, free=%zu\n",
             fifo_get_avail(fifo),
             fifo_get_free(fifo));
      usleep(10000); // 延长休眠时间以便观察
    }

    if (total_read % 100 == 0) {
      printf(
          "Progress: %ld/%ld reads completed\n", total_read, NUM_WRITERS * OPERATIONS_PER_THREAD);
    }
  }

  __sync_fetch_and_add(&read_errors, error_count);
  return NULL;
}

void run_test(const test_config_t *config) {
  printf("\n=== Starting Test: Mode=%d, Policy=%d, Writers=%d, Readers=%d ===\n",
         config->mode,
         config->policy,
         config->num_writers,
         config->num_readers);

  // 重置统计
  atomic_store(&total_writes, 0);
  atomic_store(&total_reads, 0);
  atomic_store(&write_errors, 0);
  atomic_store(&read_errors, 0);

  // 初始化FIFO
  fifo_t fifo;
  void  *buffer = malloc(BUFFER_SIZE);
  if (fifo_init(&fifo, buffer, BUFFER_SIZE, config->mode, config->policy) != 0) {
    printf("FIFO initialization failed!\n");
    free(buffer);
    return;
  }

  pthread_t writers[NUM_WRITERS];
  pthread_t readers[NUM_READERS];

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  // 创建写线程
  for (int i = 0; i < config->num_writers; i++) {
    if (pthread_create(&writers[i], NULL, writer_thread, &fifo) != 0) {
      printf("Failed to create writer thread %d\n", i);
    }
  }

  // 创建读线程
  for (int i = 0; i < config->num_readers; i++) {
    if (pthread_create(&readers[i], NULL, reader_thread, &fifo) != 0) {
      printf("Failed to create reader thread %d\n", i);
    }
  }

  // 等待所有写线程完成
  for (int i = 0; i < config->num_writers; i++) {
    pthread_join(writers[i], NULL);
  }

  // 等待所有读线程完成
  for (int i = 0; i < config->num_readers; i++) {
    pthread_join(readers[i], NULL);
  }

  clock_gettime(CLOCK_MONOTONIC, &end);

  // 计算运行时间
  double duration = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

  // 输出结果
  printf("Test completed in %.3f seconds\n", duration);
  printf("Total writes: %ld\n", atomic_load(&total_writes));
  printf("Total reads: %ld\n", atomic_load(&total_reads));
  printf("Write errors: %ld\n", atomic_load(&write_errors));
  printf("Read errors: %ld\n", atomic_load(&read_errors));
  printf("FIFO empty: %s\n", fifo_is_empty(&fifo) ? "Yes" : "No");
  printf("FIFO available: %zu\n", fifo_get_avail(&fifo));
  printf("Throughput: %.2f operations/sec\n",
         (atomic_load(&total_writes) + atomic_load(&total_reads)) / duration);

  // 清理
  free(buffer);
}

void stress_test() {
  printf("\n=== Stress Test: High Contention ===\n");

  fifo_t fifo;
  void  *buffer = malloc(1024); // 小缓冲区增加竞争
  fifo_init(&fifo, buffer, 1024, FIFO_MODE_MPMC, FIFO_POLICY_OVERWRITE);

  atomic_t(int) stop = 0;

  // 创建多个读写线程进行压力测试
  pthread_t threads[20];
  for (int i = 0; i < 20; i++) {
    if (i % 2 == 0) {
      // 写线程
      pthread_create(&threads[i], NULL, writer_thread, &fifo);
    } else {
      // 读线程
      pthread_create(&threads[i], NULL, reader_thread, &fifo);
    }
  }

  // 运行10秒
  sleep(10);

  // 停止测试
  atomic_store(&stop, 1);

  for (int i = 0; i < 20; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("Stress test completed\n");
  free(buffer);
}

int main() {
  srand(time(NULL));

  // 测试不同模式
  test_config_t tests[] = {
      {FIFO_MODE_SPSC, FIFO_POLICY_REJECT, 1, 1, OPERATIONS_PER_THREAD},
      {FIFO_MODE_SPMC, FIFO_POLICY_REJECT, 1, 2, OPERATIONS_PER_THREAD},
      {FIFO_MODE_MPSC, FIFO_POLICY_REJECT, 4, 1, OPERATIONS_PER_THREAD},
      {FIFO_MODE_MPMC, FIFO_POLICY_REJECT, 4, 2, OPERATIONS_PER_THREAD},
      {FIFO_MODE_MPMC, FIFO_POLICY_OVERWRITE, 8, 2, OPERATIONS_PER_THREAD},
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    run_test(&tests[i]);
  }

  // 压力测试
  stress_test();

  printf("\nAll tests completed!\n");
  return 0;
}
