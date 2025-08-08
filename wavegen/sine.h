#ifndef SINE_H
#define SINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../util/def.h"

typedef struct {
  fp32 fs;
  fp32 wave_freq;
  fp32 amp;
  fp32 phase;
  fp32 offset;
} sine_cfg_t;

typedef struct {
  fp32 val;
} sine_out_t;

typedef struct {
  fp32 phase_incr;
} sine_lo_t;

typedef struct {
  sine_cfg_t cfg;
  sine_out_t out;
  sine_lo_t  lo;
} sine_t;

#define DECL_SINE_PTRS(sine)                                                                       \
  sine_t     *p   = (sine);                                                                        \
  sine_cfg_t *cfg = &p->cfg;                                                                       \
  sine_out_t *out = &p->out;                                                                       \
  sine_lo_t  *lo  = &p->lo;                                                                        \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_SINE_PTRS_PREFIX(sine, prefix)                                                        \
  sine_t     *prefix##_p   = (sine);                                                               \
  sine_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                     \
  sine_out_t *prefix##_out = &prefix##_p->out;                                                     \
  sine_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                      \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static inline void sine_init(sine_t *sine, sine_cfg_t sine_cfg) {
  DECL_SINE_PTRS(sine);

  *cfg = sine_cfg;
}

static inline void sine_exec(sine_t *sine) {
  DECL_SINE_PTRS(sine);

  lo->phase_incr = TAU * cfg->wave_freq / cfg->fs;
  out->val       = cfg->amp * SIN(cfg->phase) + cfg->offset;
  cfg->phase += lo->phase_incr;
  WARP_TAU(cfg->phase);
}

#ifdef __cplusplus
}
#endif

#endif // !SINE_H
