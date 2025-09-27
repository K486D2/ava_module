#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "module.h"

#define BUF_SIZE     1024
#define TEST_COUNT   50000
#define PRODUCER_NUM 10
#define CONSUMER_NUM 10

typedef struct {
  u64 seq;
} packet_t;

static fifo_t      fifo;
static fifo_node_t buf[BUF_SIZE];

void *producer(void *arg) {
  ARG_UNUSED(arg);

  u64 base = (u64)(uintptr_t)arg * TEST_COUNT;
  for (u64 i = 0; i < TEST_COUNT; i++) {
    packet_t pkt = {.seq = base + i};
    while (fifo_in(&fifo, &pkt, sizeof(pkt)) != sizeof(pkt))
      ;
  }
  return NULL;
}

void *consumer(void *arg) {
  ARG_UNUSED(arg);

  static atomic_size_t total_consumed = 0;

  static unsigned char  *bitmap      = NULL;
  static pthread_mutex_t bitmap_lock = PTHREAD_MUTEX_INITIALIZER;

  size_t total = PRODUCER_NUM * TEST_COUNT;

  if (bitmap == NULL) {
    pthread_mutex_lock(&bitmap_lock);
    if (bitmap == NULL) {
      bitmap = calloc(total, 1);
      if (!bitmap) {
        perror("calloc");
        exit(1);
      }
    }
    pthread_mutex_unlock(&bitmap_lock);
  }

  packet_t pkt;
  while (atomic_load_explicit(&total_consumed, memory_order_relaxed) < total) {
    if (fifo_out(&fifo, &pkt, sizeof(pkt)) == sizeof(pkt)) {
      pthread_mutex_lock(&bitmap_lock);
      if (bitmap[pkt.seq]) {
        printf("ERROR: duplicate seq=%llu\n", (u64)pkt.seq);
        exit(1);
      }
      bitmap[pkt.seq] = 1;
      pthread_mutex_unlock(&bitmap_lock);

      atomic_fetch_add_explicit(&total_consumed, 1, memory_order_relaxed);
    }
  }
  return NULL;
}

int main() {
  if (fifo_init(&fifo, buf, BUF_SIZE, FIFO_MODE_MPMC, FIFO_POLICY_REJECT) != 0) {
    printf("fifo_init failed\n");
    return -1;
  }

  pthread_t producers[PRODUCER_NUM];
  pthread_t consumers[CONSUMER_NUM];

  // 启动生产者
  for (int i = 0; i < PRODUCER_NUM; i++)
    pthread_create(&producers[i], NULL, producer, (void *)(uintptr_t)i);

  // 启动消费者
  for (int i = 0; i < CONSUMER_NUM; i++)
    pthread_create(&consumers[i], NULL, consumer, NULL);

  // 等待生产者结束
  for (int i = 0; i < PRODUCER_NUM; i++)
    pthread_join(producers[i], NULL);

  // 等待消费者结束
  for (int i = 0; i < CONSUMER_NUM; i++)
    pthread_join(consumers[i], NULL);

  printf("MPMC FIFO test finished successfully. All %llu packets verified.\n",
         (u64)(PRODUCER_NUM * TEST_COUNT));
  return 0;
}
