#ifndef SMO_H
#define SMO_H

#include "../filter/pll.h"
#include "../util/util.h"

typedef struct {
  f32         fs;
  f32         ks;
  f32         es0;
  motor_cfg_t motor_cfg;
} smo_cfg_t;

typedef struct {
  f32_ab_t i_ab, v_ab;
} smo_in_t;

typedef struct {
  f32 theta;
  f32 omega;
} smo_out_t;

typedef struct {
  f32_ab_t est_i_ab;
  f32_ab_t est_i_ab_err;
  f32_ab_t est_emf_v_ab;

  pll_filter_t pll;
} smo_lo_t;

typedef struct {
  smo_cfg_t cfg;
  smo_in_t  in;
  smo_out_t out;
  smo_lo_t  lo;
} smo_obs_t;

#define DECL_SMO_PTRS(smo)                                                                         \
  smo_cfg_t *cfg = &(smo)->cfg;                                                                    \
  smo_in_t  *in  = &(smo)->in;                                                                     \
  smo_out_t *out = &(smo)->out;                                                                    \
  smo_lo_t  *lo  = &(smo)->lo;                                                                     \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_SMO_PTR_RENAME(smo, name)                                                             \
  smo_obs_t *(name) = (smo);                                                                       \
  ARG_UNUSED(name);

static void smo_init(smo_obs_t *smo, smo_cfg_t smo_cfg) {
  DECL_SMO_PTRS(smo);

  *cfg = smo_cfg;

  pll_init(&lo->pll, lo->pll.cfg);
}

/**
 * @brief 滑膜观测器
 *
 * @details
 *
 * $\theta_{r} = \arctan(\frac{-e_{s \alpha}}{e_{s \beta}})$
 *
 * @param smo
 */
static void smo_exec(smo_obs_t *smo) {
  DECL_SMO_PTRS(smo);
  DECL_PLL_PTR_RENAME(&smo->lo.pll, pll);

  // 电流误差方程
  INTEGRATOR(lo->est_i_ab.a,
             (in->v_ab.a - in->i_ab.a * cfg->motor_cfg.rs - lo->est_emf_v_ab.a) / cfg->motor_cfg.ls,
             1.0f,
             cfg->fs);

  INTEGRATOR(lo->est_i_ab.b,
             (in->v_ab.b - in->i_ab.b * cfg->motor_cfg.rs - lo->est_emf_v_ab.b) / cfg->motor_cfg.ls,
             1.0f,
             cfg->fs);

  AB_SUB_VEC(lo->est_i_ab_err, lo->est_i_ab, in->i_ab);

  // 反电动势估算
  lo->est_emf_v_ab.a = (ABS(lo->est_i_ab_err.a) > cfg->es0)
                           ? CPYSGN(cfg->ks, lo->est_i_ab_err.a)
                           : (cfg->ks * lo->est_i_ab_err.a / cfg->es0);

  lo->est_emf_v_ab.b = (ABS(lo->est_i_ab_err.b) > cfg->es0)
                           ? CPYSGN(cfg->ks, lo->est_i_ab_err.b)
                           : (cfg->ks * lo->est_i_ab_err.b / cfg->es0);

  pll_exec_ab_in(&lo->pll, lo->est_emf_v_ab);
  out->omega = pll->out.omega;

  out->theta = ATAN2(-lo->est_emf_v_ab.a * out->omega, lo->est_emf_v_ab.b * out->omega);
  WARP_TAU(out->theta);
}

static void smo_exec_in(smo_obs_t *smo, f32_ab_t i_ab, f32_ab_t v_ab) {
  DECL_SMO_PTRS(smo);

  in->i_ab = i_ab;
  in->v_ab = v_ab;
  smo_exec(smo);
}

#endif // !SMO_H
