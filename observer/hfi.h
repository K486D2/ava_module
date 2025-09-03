#ifndef HFI_H
#define HFI_H

#include "filter/pll.h"
#ifdef __cpluscplus
extern "C" {
#endif

#include "../filter/bpf.h"
#include "../transform/clarkepark.h"
#include "../util/def.h"
#include "../util/util.h"

typedef struct {
  f32 fs;
  f32 kp, ki;
  f32 fh;   // 注入信号频率，单位：Hz
  f32 vh;   // 注入信号幅值，单位：V
  f32 id_h; // 注入信号幅值，单位：A
} hfi_cfg_t;

typedef struct {
  f32_ab_t i_ab;
} hfi_in_t;

typedef struct {
  f32 obs_theta; // 估计的电角度，单位：rad
  f32 obs_omega; // 估计的角速度，单位：rad/s
  f32 theta;     // 估计的电角度，单位：rad
  f32 omega;     // 估计的角速度，单位：rad/s
  f32 id_in;     // 注入信号幅值，单位：A
  f32 vd_h;      // 注入信号幅值，单位：V
} hfi_out_t;

typedef struct {
  f32          hfi_theta;
  f32_dq_t     i_dq_hat;
  bpf_filter_t id_bpf, iq_bpf;
  f32          theta_h;
  f32          lpf_id, hfi_theta_err;
  f32          hfi_theta_err_integ;
  u32          polar_cnt;
  f32          id_pos, id_neg;
  f32          polar_offset;
} hfi_lo_t;

typedef struct {
  hfi_cfg_t cfg;
  hfi_in_t  in;
  hfi_lo_t  lo;
  hfi_out_t out;
} hfi_obs_t;

#define DECL_HFI_PTRS(hfi)                                                                         \
  hfi_obs_t *p   = (hfi);                                                                          \
  hfi_cfg_t *cfg = &p->cfg;                                                                        \
  hfi_in_t  *in  = &p->in;                                                                         \
  hfi_out_t *out = &p->out;                                                                        \
  hfi_lo_t  *lo  = &p->lo;                                                                         \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_HFI_PTRS_PREFIX(hfi, prefix)                                                          \
  hfi_obs_t *prefix##_p   = (hfi);                                                                 \
  hfi_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                      \
  hfi_in_t  *prefix##_in  = &prefix##_p->in;                                                       \
  hfi_out_t *prefix##_out = &prefix##_p->out;                                                      \
  hfi_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                       \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static void hfi_init(hfi_obs_t *hfi, hfi_cfg_t hfi_cfg) {
  DECL_HFI_PTRS(hfi);

  *cfg = hfi_cfg;
  bpf_init(&lo->id_bpf, lo->id_bpf.cfg);
  bpf_init(&lo->iq_bpf, lo->iq_bpf.cfg);
}

static void hfi_exec(hfi_obs_t *hfi) {
  DECL_HFI_PTRS(hfi);
  DECL_BPF_PTRS_PREFIX(&lo->id_bpf, id_bpf);
  DECL_BPF_PTRS_PREFIX(&lo->iq_bpf, iq_bpf);

  lo->i_dq_hat = park(in->i_ab, lo->hfi_theta);
  bpf_exec_in(&lo->id_bpf, lo->i_dq_hat.d);
  bpf_exec_in(&lo->iq_bpf, lo->i_dq_hat.q);

  LOWPASS(lo->lpf_id, id_bpf_out->y0 * SIN(lo->theta_h), 300.0f, cfg->fs);
  LOWPASS(lo->hfi_theta_err, iq_bpf_out->y0 * SIN(lo->theta_h), 300.0f, cfg->fs);

  // PLL
  INTEGRATOR(lo->hfi_theta_err_integ, lo->hfi_theta_err, cfg->ki, cfg->fs);
  out->omega = cfg->kp * lo->hfi_theta_err + lo->hfi_theta_err_integ;
  INTEGRATOR(lo->hfi_theta, out->omega, 1.0f, cfg->fs);
  WARP_TAU(lo->hfi_theta);

  // Inject
  INTEGRATOR(lo->theta_h, TAU * cfg->fh, 1.0f, cfg->fs);
  WARP_TAU(lo->theta_h);
  out->vd_h = cfg->vh * COS(lo->theta_h);

  // Polarity
  out->id_in = 0.0f;
  if (++lo->polar_cnt > (u32)(cfg->fs * 0.3f))
    lo->polar_cnt = (u32)(cfg->fs * 0.3f + 1.0f);

  if ((lo->polar_cnt > (u32)(cfg->fs * 0.1f)) && (lo->polar_cnt <= (u32)(cfg->fs * 0.2f))) {
    out->id_in = cfg->id_h;
    lo->id_pos += ABS(lo->lpf_id);
  } else if ((lo->polar_cnt > (u32)(cfg->fs * 0.2f)) && (lo->polar_cnt <= (u32)(cfg->fs * 0.3f))) {
    out->id_in = -cfg->id_h;
    lo->id_neg += ABS(lo->lpf_id);
    if (lo->polar_cnt == (u32)(cfg->fs * 0.3f)) {
      cfg->vh          = (ABS(lo->id_pos) > ABS(lo->id_neg)) ? cfg->vh : -cfg->vh;
      lo->polar_offset = (ABS(lo->id_pos) > ABS(lo->id_neg)) ? 0.0f : PI;
    }
  }

  out->obs_theta = lo->hfi_theta + lo->polar_offset;
  WARP_TAU(out->obs_theta);
}

static void hfi_exec_in(hfi_obs_t *hfi, f32_ab_t i_ab) {
  DECL_HFI_PTRS(hfi);

  in->i_ab = i_ab;
  hfi_exec(hfi);
}

#ifdef __cpluscplus
}
#endif

#endif // !HFI_H
