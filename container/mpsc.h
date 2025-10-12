#ifndef MPSC_H
#define MPSC_H

#include <stdint.h>

#include "util/typedef.h"
#include "util/util.h"

#ifdef MCU
#define MPSC_OFF_MASK      (0x0000FFFFU)
#define MPSC_WRAP_LOCK_BIT (0x80000000U)
#define MPSC_OFF_MAX       (UINT32_MAX & ~MPSC_WRAP_LOCK_BIT)
#define MPSC_WRAP_COUNTER  (0x7FFF0000U)
#define MPSC_WRAP_INCR(x)  (((x) + 0x00010000U) & MPSC_WRAP_COUNTER)
#else
#define MPSC_OFF_MASK      (0x00000000FFFFFFFFUL)
#define MPSC_WRAP_LOCK_BIT (0x8000000000000000UL)
#define MPSC_OFF_MAX       (UINT64_MAX & ~MPSC_WRAP_LOCK_BIT)
#define MPSC_WRAP_COUNTER  (0x7FFFFFFF00000000UL)
#define MPSC_WRAP_INCR(x)  (((x) + 0x100000000UL) & MPSC_WRAP_COUNTER)
#endif

typedef struct {
  ATOMIC(usz) reserve_pos; // 当前生产者申请的写区终点
  ATOMIC(bool) flag;       // 是否活跃
} mpsc_p_t;

typedef struct {
  void *buf;            // 环形缓冲区存放实际数据
  usz   cap;            // 环形缓冲区容量
  usz   warp_end;       // wrap 回绕的标记位置
  ATOMIC(usz) wp;       // 全局写指针(生产者共享)
  ATOMIC(usz) rp;       // 全局读指针(消费者独占)
  usz       nproducers; // 生产者数量
  mpsc_p_t *producers;  // 生产者状态数组
} mpsc_t;

static inline void mpsc_init(mpsc_t *mpsc, void *buf, usz cap, mpsc_p_t *producers, usz nproducers);
static inline isz  mpsc_write(mpsc_t *mpsc, mpsc_p_t *p, const void *src, usz nbytes);
static inline usz  mpsc_read(mpsc_t *mpsc, void *dst, usz nbytes);

static inline mpsc_p_t *mpsc_reg(mpsc_t *mpsc, usz id);
static inline void      mpsc_unreg(mpsc_p_t *p);
static inline isz       mpsc_acquire(mpsc_t *mpsc, mpsc_p_t *p, usz nbytes);
static inline void      mpsc_push(mpsc_p_t *p);
static inline usz       mpsc_pop(mpsc_t *mpsc, usz *off);
static inline void      mpsc_release(mpsc_t *mpsc, usz nbytes);

static inline void
mpsc_init(mpsc_t *mpsc, void *buf, usz cap, mpsc_p_t *producers, usz nproducers) {
  mpsc->buf        = buf;
  mpsc->cap        = cap;
  mpsc->warp_end   = MPSC_OFF_MAX;
  mpsc->nproducers = nproducers;
  mpsc->producers  = producers;
}

static inline mpsc_p_t *mpsc_reg(mpsc_t *mpsc, usz id) {
  mpsc_p_t *p = &mpsc->producers[id];
  ATOMIC_STORE_EXPLICIT(&p->reserve_pos, MPSC_OFF_MAX, memory_order_relaxed);
  ATOMIC_STORE_EXPLICIT(&p->flag, true, memory_order_release);
  return p;
}

static inline void mpsc_unreg(mpsc_p_t *p) { p->flag = false; }

static inline usz mpsc_get_wp(mpsc_t *mpsc) {
  u32 cnt = SPINLOCK_BACKOFF_MIN;
  usz wp;

retry:
  wp = ATOMIC_LOAD_EXPLICIT(&mpsc->wp, memory_order_acquire);
  if (wp & MPSC_WRAP_LOCK_BIT) {
    SPINLOCK_BACKOFF(cnt);
    goto retry;
  }
  return wp;
}

static inline usz mpsc_get_reserve_pos(mpsc_p_t *p) {
  u32 cnt = SPINLOCK_BACKOFF_MIN;
  usz reserve_pos;

retry:
  reserve_pos = ATOMIC_LOAD_EXPLICIT(&p->reserve_pos, memory_order_acquire);
  if (reserve_pos & MPSC_WRAP_LOCK_BIT) {
    SPINLOCK_BACKOFF(cnt);
    goto retry;
  }
  return reserve_pos;
}

/**
 * @brief 生产者申请在 MPSC 环形缓冲区中写入 nbytes 的空间
 *
 * @param mpsc
 * @param p
 * @param nbytes
 * @return 写入偏移 (逻辑 offset) 或者 -1 (表示空间不足)
 */
