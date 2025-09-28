#include <stdlib.h>

#include "module.h"

int main() {
  shm_t     shm     = {0};
  shm_cfg_t shm_cfg = {
      .name = "shm",
      // .access = PAGE_READWRITE,
  };

  int ret = shm_init(&shm, shm_cfg);
  if (ret < 0) {
    printf("writer: shm init failed, errcode: %d\n", ret);
    exit(-1);
  }

  fifo_t *fifo = (fifo_t *)shm.lo.base;
  u8     *buf  = shm.lo.base + sizeof(*fifo);

  printf("shm_addr: 0x%p, buf_addr: 0x%p\n", shm.lo.base, buf);

  u32 cnt = 0;
  while (true) {
    fifo_read_buf(fifo, buf, &cnt, sizeof(u32));
    printf("read cnt: %u, fifo wp: %zu, fifo rp: %zu, fifo free: %zu\n",
           cnt,
           atomic_load(&fifo->wp),
           atomic_load(&fifo->rp),
           fifo_free(fifo));
  }

  return 0;
}
