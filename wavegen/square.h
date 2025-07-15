#ifndef SQUARE_H
#define SQUARE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../util/def.h"

typedef struct {
  fp32 fs;
  fp32 wave_freq;  // 方波频率 (Hz)
  fp32 amp;        // 方波幅度
  fp32 phase;      // 当前相位 (rad)
  fp32 offset;     // 直流偏移量
  fp32 duty_cycle; // 占空比 (0~1)
} square_cfg_t;

typedef struct {
  fp32 val;
} square_out_t;

typedef struct {
  fp32 phase_incr;
} square_lo_t;

typedef struct {
  square_cfg_t cfg;
  square_out_t out;
  square_lo_t  lo;
} square_t;

#define DECL_SQUARE_PTRS(square)                                                                   \
  square_t     *p   = (square);                                                                    \
  square_cfg_t *cfg = &p->cfg;                                                                     \
  square_out_t *out = &p->out;                                                                     \
  square_lo_t  *lo  = &p->lo;                                                                      \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_SQUARE_PTRS_PREFIX(square, prefix)                                                    \
  square_t     *prefix##_p   = (square);                                                           \
  square_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                   \
  square_out_t *prefix##_out = &prefix##_p->out;                                                   \
  square_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                    \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static inline void square_init(square_t *square, square_cfg_t square_cfg) {
  DECL_SQUARE_PTRS(square);

  *cfg = square_cfg;
}

static inline void square_exec(square_t *square) {
  DECL_SQUARE_PTRS(square);

  lo->phase_incr = TAU * cfg->wave_freq * HZ_TO_S(cfg->fs);

  if (cfg->phase < (cfg->duty_cycle * TAU))
    out->val = cfg->amp + cfg->offset;
  else
    out->val = -cfg->amp + cfg->offset;

  cfg->phase += lo->phase_incr;
  WARP_TAU(cfg->phase);
}

#ifdef __cplusplus
}
#endif

#endif // !SQUARE_H
