#ifndef IIR_H
#define IIR_H

#include "../util/errdef.h"
#include "../util/util.h"

typedef enum {
  IIR_1 = 1,
  IIR_2 = 2,
} iir_order_e;

typedef enum {
  IIR_LOWPASS,
  IIR_HIGHPASS,
  IIR_BANDPASS,
} iir_type_e;

typedef struct {
  f32         fs;
  f32         fc;
  f32         q;
  iir_order_e order;
  iir_type_e  type;
} iir_cfg_t;

typedef struct {
  f32 x;
} iir_in_t;

typedef struct {
  f32 y;
} iir_out_t;

typedef struct {
  f32 rc;
  f32 normal_a0, normal_a1, normal_a2, normal_a3, normal_a4;
  f32 x1, x2, y1, y2;
  f32 w0, sin_w0, cos_w0, alpha;
  f32 b0, b1, b2;
  f32 a0, a1, a2;
} iir_lo_t;

typedef struct {
  iir_cfg_t cfg;
  iir_in_t  in;
  iir_out_t out;
  iir_lo_t  lo;
} iir_filter_t;

#define DECL_IIR_PTRS(iir)                                                                         \
  iir_cfg_t *cfg = &(iir)->cfg;                                                                    \
  iir_in_t  *in  = &(iir)->in;                                                                     \
  iir_out_t *out = &(iir)->out;                                                                    \
  iir_lo_t  *lo  = &(iir)->lo;                                                                     \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_IIR_PTR_RENAME(iir, name)                                                             \
  iir_filter_t *(name) = (iir);                                                                    \
  ARG_UNUSED(name);

static int iir_init(iir_filter_t *iir, iir_cfg_t iir_cfg) {
  ARG_CHECK(iir);
  DECL_IIR_PTRS(iir);

  *cfg = iir_cfg;

  switch (cfg->order) {
  case IIR_1: {
    lo->rc    = 1.0f / (TAU * cfg->fc);
    lo->alpha = 1.0f / (1.0f + lo->rc * cfg->fs);

    switch (cfg->type) {
    case IIR_LOWPASS: {
      lo->b0        = lo->alpha;
      lo->b1        = 0.0f;
      lo->normal_a1 = -(1.0f - lo->alpha);
    } break;
    case IIR_HIGHPASS: {
      lo->b0        = 1.0f - lo->alpha;
      lo->b1        = -(1.0f - lo->alpha);
      lo->normal_a1 = -(1.0f - lo->alpha);
    } break;
    default:
      break;
    }
  } break;

  case IIR_2: {
    lo->w0     = TAU * cfg->fc / cfg->fs;
    lo->sin_w0 = SIN(lo->w0);
    lo->cos_w0 = COS(lo->w0);
    lo->alpha  = lo->sin_w0 / (2.0f * cfg->q);

    switch (cfg->type) {
    case IIR_LOWPASS: {
      lo->b0 = (1.0f - lo->cos_w0) / 2.0f;
      lo->b1 = lo->b0 * 2.0f;
      lo->b2 = lo->b0;
      lo->a0 = 1.0f + lo->alpha;
      lo->a1 = -2.0f * lo->cos_w0;
      lo->a2 = 1.0f - lo->alpha;
    } break;
    case IIR_HIGHPASS: {
      lo->b0 = (1.0f + lo->cos_w0) / 2.0f;
      lo->b1 = -lo->b0 * 2.0f;
      lo->b2 = lo->b0;
      lo->a0 = 1.0f + lo->alpha;
      lo->a1 = -2.0f * lo->cos_w0;
      lo->a2 = 1.0f - lo->alpha;
    } break;
    case IIR_BANDPASS: {
      lo->b0 = lo->alpha;
      lo->b1 = 0.0f;
      lo->b2 = -lo->alpha;
      lo->a0 = 1.0f + lo->alpha;
      lo->a1 = -2.0f * lo->cos_w0;
      lo->a2 = 1.0f - lo->alpha;
    } break;
    default:
      break;
    }

    // 归一化
    lo->normal_a0 = lo->b0 / lo->a0;
    lo->normal_a1 = lo->b1 / lo->a0;
    lo->normal_a2 = lo->b2 / lo->a0;
    lo->normal_a3 = lo->a1 / lo->a0;
    lo->normal_a4 = lo->a2 / lo->a0;
  } break;
  default:
    break;
  }

  return 0;
}

static int iir_exec(iir_filter_t *iir) {
  ARG_CHECK(iir);
  DECL_IIR_PTRS(iir);

  switch (cfg->order) {
  case IIR_1: {
    out->y = lo->b0 * in->x + lo->b1 * lo->x1 - lo->normal_a1 * lo->y1;
    lo->x1 = in->x;
    lo->y1 = out->y;
  } break;
  case IIR_2: {
    out->y = lo->normal_a0 * in->x + lo->normal_a1 * lo->x1 + lo->normal_a2 * lo->x2 -
             lo->normal_a3 * lo->y1 - lo->normal_a4 * lo->y2;
    lo->x2 = lo->x1;
    lo->x1 = in->x;
    lo->y2 = lo->y1;
    lo->y1 = out->y;
  } break;
  default:
    break;
  }

  return 0;
}

static int iir_exec_in(iir_filter_t *iir, f32 x) {
  ARG_CHECK(iir);
  DECL_IIR_PTRS(iir);

  in->x = x;
  return iir_exec(iir);
}

#endif // !IIR_H
