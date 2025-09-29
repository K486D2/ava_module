#ifndef MPSC_H
#define MPSC_H

#include <stdint.h>
#include <unistd.h>

#include "util/typedef.h"
#include "util/util.h"

#define MPSC_OFF_MASK      (0x00000000FFFFFFFFUL)
#define MPSC_WRAP_LOCK_BIT (0x8000000000000000UL)
#define MPSC_OFF_MAX       (UINT64_MAX & ~MPSC_WRAP_LOCK_BIT)

#define MPSC_WRAP_COUNTER  (0x7FFFFFFF00000000UL)
#define MPSC_WRAP_INCR(x)  (((x) + 0x100000000UL) & MPSC_WRAP_COUNTER)

typedef struct {
  atomic_t(size_t) seen_off;
  atomic_t(bool) registered;
} mpsc_producer_t;

typedef struct {
  atomic_t(size_t) next;
  size_t end;
  atomic_t(size_t) written;
  size_t           nproducers;
  size_t           cap;
  mpsc_producer_t *producers;
} mpsc_t;

static inline void
mpsc_init(mpsc_t *mpsc, size_t cap, mpsc_producer_t *producers, size_t nproducers) {
  mpsc->cap        = cap;
  mpsc->end        = MPSC_OFF_MAX;
  mpsc->nproducers = nproducers;
  mpsc->producers  = producers;
}

static inline mpsc_producer_t *mpsc_register(mpsc_t *mpsc, size_t i) {
  mpsc_producer_t *w = &mpsc->producers[i];
  w->seen_off        = MPSC_OFF_MAX;
  atomic_store_explicit(&w->registered, true, memory_order_release);
  return w;
}

static inline void mpsc_unregister(mpsc_t *mpsc, mpsc_producer_t *w) {
  ARG_UNUSED(mpsc);
  w->registered = false;
}

static inline u64 mpsc_stable_nextoff(mpsc_t *mpsc) {
  unsigned count = SPINLOCK_BACKOFF_MIN;
  size_t   next;
retry:
  next = atomic_load_explicit(&mpsc->next, memory_order_acquire);
  if (next & MPSC_WRAP_LOCK_BIT) {
    SPINLOCK_BACKOFF(count);
    goto retry;
  }
  return next;
}

static inline u64 mpsc_stable_seenoff(mpsc_producer_t *w) {
  unsigned count = SPINLOCK_BACKOFF_MIN;
  size_t   seen_off;
retry:
  seen_off = atomic_load_explicit(&w->seen_off, memory_order_acquire);
  if (seen_off & MPSC_WRAP_LOCK_BIT) {
    SPINLOCK_BACKOFF(count);
    goto retry;
  }
  return seen_off;
}

static inline ssize_t mpsc_acquire(mpsc_t *mpsc, mpsc_producer_t *w, size_t len) {
  size_t seen, next, target;

  do {
    size_t written;
    seen = mpsc_stable_nextoff(mpsc);
    next = seen & MPSC_OFF_MASK;

    atomic_store_explicit(&w->seen_off, next | MPSC_WRAP_LOCK_BIT, memory_order_relaxed);

    target  = next + len;
    written = mpsc->written;
    if (next < written && target >= written) {
      atomic_store_explicit(&w->seen_off, MPSC_OFF_MAX, memory_order_release);
      return -1;
    }

    if (target >= mpsc->cap) {
      const bool exceed = target > mpsc->cap;
      target            = exceed ? (MPSC_WRAP_LOCK_BIT | len) : 0;
      if ((target & MPSC_OFF_MASK) >= written) {
        atomic_store_explicit(&w->seen_off, MPSC_OFF_MAX, memory_order_release);
        return -1;
      }
      target |= MPSC_WRAP_INCR(seen & MPSC_WRAP_COUNTER);
    } else {
      target |= seen & MPSC_WRAP_COUNTER;
    }
  } while (!atomic_compare_exchange_weak(&mpsc->next, &seen, target));

  atomic_store_explicit(&w->seen_off, w->seen_off & ~MPSC_WRAP_LOCK_BIT, memory_order_relaxed);

  if (target & MPSC_WRAP_LOCK_BIT) {
    mpsc->end = next;
    atomic_store_explicit(&mpsc->next, (target & ~MPSC_WRAP_LOCK_BIT), memory_order_release);
    next = 0;
  }
  return (ssize_t)next;
}

static inline void mpsc_produce(mpsc_t *mpsc, mpsc_producer_t *w) {
  ARG_UNUSED(mpsc);
  atomic_store_explicit(&w->seen_off, MPSC_OFF_MAX, memory_order_release);
}

static inline size_t mpsc_consume(mpsc_t *mpsc, size_t *offset) {
  size_t written = mpsc->written, next, ready;
  size_t towrite;

retry:
  next = mpsc_stable_nextoff(mpsc) & MPSC_OFF_MASK;
  if (written == next)
    return 0;

  ready = MPSC_OFF_MAX;
  for (unsigned i = 0; i < mpsc->nproducers; i++) {
    mpsc_producer_t *w = &mpsc->producers[i];
    if (!atomic_load_explicit(&w->registered, memory_order_relaxed))
      continue;
    size_t seen_off = mpsc_stable_seenoff(w);
    if (seen_off >= written) {
      if (seen_off < ready)
        ready = seen_off;
    }
  }

  if (next < written) {
    const size_t end = (mpsc->end == MPSC_OFF_MAX) ? mpsc->cap : mpsc->end;
    if (ready == MPSC_OFF_MAX && written == end) {
      if (mpsc->end != MPSC_OFF_MAX)
        mpsc->end = MPSC_OFF_MAX;
      written = 0;
      atomic_store_explicit(&mpsc->written, written, memory_order_release);
      goto retry;
    }
    ready = (ready < end) ? ready : end;
  } else {
    ready = (ready < next) ? ready : next;
  }

  towrite = ready - written;
  *offset = written;
  return towrite;
}

static inline void mpsc_release(mpsc_t *mpsc, size_t nbytes) {
  const size_t nwritten = mpsc->written + nbytes;
  mpsc->written         = (nwritten == mpsc->cap) ? 0 : nwritten;
}

#endif // !MPSC_H