static inline isz mpsc_acquire(mpsc_t *mpsc, mpsc_p_t *p, usz nbytes) {
  usz wp, off, target;

  do {
    // 读取全局写指针
    wp = mpsc_get_wp(mpsc);

    // 提取实际偏移
    off = wp & MPSC_OFF_MASK;

    // 标记正在申请写入
    ATOMIC_STORE_EXPLICIT(&p->reserve_pos, off | MPSC_WRAP_LOCK_BIT, memory_order_relaxed);

    // 尝试申请的终点位置
    target = off + nbytes;
    usz rp = ATOMIC_LOAD_EXPLICIT(&mpsc->rp, memory_order_relaxed);
    if (off < rp && target >= rp) {
      ATOMIC_STORE_EXPLICIT(&p->reserve_pos, MPSC_OFF_MAX, memory_order_release);
      return -1;
    }

    // 如果申请空间超过环尾
    if (target >= mpsc->cap) {
      target = (target > mpsc->cap) ? (MPSC_WRAP_LOCK_BIT | nbytes) : 0;
      if ((target & MPSC_OFF_MASK) >= rp) {
        ATOMIC_STORE_EXPLICIT(&p->reserve_pos, MPSC_OFF_MAX, memory_order_release);
        return -1;
      }
      target |= MPSC_WRAP_INCR(wp & MPSC_WRAP_COUNTER);
    } else
      target |= wp & MPSC_WRAP_COUNTER;
  } while (!atomic_compare_exchange_weak(&mpsc->wp, &wp, target));

  // 清除 wrap lock bit，标记 reserve_pos 申请完成
  ATOMIC_STORE_EXPLICIT(
      &p->reserve_pos, p->reserve_pos & ~MPSC_WRAP_LOCK_BIT, memory_order_relaxed);

  // 如果申请触发 wrap
  if (target & MPSC_WRAP_LOCK_BIT) {
    mpsc->warp_end = off;
    ATOMIC_STORE_EXPLICIT(&mpsc->wp, (target & ~MPSC_WRAP_LOCK_BIT), memory_order_release);
    off = 0;
  }
  return (isz)off;
}

static inline void mpsc_push(mpsc_p_t *p) {
  ATOMIC_STORE_EXPLICIT(&p->reserve_pos, MPSC_OFF_MAX, memory_order_release);
}

static inline usz mpsc_pop(mpsc_t *mpsc, usz *off) {
  usz rp = ATOMIC_LOAD_EXPLICIT(&mpsc->rp, memory_order_relaxed);
  usz wp;

retry:
  wp = mpsc_get_wp(mpsc) & MPSC_OFF_MASK;
  if (rp == wp)
    return 0;

  // 遍历所有 producer 的 reserve_pos
  // 找到最小的 reserve_pos (还没写完的数据)
  // consumer 只能读到这个位置，保证不读到未完成数据

  // 本次 pop 能安全读取的最大逻辑偏移量
  usz ready = MPSC_OFF_MAX;
  for (usz i = 0; i < mpsc->nproducers; i++) {
    mpsc_p_t *p = &mpsc->producers[i];
    if (!ATOMIC_LOAD_EXPLICIT(&p->flag, memory_order_relaxed))
      continue;

    usz reserve_pos = mpsc_get_reserve_pos(p);
    if (reserve_pos >= rp) {
      if (reserve_pos < ready)
        ready = reserve_pos;
    }
  }

  // 处理环形缓冲 wrap
  if (wp < rp) {
    usz warp_end = (mpsc->warp_end == MPSC_OFF_MAX) ? mpsc->cap : mpsc->warp_end;
    if (ready == MPSC_OFF_MAX && rp == warp_end) {
      if (mpsc->warp_end != MPSC_OFF_MAX)
        mpsc->warp_end = MPSC_OFF_MAX;
      rp = 0;
      ATOMIC_STORE_EXPLICIT(&mpsc->rp, rp, memory_order_release);
      goto retry;
    }
    ready = (ready < warp_end) ? ready : warp_end;
  } else
    ready = (ready < wp) ? ready : wp;

  usz write_nbytes = ready - rp;
  *off             = rp;
  return write_nbytes;
}

static inline void mpsc_release(mpsc_t *mpsc, usz nbytes) {
  usz write_nbytes = mpsc->rp + nbytes;
  mpsc->rp         = (write_nbytes == mpsc->cap) ? 0 : write_nbytes;
}

static inline isz mpsc_write(mpsc_t *mpsc, mpsc_p_t *p, const void *src, usz nbytes) {
  isz off = mpsc_acquire(mpsc, p, nbytes);
  if (off < 0)
    return -1;

  if ((usz)off + nbytes <= mpsc->cap)
    memcpy((u8 *)mpsc->buf + (usz)off, src, nbytes);
  else {
    usz first = mpsc->cap - (usz)off;
    memcpy((u8 *)mpsc->buf + (usz)off, src, first);
    memcpy((u8 *)mpsc->buf, (u8 *)src + first, nbytes - first);
  }

  mpsc_push(p);
  return off;
}

static inline usz mpsc_read(mpsc_t *mpsc, void *dst, usz nbytes) {
  usz off, avail_nbytes = mpsc_pop(mpsc, &off);
  if (avail_nbytes == 0)
    return 0;

  usz read_nbytes = (avail_nbytes < nbytes) ? 0 : nbytes;

  if (off + read_nbytes <= mpsc->cap)
    memcpy(dst, (u8 *)mpsc->buf + off, read_nbytes);
  else {
    usz first = mpsc->cap - off;
    memcpy(dst, (u8 *)mpsc->buf + off, first);
    memcpy((u8 *)dst + first, (u8 *)mpsc->buf, read_nbytes - first);
  }

  mpsc_release(mpsc, read_nbytes);
  return read_nbytes;
}

#endif // !MPSC_H
