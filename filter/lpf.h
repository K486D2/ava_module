#ifndef LPF_H
#define LPF_H

#include "../util/util.h"

typedef struct {
  f32 fs;
  f32 fc;
  f32 rc;
  f32 alpha;
} lpf_cfg_t;

typedef struct {
  f32 x;
} lpf_in_t;

typedef struct {
  f32 y;
} lpf_out_t;

typedef struct {
  lpf_cfg_t cfg;
  lpf_in_t  in;
  lpf_out_t out;
} lpf_filter_t;

#define DECL_LPF_PTRS(lpf)                                                                         \
  lpf_cfg_t *cfg = &(lpf)->cfg;                                                                    \
  lpf_in_t  *in  = &(lpf)->in;                                                                     \
  lpf_out_t *out = &(lpf)->out;                                                                    \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);

#define DECL_LPF_PTRS_RENAME(lpf, name)                                                            \
  lpf_filter_t *name = (lpf);                                                                      \
  ARG_UNUSED(name);

static void lpf_init(lpf_filter_t *lpf, lpf_cfg_t lpf_cfg) {
  DECL_LPF_PTRS(lpf);

  *cfg = lpf_cfg;

  cfg->rc    = 1.0f / (TAU * cfg->fc);
  cfg->alpha = 1.0f / (1.0f + cfg->rc * cfg->fs);
}

/**
 * @brief 一阶低通滤波器

 * @details $y(n) = a x(n) + (1 - a)x(n - 1)$
 *
 * $f_{c}$: 截止频率
 * $f_{s}$: 采样频率
 *
 * @param lpf
 */
static void lpf_exec(lpf_filter_t *lpf) {
  DECL_LPF_PTRS(lpf);

  out->y = cfg->alpha * in->x + (1.0f - cfg->alpha) * out->y;
}

#endif // !LPF_H
