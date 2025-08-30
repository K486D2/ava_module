#include "../shared_mem/shared_mem.h"

#include <stdlib.h>

int main() {
  shm_t     shm     = {0};
  shm_cfg_t shm_cfg = {
      .name   = "shm",
      // .access = PAGE_READWRITE,
  };

  if (shm_init(&shm, shm_cfg) < 0) {
    printf("reader: shm init failed!\n");
    exit(-1);
  }

  fifo_t *fifo = (fifo_t *)shm.lo.base;
  u8     *buf  = shm.lo.base + sizeof(*fifo);

  printf("shm_addr: %p, buf_addr: %p\n", shm.lo.base, buf);

  u32 cnt = 0;
  while (true) {
    __fifo_buf_out(fifo, buf, &cnt, sizeof(u32));
    printf("read cnt: %u\n", cnt);
  }

  return 0;
}
