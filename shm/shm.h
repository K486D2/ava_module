#ifndef SHM_H
#define SHM_H

#ifdef __linux__
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include "ds/spsc.h"
#include "util/util.h"

typedef enum {
#ifdef __linux__
        SHM_READONLY  = PROT_READ,
        SHM_WRITEONLY = PROT_WRITE,
        SHM_READWRITE = SHM_READONLY | SHM_WRITEONLY,
#elif defined(_WIN32)
        SHM_READONLY  = PAGE_READONLY,
        SHM_READWRITE = PAGE_READWRITE,
#endif
} shm_access_e;

typedef struct {
        const char  *name;
        shm_access_e access;
        usz          cap;
} shm_cfg_t;

typedef struct {
#ifdef __linux__
        int fd;
#elif defined(_WIN32)
        HANDLE fd;
#endif
        void   *base;
        bool    is_creator;
        spsc_t *spsc;
} shm_lo_t;

typedef struct {
        shm_cfg_t cfg;
        shm_lo_t  lo;
} shm_t;

static inline int shm_init(shm_t *shm, shm_cfg_t shm_cfg) {
        DECL_PTRS(shm, cfg, lo);

        *cfg = shm_cfg;

#ifdef __linux__
        lo->fd = shm_open(cfg->name, O_RDWR, 0666);
        if (lo->fd == -1) {
                lo->fd = shm_open(cfg->name, O_CREAT | O_RDWR, 0666);
                if (lo->fd == -1)
                        return -MEACCES;
                lo->is_creator = true;

                if (ftruncate(lo->fd, cfg->cap) == -1) {
                        close(lo->fd);
                        return -MEACCES;
                }
        } else
                lo->is_creator = false;

        lo->base = mmap(NULL, cfg->cap, cfg->access, MAP_SHARED, lo->fd, 0);
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
                                           cfg->cap,             // 内存大小低32位
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
                                 cfg->cap); // 映射大小
        if (lo->base == NULL) {
                UnmapViewOfFile(lo->base);
                CloseHandle(lo->fd);
                return -MEACCES;
        }
#endif

        lo->spsc = (spsc_t *)lo->base;
        if (lo->is_creator)
                spsc_init_buf(lo->spsc, cfg->cap >> 1, SPSC_POLICY_REJECT);

        return 0;
}

static inline void shm_read(shm_t *shm, void *dst, usz nbytes) {
        DECL_PTRS(shm, lo);

        spsc_read_buf(lo->spsc, (u8 *)lo->base + sizeof(*lo->spsc), dst, nbytes);
}

static inline void shm_write(shm_t *shm, void *src, usz nbytes) {
        DECL_PTRS(shm, lo);

        spsc_write_buf(lo->spsc, (u8 *)lo->base + sizeof(*lo->spsc), src, nbytes);
}

#endif // !SHM_H
