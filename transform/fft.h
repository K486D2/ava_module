#ifndef FFT_H
#define FFT_H

#ifdef ARM_MATH
#include "arm_const_structs.h"
#include "arm_math.h"
#endif

#if defined(__linux__) || defined(_WIN32)
#include "fftw3.h"
#endif

#include <string.h>

#include "../util/mathdef.h"
#include "../util/util.h"

typedef enum {
  FFT_SIZE_32      = 32,
  FFT_SIZE_64      = 64,
  FFT_SIZE_128     = 128,
  FFT_SIZE_256     = 256,
  FFT_SIZE_512     = 512,
  FFT_SIZE_1024    = 1024,
  FFT_SIZE_2048    = 2048,
  FFT_SIZE_4096    = 4096,
  FFT_SIZE_8192    = 8192,
  FFT_SIZE_16384   = 16384,
  FFT_SIZE_32768   = 32768,
  FFT_SIZE_65536   = 65536,
  FFT_SIZE_131072  = 131072,
  FFT_SIZE_262144  = 262144,
  FFT_SIZE_524288  = 524288,
  FFT_SIZE_1048576 = 1048576,
} fft_size_e;

#define FFT_POINT_SIZE FFT_SIZE_65536
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
#ifdef ARM_MATH
  f32 buf[FFT_POINT_SIZE];
#endif
  size_t out_idx;
  f32    mag[FFT_POINT_SIZE];
  f32    max_mag;
  f32    max_freq;
#if defined(__linux__) || defined(_WIN32)
  fftwf_complex buf[FFT_POINT_SIZE / 2 + 1];
#endif
} fft_out_t;

typedef struct {
  size_t in_idx;
#if defined(__linux__) || defined(_WIN32)
  fftwf_plan p;
#endif
} fft_lo_t;

typedef struct {
  fft_cfg_t cfg;
  fft_in_t  in;
  fft_out_t out;
  fft_lo_t  lo;
} fft_t;

#define DECL_FFT_PTRS(fft)                                                                         \
  fft_cfg_t *cfg = &(fft)->cfg;                                                                    \
  fft_in_t  *in  = &(fft)->in;                                                                     \
  fft_out_t *out = &(fft)->out;                                                                    \
  fft_lo_t  *lo  = &(fft)->lo;                                                                     \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_FFT_PTR_RENAME(fft, name)                                                             \
  fft_t *(name) = (fft);                                                                           \
  ARG_UNUSED(name);

static void fft_init(fft_t *fft, fft_cfg_t fft_cfg) {
  DECL_FFT_PTRS(fft);

  *cfg = fft_cfg;

  cfg->flag = 0;

#ifdef ARM_MATH
  arm_rfft_fast_init_f32(&cfg->S, FFT_POINT_SIZE);
#endif

#if defined(__linux__) || defined(_WIN32)
  lo->p = fftwf_plan_dft_r2c_1d(FFT_POINT_SIZE, in->buf, out->buf, FFTW_ESTIMATE);
#endif
}

static void fft_exec(fft_t *fft) {
  DECL_FFT_PTRS(fft);

#ifdef ARM_MATH
  arm_rfft_fast_f32(&cfg->S, in->buf, out->buf, cfg->flag);
  arm_cmplx_mag_f32(out->buf, out->mag, FFT_POINT_SIZE >> 1);
  arm_max_f32(&out->mag[1], FFT_POINT_SIZE >> 1, &out->max_mag, &out->out_idx);
#endif

#if defined(__linux__) || defined(_WIN32)
  fftwf_execute(lo->p);
  for (int i = 0; i < FFT_POINT_SIZE / 2 + 1; i++)
    out->mag[i] = SQRT(out->buf[i][0] * out->buf[i][0] + out->buf[i][1] * out->buf[i][1]);
  find_max(&out->mag[1], FFT_POINT_SIZE >> 1, &out->max_mag, &out->out_idx);
#endif

  out->max_freq = out->out_idx * cfg->fs / (f32)FFT_POINT_SIZE;
}

static void fft_destroy(fft_t *fft) {
  DECL_FFT_PTRS(fft);

#if defined(__linux__) || defined(_WIN32)
  fftwf_destroy_plan(lo->p);
#endif
}

static void fft_exec_in(fft_t *fft, f32 val) {
  DECL_FFT_PTRS(fft);

  in->buf[lo->in_idx++] = val;
  if (lo->in_idx >= FFT_POINT_SIZE) {
    lo->in_idx = 0;
    fft_exec(fft);

    printf("freq: %f, idx: %zu, mag: %f\n", out->max_freq, out->out_idx, out->max_mag);
  }
}

#endif // !FFT_H
