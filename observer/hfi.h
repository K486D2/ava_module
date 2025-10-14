#ifndef HFI_H
#define HFI_H

#include "filter/iir.h"
#include "filter/pll.h"
#include "transform/clarkepark.h"
#include "util/def.h"
#include "util/util.h"

typedef struct {
  f32 fs;
  f32 fh;
  f32 hfi_vd, hfi_id;
  f32 id_lpf_fc, iq_lpf_fc;
} hfi_cfg_t;

typedef struct {
  f32_dq_t i_dq;
} hfi_in_t;

typedef struct {
  f32 est_theta;
  f32 est_omega;
  f32 id;
  f32 vd;
} hfi_out_t;

typedef enum {
  HFI_POLAR_IDF_NULL,
  HFI_POLAR_IDF_POSITIVE,
  HFI_POLAR_IDF_NEGATIVE,
  HFI_POALR_IDF_FINISH,
} polar_idf_e;

typedef struct {
  f32 hfi_id, hfi_iq;
  f32 hfi_theta;
  f32 lpf_id, hfi_theta_err;
  f32 hfi_theta_err_integ;

  // 极性辨识
  polar_idf_e e_polar_idf;
  u32         polar_cnt, polar_cnt_max;
  f32         id_pos, id_neg;
  f32         polar_offset;

  iir_filter_t id_bpf, iq_bpf;
  pll_filter_t pll;
} hfi_lo_t;

typedef struct {
  hfi_cfg_t cfg;
  hfi_in_t  in;
  hfi_out_t out;
  hfi_lo_t  lo;
} hfi_obs_t;

#define DECL_HFI_PTRS(hfi)                                                                         \
  hfi_cfg_t *cfg = &(hfi)->cfg;                                                                    \
  hfi_in_t  *in  = &(hfi)->in;                                                                     \
  hfi_out_t *out = &(hfi)->out;                                                                    \
  hfi_lo_t  *lo  = &(hfi)->lo;                                                                     \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_HFI_PTR_RENAME(hfi, name)                                                             \
  hfi_obs_t *name = (hfi);                                                                         \
  ARG_UNUSED(name);

static inline void hfi_init(hfi_obs_t *hfi, hfi_cfg_t hfi_cfg) {
  DECL_HFI_PTRS(hfi);

  *cfg = hfi_cfg;

  lo->pll.cfg.fs = lo->id_bpf.cfg.fs = lo->iq_bpf.cfg.fs = cfg->fs;
  lo->polar_cnt_max                                      = (u32)(cfg->fs / 3.0f);

  pll_init(&lo->pll, lo->pll.cfg);
  iir_init(&lo->id_bpf, lo->id_bpf.cfg);
  iir_init(&lo->iq_bpf, lo->iq_bpf.cfg);
}

static inline void hfi_polar_idf(hfi_obs_t *hfi) {
  DECL_HFI_PTRS(hfi);

  out->id = 0.0f;
  switch (lo->e_polar_idf) {
  case HFI_POLAR_IDF_NULL: {
    if (lo->polar_cnt > lo->polar_cnt_max * 1)
      lo->e_polar_idf = HFI_POLAR_IDF_POSITIVE;
  } break;
  case HFI_POLAR_IDF_POSITIVE: {
    out->id = cfg->hfi_id;
    lo->id_pos += ABS(lo->lpf_id);
    if (lo->polar_cnt > lo->polar_cnt_max * 2)
      lo->e_polar_idf = HFI_POLAR_IDF_NEGATIVE;
  } break;
  case HFI_POLAR_IDF_NEGATIVE: {
    out->id = -cfg->hfi_id;
    lo->id_neg += ABS(lo->lpf_id);
    if (lo->polar_cnt == lo->polar_cnt_max * 3) {
      // cfg->hfi_vd      = (ABS(lo->id_pos) > ABS(lo->id_neg)) ? cfg->hfi_vd : -cfg->hfi_vd;
      lo->polar_offset = (ABS(lo->id_pos) > ABS(lo->id_neg)) ? 0.0f : PI;
      lo->polar_cnt    = 0;
      lo->e_polar_idf  = HFI_POALR_IDF_FINISH;
    }
  } break;
  case HFI_POALR_IDF_FINISH:
    return;
  default:
    break;
  }

  lo->polar_cnt++;
}

static inline void hfi_exec(hfi_obs_t *hfi) {
  DECL_HFI_PTRS(hfi);
  DECL_IIR_PTR_RENAME(&lo->id_bpf, id_bpf);
  DECL_IIR_PTR_RENAME(&lo->iq_bpf, iq_bpf);

  iir_exec_in(id_bpf, in->i_dq.d);
  iir_exec_in(iq_bpf, in->i_dq.q);

  lo->hfi_id = id_bpf->out.y * SIN(lo->hfi_theta);
  lo->hfi_iq = iq_bpf->out.y * SIN(lo->hfi_theta);
  LOWPASS(lo->lpf_id, lo->hfi_id, cfg->id_lpf_fc, cfg->fs);
  LOWPASS(lo->hfi_theta_err, lo->hfi_iq, cfg->iq_lpf_fc, cfg->fs);

  // PLL
  DECL_PLL_PTR_RENAME(&lo->pll, pll);
  pll->lo.theta_err = lo->hfi_theta_err;
  pll_exec(pll);

  // 注入
  INTEGRATOR(lo->hfi_theta, TAU * cfg->fh, 1.0f, cfg->fs);
  WARP_TAU(lo->hfi_theta);
  out->vd = cfg->hfi_vd * COS(lo->hfi_theta);

  // 极性辨识
  hfi_polar_idf(hfi);

  // 角度计算
  out->est_theta = pll->out.theta + lo->polar_offset;
  WARP_TAU(out->est_theta);
  out->est_omega = pll->out.omega;
}

static inline void hfi_exec_in(hfi_obs_t *hfi, f32_dq_t i_dq) {
  DECL_HFI_PTRS(hfi);

  in->i_dq = i_dq;
  hfi_exec(hfi);
}

#endif // !HFI_H
