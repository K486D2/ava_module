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
  fifo_init_buf(fifo, SHM_SIZE >> 1, FIFO_MODE_SPSC, FIFO_POLICY_REJECT);

  printf("shm_addr: 0x%p, buf_addr: 0x%p\n", shm.lo.base, buf);

  u32 cnt = 0;
  while (true) {
    cnt++;
    fifo_in_buf(fifo, buf, &cnt, sizeof(u32));
    printf("write cnt: %u, fifo in: %zu, fifo out: %zu, fifo len: %zu\n",
           cnt,
           atomic_load(&fifo->in),
           atomic_load(&fifo->out),
           fifo_avail(fifo));
  }

  return 0;
}
