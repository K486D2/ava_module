#include <stdlib.h>

#include "module.h"

int main() {
  shm_t     shm     = {0};
  shm_cfg_t shm_cfg = {
      .name   = "shm",
      .access = PAGE_READWRITE,
      .cap    = 1024,
  };

  int ret = shm_init(&shm, shm_cfg);
  if (ret < 0) {
    printf("writer: shm init failed, errcode: %d\n", ret);
    exit(-1);
  }

  printf("shm_addr: 0x%p, buf_addr: 0x%p\n", shm.lo.base, shm.lo.spsc->buf);

  u64 cnt = 0;
  while (true) {
    shm_read(&shm, &cnt, sizeof(cnt));
    printf("read cnt: %u, spsc wp: %llu, spsc rp: %llu, spsc free: %llu\n",
           cnt,
           atomic_load(&shm.lo.spsc->wp),
           atomic_load(&shm.lo.spsc->rp),
           spsc_free(shm.lo.spsc));
  }

  return 0;
}
