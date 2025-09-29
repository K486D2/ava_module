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

  spsc_t *spsc = (spsc_t *)shm.lo.base;
  u8     *buf  = shm.lo.base + sizeof(*spsc);

  printf("shm_addr: 0x%p, buf_addr: 0x%p\n", shm.lo.base, buf);

  u32 cnt = 0;
  while (true) {
    spsc_pop_buf(spsc, buf, &cnt, sizeof(u32));
    printf("read cnt: %u, spsc wp: %zu, spsc rp: %zu, spsc free: %zu\n",
           cnt,
           atomic_load(&spsc->wp),
           atomic_load(&spsc->rp),
           spsc_free(spsc));
  }

  return 0;
}
