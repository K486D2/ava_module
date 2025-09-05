#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include "../container/fifo.h"
#include "../util/util.h"

#define SHM_SIZE      1024

#define SHM_NAME_SIZE 32
typedef struct {
  char name[SHM_NAME_SIZE];
  u32  access;
} shm_cfg_t;

typedef struct {
#ifdef __linux__
  int fd;
#elif defined(_WIN32)
  HANDLE fd;
#endif
  void *base;
  bool  is_creator;
} shm_lo_t;

typedef struct {
  shm_cfg_t cfg;
  shm_lo_t  lo;
} shm_t;

#define DECL_SHM_PTRS(shm)                                                                         \
  shm_t     *p   = (shm);                                                                          \
  shm_cfg_t *cfg = &p->cfg;                                                                        \
  shm_lo_t  *lo  = &p->lo;                                                                         \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_SHM_PTRS_PREFIX(shm, prefix)                                                          \
  shm_t     *prefix##_p   = (shm);                                                                 \
  shm_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                      \
  shm_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                       \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_lo);

static int shm_init(shm_t *shm, shm_cfg_t shm_cfg) {
  DECL_SHM_PTRS(shm);

  *cfg = shm_cfg;

#ifdef __linux__
  lo->fd = shm_open(cfg->name, O_RDWR, 0666);
  if (lo->fd == -1) {
    lo->fd = shm_open(cfg->name, O_CREAT | O_RDWR, 0666);
    if (lo->fd == -1)
      return -MEACCES;
    lo->is_creator = true;

    if (ftruncate(lo->fd, SHM_SIZE) == -1) {
      close(lo->fd);
      return -MEACCES;
    }
  } else
    lo->is_creator = false;

  lo->base = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, lo->fd, 0);
  if (lo->base == MAP_FAILED) {
    close(lo->fd);
    if (lo->is_creator)
      shm_unlink(cfg->name);
    return -MEACCES;
  }
#elif defined(_WIN32)
  lo->fd = OpenFileMapping(FILE_MAP_ALL_ACCESS, // 读写权限
                           FALSE,               // 不继承句柄
                           cfg->name);          // 共享内存名称
  if (lo->fd == NULL) {
    lo->fd = CreateFileMapping(INVALID_HANDLE_VALUE, // 使用物理内存
                               NULL,                 // 默认安全属性
                               cfg->access,          // 可读可写
                               0,                    // 内存大小高32位
                               SHM_SIZE,             // 内存大小低32位
                               cfg->name);           // 命名对象
    if (lo->fd == NULL)
      return -MECREATE;
    lo->is_creator = true;
  } else
    lo->is_creator = false;

  // 映射到进程地址空间
  lo->base = MapViewOfFile(lo->fd,              // 文件映射句柄
                           FILE_MAP_ALL_ACCESS, // 读写权限
                           0,
                           0,         // 偏移量
                           SHM_SIZE); // 映射大小
  if (lo->base == NULL) {
    UnmapViewOfFile(lo->base);
    CloseHandle(lo->fd);
    return -MEACCES;
  }
#endif

  return 0;
}

#ifdef __cplusplus
}
#endif

#endif // !SHARED_MEM_H
