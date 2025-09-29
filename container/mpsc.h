#ifndef MPSC_H
#define MPSC_H

#include "util/typedef.h"

#define MPSC_OFF_MASK      (0x00000000FFFFFFFFUL)
#define MPSC_WRAP_LOCK_BIT (0x8000000000000000UL)
#define MPSC_OFF_MAX       (UINT64_MAX & ~MPSC_WRAP_LOCK_BIT)

#define MPSC_WRAP_COUNTER  (0x7FFFFFFF00000000UL)
#define MPSC_WRAP_INCR(x)  (((x) + 0x100000000UL) & MPSC_WRAP_COUNTER)

typedef struct {
  size_t seen_off;
  size_t registered;
} mpsc_producer_t;

typedef struct {
  size_t           cap;
  size_t           next;
  size_t           end;
  size_t           written;
  size_t           nproducers;
  mpsc_producer_t *producers;
} mpsc_t;

static inline void
mpsc_init(mpsc_t *mpsc, size_t cap, mpsc_producer_t *producers, size_t nproducers) {
  mpsc->cap        = cap;
  mpsc->end        = MPSC_OFF_MAX;
  mpsc->nproducers = nproducers;
  mpsc->producers  = producers;
}

void mpsc_get_sizes(size_t nworkers, size_t *ringbuf_size, size_t *ringbuf_worker_size) {
  if (ringbuf_size)
    // *ringbuf_size = offsetof(mpsc_t, workers[nworkers]);
    if (ringbuf_worker_size)
      *ringbuf_worker_size = sizeof(mpsc_producer_t);
}

mpsc_producer_t *mpsc_register(mpsc_t *rbuf, size_t i) {
  mpsc_producer_t *w = &rbuf->producers[i];
  w->seen_off        = MPSC_OFF_MAX;
  atomic_store_explicit(&w->registered, true, memory_order_release);
  return w;
}

void mpsc_unregister(mpsc_t *rbuf, mpsc_producer_t *w) {
  w->registered = false;
}

#endif // !MPSC_H
