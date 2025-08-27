#ifndef PLL_H
#define PLL_H

#include "../util/util.h"

typedef struct {
  f32 fs;
  f32 damp;
  f32 kp;
  f32 ki;
  f32 lpf_fc, lpf_ffd_fc;
} pll_cfg_t;

typedef struct {
  f32_ab_t ab;
  f32      theta;
} pll_in_t;

typedef struct {
  f32 lpf_theta;
  f32 omega, lpf_omega;
} pll_out_t;

typedef struct {
  f32 prev_theta;
  f32 theta_err;
  f32 ki_out;
  f32 ffd_omega, lpf_ffd_omega;
} pll_lo_t;

typedef struct {
  pll_cfg_t cfg;
  pll_in_t  in;
  pll_out_t out;
  pll_lo_t  lo;
} pll_filter_t;

#define DECL_PLL_PTRS(pll)                                                                         \
  pll_filter_t *p   = (pll);                                                                       \
  pll_cfg_t    *cfg = &p->cfg;                                                                     \
  pll_in_t     *in  = &p->in;                                                                      \
  pll_out_t    *out = &p->out;                                                                     \
  pll_lo_t     *lo  = &p->lo;                                                                      \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_PLL_PTRS_PREFIX(pll, prefix)                                                          \
  pll_filter_t *prefix##_p   = (pll);                                                              \
  pll_cfg_t    *prefix##_cfg = &prefix##_p->cfg;                                                   \
  pll_in_t     *prefix##_in  = &prefix##_p->in;                                                    \
  pll_out_t    *prefix##_out = &prefix##_p->out;                                                   \
  pll_lo_t     *prefix##_lo  = &prefix##_p->lo;                                                    \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static void pll_init(pll_filter_t *pll, pll_cfg_t pll_cfg) {
  DECL_PLL_PTRS(pll);

  *cfg = pll_cfg;
}

static void pll_exec(pll_filter_t *pll) {
  DECL_PLL_PTRS(pll);

  // LF环路滤波器
  INTEGRATOR(lo->ki_out, lo->theta_err, cfg->ki, cfg->fs);
  out->omega = cfg->kp * lo->theta_err + lo->ki_out;
  LOWPASS(out->lpf_omega, out->omega, cfg->lpf_fc, cfg->fs);

  // VCO压控振荡器
  INTEGRATOR(out->lpf_theta, out->omega, 1.0f, cfg->fs);
  WARP_TAU(out->lpf_theta);
}

static void pll_exec_ab_in(pll_filter_t *pll, f32_ab_t ab) {
  DECL_PLL_PTRS(pll);

  in->ab = ab;

  // PD鉴相器
  lo->theta_err = in->ab.b * COS(out->lpf_theta) - in->ab.a * SIN(out->lpf_theta);

  pll_exec(p);
}

static void pll_exec_theta_in(pll_filter_t *pll, f32 theta) {
  DECL_PLL_PTRS(pll);

  in->theta = theta;

  // 前馈速度计算
  THETA_DERIVATIVE(lo->ffd_omega, in->theta, lo->prev_theta, 1.0f, cfg->fs);
  LOWPASS(lo->lpf_ffd_omega, lo->ffd_omega, cfg->lpf_fc, cfg->fs);

  // PD鉴相器
  lo->theta_err = in->theta - out->lpf_theta;
  WARP_PI(lo->theta_err);

  pll_exec(p);
}

#endif // !PLL_H
