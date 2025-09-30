#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "container/mpsc.h"

#define CAP      4096
#define NPROD    40
#define NMSG     100000000
#define MSG_SIZE 40

static mpsc_t   mpsc;
static mpsc_p_t producers[NPROD];
static char     buffer[CAP];

void *producer_thread(void *arg) {
  size_t    id = (size_t)arg;
  mpsc_p_t *p  = mpsc_reg(&mpsc, id);

  for (int i = 0; i < NMSG; i++) {
    char msg[MSG_SIZE];
    int  n = snprintf(msg, sizeof(msg), "P%3zu-msg-abcdefghijklmnopqrstuvwxyz-%d", id, i);

    mpsc_write(&mpsc, p, msg, MSG_SIZE);
  }

  // producer 完成后注销，避免 consumer 卡死
  mpsc_unreg(p);
  return NULL;
}

void *consumer_thread(void *arg) {
  (void)arg;
  size_t       total    = 0;
  const size_t expected = NPROD * NMSG;

  while (total < expected) {
    u8  tmp[MSG_SIZE];
    usz n = mpsc_read(&mpsc, tmp, MSG_SIZE);
    if (n > 0)
      printf("Consumer got: %.*s\n", (int)n, tmp);
  }

  printf("Consumer done: got %zu messages\n", total);
  return NULL;
}

int main(void) {
  mpsc_init(&mpsc, buffer, CAP, producers, NPROD);

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
