#include "pthread.h"

#include "../shared_mem/shared_mem.h"

fifo_t fifo;

pthread_t writer_t, reader_t;

void *shm_writer(void *) {
  shm_t     shm     = {0};
  shm_cfg_t shm_cfg = {
      .name   = "Local\\shm",
      .access = PAGE_READWRITE,
  };

  if (shm_init(&shm, shm_cfg) < 0) {
    printf("writer: shm init failed!\n");
    exit(-1);
  }
  fifo_init(&fifo, shm.lo.buf, SHM_BUF_SIZE);

  u32 cnt = 0;
  while (true) {
    cnt++;
    __fifo_in(&fifo, &cnt, sizeof(u32));
    printf("write cnt: %u\n", cnt);
  }

  return NULL;
}

void *shm_reader(void *) {
  shm_t     shm     = {0};
  shm_cfg_t shm_cfg = {
      .name   = "Local\\shm",
      .access = PAGE_READONLY,
  };

  if (shm_init(&shm, shm_cfg) < 0) {
    printf("reader: shm init failed!\n");
    exit(-1);
  }
  fifo_init(&fifo, shm.lo.buf, SHM_BUF_SIZE);

  u32 cnt = 0;
  while (true) {
    __fifo_out(&fifo, &cnt, sizeof(u32));
    printf("read cnt: %u\n", cnt);
  }

  return NULL;
}

static int init(void) {
  pthread_create(&writer_t, NULL, shm_writer, NULL);
  pthread_create(&reader_t, NULL, shm_reader, NULL);

  pthread_join(writer_t, NULL);
  pthread_join(reader_t, NULL);
}

int main() {
  init();
  while (true)
    ;
  return 0;
}
