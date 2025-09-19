#ifndef LUENBERGER_H
#define LUENBERGER_H

#include "../filter/pll.h"
#include "../util/util.h"

typedef struct {
  f32         fs;
  motor_cfg_t motor;
} lbg_cfg_t;

typedef struct {
} lbg_in_t;

typedef struct {
} lbg_out_t;

typedef struct {
  pll_filter_t pll;
} lbg_lo_t;

typedef struct {
  lbg_cfg_t cfg;
  lbg_in_t  in;
  lbg_out_t out;
  lbg_lo_t  lo;
} lbg_obs_t;

#define DECL_LBG_PTRS(lbg)                                                                         \
  lbg_cfg_t *cfg = &(lbg)->cfg;                                                                    \
  lbg_in_t  *in  = &(lbg)->in;                                                                     \
  lbg_out_t *out = &(lbg)->out;                                                                    \
  lbg_lo_t  *lo  = &(lbg)->lo;                                                                     \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_LBG_PTR_RENAME(lbg, name)                                                             \
  lbg_obs_t *name = (lbg);                                                                         \
  ARG_UNUSED(name);

static void lbg_init(lbg_obs_t *lbg, lbg_cfg_t lbg_cfg) {
  DECL_LBG_PTRS(lbg);

  *cfg = lbg_cfg;
}

static void lbg_exec(lbg_obs_t *smo) {
  DECL_LBG_PTRS(smo);
}

#endif // !LUENBERGER_H
