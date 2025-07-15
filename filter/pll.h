#ifndef PLL_H
#define PLL_H

#include "../util/util.h"

typedef struct {
  fp32 fs;
  fp32 lpf_fc;
  fp32 lpf_ffd_fc;
  fp32 damp;
  fp32 kp;
  fp32 ki;
} pll_cfg_t;

typedef struct {
  fp32_ab_t val_ab;
} pll_in_t;

typedef struct {
  fp32 theta;
} pll_theta_in_t;

typedef struct {
  fp32 filter_theta;
  fp32 omega;
  fp32 filter_omega;
} pll_out_t;

typedef struct {
  fp32 theta_err;
  fp32 ki_out;
  fp32 pll_ffd;
} pll_lo_t;

typedef struct {
  fp32 theta_err;
  fp32 prev_theta;
  fp32 ffd_omega;
  fp32 ki_out;
  fp32 filter_ffd_omega;
} pll_theta_lo_t;

typedef struct {
  pll_cfg_t cfg;
  pll_in_t  in;
  pll_out_t out;
  pll_lo_t  lo;
} pll_filter_t;

typedef struct {
  pll_cfg_t      cfg;
  pll_theta_in_t in;
  pll_out_t      out;
  pll_theta_lo_t lo;
} pll_theta_filter_t;

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

#define DECL_PLL_THETA_PTRS(pll)                                                                   \
  pll_theta_filter_t *p   = (pll);                                                                 \
  pll_cfg_t          *cfg = &p->cfg;                                                               \
  pll_theta_in_t     *in  = &p->in;                                                                \
  pll_out_t          *out = &p->out;                                                               \
  pll_theta_lo_t     *lo  = &p->lo;                                                                \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_PLL_THETA_PTRS_PREFIX(pll, prefix)                                                    \
  pll_theta_filter_t *prefix##_p   = (pll);                                                        \
  pll_cfg_t          *prefix##_cfg = &prefix##_p->cfg;                                             \
  pll_theta_in_t     *prefix##_in  = &prefix##_p->in;                                              \
  pll_out_t          *prefix##_out = &prefix##_p->out;                                             \
  pll_theta_lo_t     *prefix##_lo  = &prefix##_p->lo;                                              \
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

  // PD鉴相器
  lo->theta_err = in->val_ab.b * COS(out->filter_theta) - in->val_ab.a * SIN(out->filter_theta);

  // LF环路滤波器
  INTEGRATOR(lo->ki_out, lo->theta_err, cfg->ki, cfg->fs);
  out->omega = cfg->kp * lo->theta_err + lo->ki_out;
  LOWPASS(out->filter_omega, out->omega, cfg->lpf_fc, cfg->fs);

  // VCO压控振荡器
  INTEGRATOR(out->filter_theta, out->omega, 1.0f, cfg->fs);
  WARP_TAU(out->filter_theta);
}

static void pll_exec_in(pll_filter_t *pll, fp32_ab_t val_ab) {
  DECL_PLL_PTRS(pll);

  in->val_ab = val_ab;
  pll_exec(p);
}

static void pll_theta_init(pll_theta_filter_t *theta_pll, pll_cfg_t pll_cfg) {
  DECL_PLL_THETA_PTRS(theta_pll);

  *cfg = pll_cfg;
}

static void pll_theta_exec(pll_theta_filter_t *pll) {
  DECL_PLL_THETA_PTRS(pll);

  // 前馈速度计算
  THETA_DERIVATIVE(lo->ffd_omega, in->theta, lo->prev_theta, 1.0f, cfg->fs);
  LOWPASS(lo->filter_ffd_omega, lo->ffd_omega, cfg->lpf_fc, cfg->fs);

  // PD鉴相器
  lo->theta_err = in->theta - out->filter_theta;
  WARP_PI(lo->theta_err);

  // LF环路滤波器
  INTEGRATOR(lo->ki_out, lo->theta_err, cfg->ki, cfg->fs);
  out->omega = cfg->kp * lo->theta_err + lo->ki_out + lo->filter_ffd_omega;
  LOWPASS(out->filter_omega, out->omega, cfg->lpf_fc, cfg->fs);

  // VCO压控振荡器
  INTEGRATOR(out->filter_theta, out->omega, 1.0f, cfg->fs);
  WARP_TAU(out->filter_theta);
}

static void pll_theta_exec_in(pll_theta_filter_t *pll, fp32 theta) {
  DECL_PLL_THETA_PTRS(pll);

  in->theta = theta;
  pll_theta_exec(p);
}

#endif // !PLL_H
