#ifndef FFT_H
#define FFT_H

#if defined(__linux__) || defined(_WIN32)
#include "fftw3.h"
#elif defined(ARM_MATH)
#include "arm_const_structs.h"
#include "arm_math.h"
#endif

#include <string.h>

#include "../container/fifo.h"
#include "../util/mathdef.h"
#include "../util/util.h"

typedef enum {
  FFT_POINT_32      = 32,
  FFT_POINT_64      = 64,
  FFT_POINT_128     = 128,
  FFT_POINT_256     = 256,
  FFT_POINT_512     = 512,
  FFT_POINT_1024    = 1024,
  FFT_POINT_2048    = 2048,
  FFT_POINT_4096    = 4096,
  FFT_POINT_8192    = 8192,
  FFT_POINT_16384   = 16384,
  FFT_POINT_32768   = 32768,
  FFT_POINT_65536   = 65536,
  FFT_POINT_131072  = 131072,
  FFT_POINT_262144  = 262144,
  FFT_POINT_524288  = 524288,
  FFT_POINT_1048576 = 1048576,
} fft_size_e;

typedef struct {
  f32    fs;
  u8     flag;
  size_t point_num;
  f32   *fifo_buf;
  f32   *in_buf;
#if defined(__linux__) || defined(_WIN32)
  fftwf_complex *out_buf;
#elif defined(ARM_MATH)
  f32 *out_buf;
#endif
  f32 *mag_buf;
} fft_cfg_t;

typedef struct {
  f32 *buf;
} fft_in_t;

typedef struct {
  f32    fr;
  f32    ft;
  size_t out_idx;
  f32   *mag_buf;
  f32    max_mag;
} fft_out_t;

typedef struct {
  u32    elapsed_us;
  fifo_t fifo;
  bool   neet_exec;
#if defined(__linux__) || defined(_WIN32)
  fftwf_plan     p;
  fftwf_complex *buf;
#elif defined(ARM_MATH)
  arm_rfft_fast_instance_f32 s;
  f32                       *buf;
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
  fft_t *name = (fft);                                                                             \
  ARG_UNUSED(name);

static inline void fft_init(fft_t *fft, fft_cfg_t fft_cfg) {
  DECL_FFT_PTRS(fft);

  *cfg = fft_cfg;

  fifo_init(&lo->fifo, cfg->fifo_buf, cfg->point_num * sizeof(f32), FIFO_POLICY_DISCARD);

  in->buf      = cfg->in_buf;
  lo->buf      = cfg->out_buf;
  out->mag_buf = cfg->mag_buf;

  out->fr = cfg->fs / (f32)cfg->point_num;

#if defined(__linux__) || defined(_WIN32)
  lo->p = fftwf_plan_dft_r2c_1d(cfg->point_num, in->buf, lo->buf, FFTW_ESTIMATE);
#elif defined(ARM_MATH)
  arm_rfft_fast_init_f32(&lo->s, (u16)cfg->point_num);
#endif
}

static inline void fft_exec(fft_t *fft) {
  DECL_FFT_PTRS(fft);

  if (!lo->neet_exec)
    return;

  u64 start = get_mono_ts_us();

  memcpy(in->buf, lo->fifo.buf, lo->fifo.buf_size);

#if defined(__linux__) || defined(_WIN32)
  fftwf_execute(lo->p);
  for (int i = 0; i < cfg->point_num / 2 + 1; i++)
    out->mag_buf[i] = SQRT(lo->buf[i][0] * lo->buf[i][0] + lo->buf[i][1] * lo->buf[i][1]);
  find_max(&out->mag_buf[1], cfg->point_num >> 1, &out->max_mag, &out->out_idx);
#elif defined(ARM_MATH)
  arm_hanning_f32(lo->buf, cfg->point_num);
  arm_rfft_fast_f32(&lo->s, in->buf, lo->buf, cfg->flag);
  arm_cmplx_mag_f32(lo->buf, out->mag_buf, cfg->point_num >> 1);
  arm_max_f32(&out->mag_buf[1], cfg->point_num >> 1, &out->max_mag, &out->out_idx);
#endif

  out->ft = out->out_idx * cfg->fs / (f32)cfg->point_num;

  lo->neet_exec = false;

  lo->elapsed_us = (u32)(get_mono_ts_us() - start);
}

static inline void fft_destroy(fft_t *fft) {
  DECL_FFT_PTRS(fft);

#if defined(__linux__) || defined(_WIN32)
  fftwf_destroy_plan(lo->p);
#endif
}

static inline void fft_exec_in(fft_t *fft, f32 val) {
  DECL_FFT_PTRS(fft);

  if (fifo_spsc_in(&lo->fifo, &val, sizeof(val)) == 0)
    lo->neet_exec = true;
}

#endif // !FFT_H
