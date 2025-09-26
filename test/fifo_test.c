#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "module.h"

#define BUF_SIZE     1024    // 必须是 2 的幂
#define TEST_COUNT   5000000 // 每个生产者生产数量
#define PRODUCER_NUM 1
#define CONSUMER_NUM 1

typedef struct {
  u64 seq;
} packet_t;

static fifo_t        fifo;
static unsigned char buffer[BUF_SIZE];

// ------------------------- 生产者线程 -------------------------
void *producer(void *arg) {
  u64 base = (u64)(uintptr_t)arg * TEST_COUNT;
  for (u64 i = 0; i < TEST_COUNT; i++) {
    packet_t pkt = {.seq = base + i};
    while (fifo_in(&fifo, &pkt, sizeof(pkt)) != sizeof(pkt))
      ;
  }
  return NULL;
}

// ------------------------- 消费者线程 -------------------------
void *consumer(void *arg) {
  ARG_UNUSED(arg);
  static atomic_ullong last_seq = 0;
  packet_t             pkt;
  for (;;) {
    if (fifo_out(&fifo, &pkt, sizeof(pkt)) == sizeof(pkt)) {
      // 检查 seq 唯一性和递增性
      u64 expected = atomic_fetch_add_explicit(&last_seq, 1, memory_order_relaxed);
      if (pkt.seq != expected) {
        printf("ERROR: expected=%llu, got=%llu\n",
               (unsigned long long)expected,
               (unsigned long long)pkt.seq);
        exit(1);
      }
    } else {
      // 检查是否所有生产者已完成
      if (atomic_load_explicit(&last_seq, memory_order_relaxed) >= PRODUCER_NUM * TEST_COUNT)
        break;
    }
  }
  return NULL;
}

int main() {
  if (fifo_init(&fifo, buffer, BUF_SIZE, FIFO_MODE_MPSC, FIFO_POLICY_REJECT) != 0) {
    printf("fifo_init failed\n");
    return -1;
  }

  pthread_t producers[PRODUCER_NUM];
  pthread_t consumers[CONSUMER_NUM];

  // 启动生产者
  for (int i = 0; i < PRODUCER_NUM; i++) {
    pthread_create(&producers[i], NULL, producer, (void *)(uintptr_t)i);
  }

  // 启动消费者
  for (int i = 0; i < CONSUMER_NUM; i++) {
    pthread_create(&consumers[i], NULL, consumer, NULL);
  }

  // 等待生产者结束
  for (int i = 0; i < PRODUCER_NUM; i++) {
    pthread_join(producers[i], NULL);
  }

  // 等待消费者结束
  for (int i = 0; i < CONSUMER_NUM; i++) {
    pthread_join(consumers[i], NULL);
  }

  printf("MPMC FIFO test finished successfully. All %llu packets verified.\n",
         (unsigned long long)(PRODUCER_NUM * TEST_COUNT));
  return 0;
}
