#ifndef FFT_H
#define FFT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FFT_POINT_SIZE
#define FFT_POINT_SIZE (LF(6))
#endif

#ifdef ARM_MATH
#include "arm_const_structs.h"
#include "arm_math.h"
#endif

#include <string.h>

#include "../util/util.h"

typedef struct {
  f32 fs;
  u8  flag;
#ifdef ARM_MATH
  arm_rfft_fast_instance_f32 S;
#endif
} fft_cfg_t;

typedef struct {
  f32 buf[FFT_POINT_SIZE];
} fft_in_t;

typedef struct {
  f32 buf[FFT_POINT_SIZE];
  f32 mod[FFT_POINT_SIZE];
  u32 res_idx;
  f32 val_max;
  f32 freq_max;
} fft_out_t;

typedef struct {
  bool is_cal_finish;
  u32  added_idx;
} fft_lo_t;

typedef struct {
  fft_cfg_t cfg;
  fft_in_t  in;
  fft_out_t out;
  fft_lo_t  lo;
} fft_t;

#define DECL_FFT_PTRS(fft)                                                                         \
  fft_t     *p   = (fft);                                                                          \
  fft_cfg_t *cfg = &p->cfg;                                                                        \
  fft_in_t  *in  = &p->in;                                                                         \
  fft_out_t *out = &p->out;                                                                        \
  fft_lo_t  *lo  = &p->lo;                                                                         \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_FFT_PTRS_PREFIX(fft, prefix)                                                          \
  fft_t     *prefix##_p   = (fft);                                                                 \
  fft_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                      \
  fft_in_t  *prefix##_in  = &prefix##_p->in;                                                       \
  fft_out_t *prefix##_out = &prefix##_p->out;                                                      \
  fft_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                       \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static void fft_add_value(fft_t *fft, f32 value) {
  DECL_FFT_PTRS(fft);

  if (!lo->is_cal_finish)
    return;

  if (lo->added_idx >= FFT_POINT_SIZE)
    lo->added_idx = 0;

  in->buf[lo->added_idx++] = value;
}

static void fft_init(fft_t *fft, fft_cfg_t fft_cfg) {
  DECL_FFT_PTRS(fft);

  *cfg = fft_cfg;

  cfg->flag = 0u;

#ifdef ARM_MATH
  switch (FFT_POINT_SIZE) {
  case LF(5):
    arm_rfft_fast_init_32_f32(&cfg->S);
    break;
  case LF(6):
    arm_rfft_fast_init_64_f32(&cfg->S);
    break;
  case LF(7):
    arm_rfft_fast_init_128_f32(&cfg->S);
    break;
  case LF(8):
    arm_rfft_fast_init_256_f32(&cfg->S);
    break;
  case LF(9):
    arm_rfft_fast_init_512_f32(&cfg->S);
    break;
  case LF(10):
    arm_rfft_fast_init_1024_f32(&cfg->S);
    break;
  case LF(11):
    arm_rfft_fast_init_2048_f32(&cfg->S);
    break;
  case LF(12):
    arm_rfft_fast_init_4096_f32(&cfg->S);
    break;
  default:
    break;
  }
#endif
}

static void fft_exec(fft_t *fft) {
  DECL_FFT_PTRS(fft);

#ifdef ARM_MATH
  arm_rfft_fast_f32(&cfg->S, in->buf, out->buf, cfg->flag);
  arm_cmplx_mag_f32(out->buf, out->mod, FFT_POINT_SIZE >> 1);
  arm_max_f32(&out->mod[1], FFT_POINT_SIZE >> 1, &out->val_max, &out->res_idx);
#endif

  out->freq_max = out->res_idx * cfg->fs / FFT_POINT_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif // !FFT_H
