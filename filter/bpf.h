#ifndef BPF_H
#define BPF_H

#ifdef __cpluscplus
extern "C" {
#endif

#include "../util/util.h"

typedef struct {
  f32 fs;       // 采样频率
  f32 f_center; // 中心频率
  f32 bw;       // 带宽
} bpf_cfg_t;

typedef struct {
  f32 x0;
} bpf_in_t;

typedef struct {
  f32 y0;
} bpf_out_t;

typedef struct {
  f32 w0;
  f32 alpha;
  f32 cosw0;

  // 滤波器状态变量
  f32 x1, x2; // 前两个输入
  f32 y1, y2; // 前两个输出

  // 滤波器系数
  f32 b0, b1, b2;
  f32 a0, a1, a2;
} bpf_lo_t;

typedef struct {
  bpf_cfg_t cfg;
  bpf_in_t  in;
  bpf_out_t out;
  bpf_lo_t  lo;
} bpf_filter_t;

#define DECL_BPF_PTRS(bpf)                                                                         \
  bpf_filter_t *p   = (bpf);                                                                       \
  bpf_cfg_t    *cfg = &p->cfg;                                                                     \
  bpf_in_t     *in  = &p->in;                                                                      \
  bpf_out_t    *out = &p->out;                                                                     \
  bpf_lo_t     *lo  = &p->lo;                                                                      \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_BPF_PTRS_PREFIX(bpf, prefix)                                                          \
  bpf_filter_t *prefix##_p   = (bpf);                                                              \
  bpf_cfg_t    *prefix##_cfg = &prefix##_p->cfg;                                                   \
  bpf_in_t     *prefix##_in  = &prefix##_p->in;                                                    \
  bpf_out_t    *prefix##_out = &prefix##_p->out;                                                   \
  bpf_lo_t     *prefix##_lo  = &prefix##_p->lo;                                                    \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static void bpf_init(bpf_filter_t *bpf, bpf_cfg_t bpf_cfg) {
  DECL_BPF_PTRS(bpf);

  *cfg = bpf_cfg;

  lo->w0    = 2.0f * PI * cfg->f_center / cfg->fs;
  lo->alpha = SIN(lo->w0) * SINH(LOG(2.0f) / 2.0f * cfg->bw * lo->w0 / SIN(lo->w0));
  lo->cosw0 = COS(lo->w0);

  lo->a0 = 1.0f + lo->alpha;

  lo->b0 = lo->alpha / lo->a0;
  lo->b1 = 0.0f;
  lo->b2 = -lo->alpha / lo->a0;
  lo->a1 = -2.0f * lo->cosw0 / lo->a0;
  lo->a2 = (1.0f - lo->alpha) / lo->a0;
}

/**
 * @brief 二阶带通滤波器
 *
 * @details 差分方程: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
 *
 * @param bpf
 */
static void bpf_exec(bpf_filter_t *bpf) {
  DECL_BPF_PTRS(bpf);

  out->y0 = lo->b0 * in->x0 + lo->b1 * lo->x1 + lo->b2 * lo->x2 - lo->a1 * lo->y1 - lo->a2 * lo->y2;

  lo->x2 = lo->x1;
  lo->x1 = in->x0;
  lo->y2 = lo->y1;
  lo->y1 = out->y0;
}

#ifdef __cpluscplus
}
#endif

#endif // !BPF_H
