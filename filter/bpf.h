#ifndef BPF_H
#define BPF_H

#include "../util/errdef.h"
#include "../util/util.h"

typedef struct {
  f32 fs;       // 采样频率
  f32 f_center; // 中心频率
  f32 bw;       // 带宽
  f32 q;
  f32 w0;
  f32 alpha;
  f32 cosw;

  // 滤波器系数
  f32 b0, b1, b2;
  f32 a0, a1, a2;
} bpf_cfg_t;

typedef struct {
  f32 x;
} bpf_in_t;

typedef struct {
  f32 y;
} bpf_out_t;

typedef struct {
  f32 x1, x2; // 前两个输入
  f32 y1, y2; // 前两个输出
} bpf_lo_t;

typedef struct {
  bpf_cfg_t cfg;
  bpf_in_t  in;
  bpf_out_t out;
  bpf_lo_t  lo;
} bpf_filter_t;

#define DECL_BPF_PTRS(bpf)                                                                         \
  bpf_cfg_t *cfg = &(bpf)->cfg;                                                                    \
  bpf_in_t  *in  = &(bpf)->in;                                                                     \
  bpf_out_t *out = &(bpf)->out;                                                                    \
  bpf_lo_t  *lo  = &(bpf)->lo;                                                                     \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_BPF_PTRS_RENAME(bpf, name)                                                            \
  bpf_filter_t *name = (bpf);                                                                      \
  ARG_UNUSED(name);

static int bpf_init(bpf_filter_t *bpf, bpf_cfg_t bpf_cfg) {
  ARG_CHECK(bpf);
  DECL_BPF_PTRS(bpf);

  *cfg = bpf_cfg;

  cfg->q     = cfg->f_center / cfg->bw;
  cfg->w0    = TAU * cfg->f_center / cfg->fs;
  cfg->alpha = SIN(cfg->w0) / (2.0f * cfg->q);
  cfg->cosw  = COS(cfg->w0);

  cfg->a0 = 1.0f + cfg->alpha;

  cfg->b0 = cfg->alpha / cfg->a0;
  cfg->b1 = 0.0f / cfg->a0;
  cfg->b2 = -cfg->alpha / cfg->a0;
  cfg->a1 = -2.0f * cfg->cosw / cfg->a0;
  cfg->a2 = (1.0f - cfg->alpha) / cfg->a0;

  return 0;
}

/**
 * @brief 二阶带通滤波器
 *
 * @details 差分方程: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
 *
 * @param bpf
 */
static int bpf_exec(bpf_filter_t *bpf) {
  ARG_CHECK(bpf);
  DECL_BPF_PTRS(bpf);

  out->y =
      cfg->b0 * in->x + cfg->b1 * lo->x1 + cfg->b2 * lo->x2 - cfg->a1 * lo->y1 - cfg->a2 * lo->y2;

  lo->x2 = lo->x1;
  lo->x1 = in->x;
  lo->y2 = lo->y1;
  lo->y1 = out->y;

  return 0;
}

static int bpf_exec_in(bpf_filter_t *bpf, f32 x) {
  ARG_CHECK(bpf);
  DECL_BPF_PTRS(bpf);

  in->x = x;
  return bpf_exec(bpf);
}

#endif // !BPF_H
