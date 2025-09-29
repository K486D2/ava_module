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
  size_t id;
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
  mpsc->next       = 0;
  mpsc->end        = MPSC_OFF_MAX;
  mpsc->written    = 0;
  mpsc->nproducers = nproducers;
  mpsc->producers  = producers;
  for (size_t i = 0; i < nproducers; i++) {
    producers[i].seen_off = 0;
    producers[i].id       = i;
  }
}

#endif // !MPSC_H
