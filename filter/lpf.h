#ifndef LPF_H
#define LPF_H

#ifdef __cpluscplus
extern "C" {
#endif

#include "../util/util.h"

typedef struct {
  f32 fs; // 采样频率
  f32 fc; // 截止频率
} lpf_cfg_t;

typedef struct {
  f32 raw_val;
} lpf_in_t;

typedef struct {
  f32 filter_val;
} lpf_out_t;

typedef struct {
  lpf_cfg_t cfg;
  lpf_in_t  in;
  lpf_out_t out;
} lpf_filter_t;

#define DECL_LPF_PTRS(lpf)                                                                         \
  lpf_filter_t *p   = (lpf);                                                                       \
  lpf_cfg_t    *cfg = &p->cfg;                                                                     \
  lpf_in_t     *in  = &p->in;                                                                      \
  lpf_out_t    *out = &p->out;                                                                     \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);

#define DECL_LPF_PTRS_PREFIX(lpf, prefix)                                                          \
  lpf_filter_t *prefix##_p   = (lpf);                                                              \
  lpf_cfg_t    *prefix##_cfg = &prefix##_p->cfg;                                                   \
  lpf_in_t     *prefix##_in  = &prefix##_p->in;                                                    \
  lpf_out_t    *prefix##_out = &prefix##_p->out;                                                   \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);

static void lpf_init(lpf_filter_t *lpf, lpf_cfg_t lpf_cfg) {
  DECL_LPF_PTRS(lpf);

  *cfg = lpf_cfg;
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

  LOWPASS(out->filter_val, in->raw_val, cfg->fc, cfg->fs);
}

#ifdef __cpluscplus
}
#endif

#endif // !LPF_H
