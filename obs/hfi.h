#ifndef HFI_H
#define HFI_H

#include "filter/iir.h"
#include "filter/pll.h"
#include "trans/clarkepark.h"
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
        HFI_POLAR_IDF_READY,
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

HAPI void
hfi_init(hfi_obs_t *hfi, hfi_cfg_t hfi_cfg)
{
        DECL_PTRS(hfi, cfg, lo);

        *cfg = hfi_cfg;

        lo->pll.cfg.fs = lo->id_bpf.cfg.fs = lo->iq_bpf.cfg.fs = cfg->fs;

        lo->polar_cnt_max = (u32)(cfg->fs / 3.0f);

        pll_init(&lo->pll, lo->pll.cfg);
        iir_init(&lo->id_bpf, lo->id_bpf.cfg);
        iir_init(&lo->iq_bpf, lo->iq_bpf.cfg);
}

HAPI void
hfi_polar_idf(hfi_obs_t *hfi)
{
        DECL_PTRS(hfi, cfg, out, lo);

        switch (lo->e_polar_idf) {
                case HFI_POLAR_IDF_READY: {
                        if (lo->polar_cnt == lo->polar_cnt_max * 1)
                                lo->e_polar_idf = HFI_POLAR_IDF_POSITIVE;
                        break;
                }
                case HFI_POLAR_IDF_POSITIVE: {
                        out->id     = cfg->hfi_id;
                        lo->id_pos += ABS(lo->lpf_id);
                        if (lo->polar_cnt == lo->polar_cnt_max * 2)
                                lo->e_polar_idf = HFI_POLAR_IDF_NEGATIVE;
                        break;
                }
                case HFI_POLAR_IDF_NEGATIVE: {
                        out->id     = -cfg->hfi_id;
                        lo->id_neg += ABS(lo->lpf_id);
                        if (lo->polar_cnt == lo->polar_cnt_max * 3) {
                                lo->polar_offset = (ABS(lo->id_pos) > ABS(lo->id_neg)) ? 0.0f : PI;
                                lo->e_polar_idf  = HFI_POALR_IDF_FINISH;
                        }
                        break;
                }
                case HFI_POALR_IDF_FINISH: {
                        out->id       = 0.0f;
                        lo->polar_cnt = 0;
                        return;
                }
                default:
                        break;
        }

        lo->polar_cnt++;
}

HAPI void
hfi_exec(hfi_obs_t *hfi)
{
        DECL_PTRS(hfi, cfg, in, out, lo);

        DECL_PTR_RENAME(&lo->id_bpf, id_bpf);
        iir_exec_in(id_bpf, in->i_dq.d);
        lo->hfi_id = id_bpf->out.y * SIN(lo->hfi_theta);
        LOWPASS(lo->lpf_id, lo->hfi_id, cfg->id_lpf_fc, cfg->fs);

        // 极性辨识
        hfi_polar_idf(hfi);

        // 注入
        INTEGRATOR(lo->hfi_theta, TAU * cfg->fh, 1.0f, cfg->fs);
        WARP_TAU(lo->hfi_theta);
        out->vd = cfg->hfi_vd * COS(lo->hfi_theta);

        DECL_PTR_RENAME(&lo->iq_bpf, iq_bpf);
        iir_exec_in(iq_bpf, in->i_dq.q);
        lo->hfi_iq = iq_bpf->out.y * SIN(lo->hfi_theta);
        LOWPASS(lo->hfi_theta_err, lo->hfi_iq, cfg->iq_lpf_fc, cfg->fs);

        // PLL
        DECL_PTR_RENAME(&lo->pll, pll);
        pll_exec_theta_err_in(pll, lo->hfi_theta_err);
        out->est_theta = pll->out.theta + lo->polar_offset;
        WARP_TAU(out->est_theta);
        out->est_omega = pll->out.omega;
}

HAPI void
hfi_exec_in(hfi_obs_t *hfi, f32_dq_t i_dq)
{
        DECL_PTRS(hfi, in);

        in->i_dq = i_dq;
        hfi_exec(hfi);
}

#endif // !HFI_H
