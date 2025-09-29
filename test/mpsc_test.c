#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "container/mpsc.h"

#define CAP      4096
#define NPROD    4
#define NMSG     1000
#define MSG_SIZE 32

static mpsc_t          mpsc;
static mpsc_producer_t producers[NPROD];
static char            buffer[CAP];

void *producer_thread(void *arg) {
  size_t           id = (size_t)arg;
  mpsc_producer_t *p  = mpsc_register(&mpsc, id);

  for (int i = 0; i < NMSG; i++) {
    char msg[MSG_SIZE];
    snprintf(msg, sizeof(msg), "P%zu-msg%d", id, i);

    ssize_t off;
    do {
      off = mpsc_acquire(&mpsc, p, MSG_SIZE);
      if (off < 0) {
        usleep(100);
      }
    } while (off < 0);

    memcpy(buffer + off, msg, MSG_SIZE);
    mpsc_produce(&mpsc, p);
  }

  // producer 完成后注销，避免 consumer 卡死
  mpsc_unregister(&mpsc, p);
  return NULL;
}

void *consumer_thread(void *arg) {
  (void)arg;
  size_t       total    = 0;
  const size_t expected = NPROD * NMSG;

  while (total < expected) {
    size_t off, n;
    n = mpsc_consume(&mpsc, &off);
    if (n == 0) {
      usleep(1000);
      continue;
    }

    size_t used = 0;
    while (used + MSG_SIZE <= n) {
      char tmp[MSG_SIZE + 1];
      memcpy(tmp, buffer + off + used, MSG_SIZE);
      tmp[MSG_SIZE] = '\0';
      printf("consumer got: \"%s\"\n", tmp);
      used += MSG_SIZE;
      total++;
    }

    mpsc_release(&mpsc, used);
  }

  printf("Consumer done: got %zu messages\n", total);
  return NULL;
}

int main(void) {
  mpsc_init(&mpsc, CAP, producers, NPROD);

  pthread_t prod[NPROD], cons;
  pthread_create(&cons, NULL, consumer_thread, NULL);
  for (size_t i = 0; i < NPROD; i++) {
    pthread_create(&prod[i], NULL, producer_thread, (void *)i);
  }

  for (size_t i = 0; i < NPROD; i++) {
    pthread_join(prod[i], NULL);
  }
  pthread_join(cons, NULL);

  return 0;
}
