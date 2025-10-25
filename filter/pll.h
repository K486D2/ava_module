#ifndef PLL_H
#define PLL_H

#include "../util/marcodef.h"
#include "../util/mathdef.h"
#include "../util/util.h"

typedef struct {
        f32 fs;
        f32 wc;
        f32 damp;
        f32 lpf_fc, ffd_lpf_fc;
        f32 kp;
        f32 ki;
} pll_cfg_t;

typedef struct {
        f32_ab_t ab;
        f32      theta;
} pll_in_t;

typedef struct {
        f32 theta;
        f32 omega, lpf_omega;
} pll_out_t;

typedef struct {
        pll_cfg_t cfg;
        f32       ki_out;
        f32       prev_theta;
        f32       theta_err;
        f32       ffd_omega, lpf_ffd_omega;
} pll_lo_t;

typedef struct {
        pll_cfg_t cfg;
        pll_in_t  in;
        pll_out_t out;
        pll_lo_t  lo;
} pll_filter_t;

HAPI void
pll_init(pll_filter_t *pll, pll_cfg_t pll_cfg)
{
        DECL_PTRS(pll, cfg);

        *cfg = pll_cfg;

        cfg->kp         = 2.0f * cfg->wc * cfg->damp;
        cfg->ki         = SQ(cfg->wc);
        cfg->ffd_lpf_fc = 0.5f * cfg->lpf_fc;
}

HAPI void
pll_exec(pll_filter_t *pll)
{
        CFG_CHECK(pll, pll_init);
        DECL_PTRS(pll, cfg, in, out, lo);

        // LF环路滤波器
        INTEGRATOR(lo->ki_out, lo->theta_err, cfg->ki, cfg->fs);
        out->omega = cfg->kp * lo->theta_err + lo->ki_out;
        LOWPASS(out->lpf_omega, out->omega, cfg->lpf_fc, cfg->fs);

        // VCO压控振荡器
        INTEGRATOR(out->theta, out->omega, 1.0f, cfg->fs);
        WARP_TAU(out->theta);
}

HAPI void
pll_exec_ab_in(pll_filter_t *pll, f32_ab_t ab)
{
        DECL_PTRS(pll, in, out, lo);

        in->ab = ab;

        // PD鉴相器
        lo->theta_err = in->ab.b * COS(out->theta) - in->ab.a * SIN(out->theta);

        pll_exec(pll);
}

HAPI void
pll_exec_theta_err_in(pll_filter_t *pll, f32 theta_err)
{
        DECL_PTRS(pll, cfg, in, out, lo);

        // PD鉴相器
        lo->theta_err = theta_err;
        WARP_PI(lo->theta_err);

        pll_exec(pll);
}

HAPI void
pll_exec_theta_in(pll_filter_t *pll, f32 theta)
{
        DECL_PTRS(pll, cfg, in, out, lo);

        in->theta = theta;

        // 前馈速度计算
        THETA_DERIVATIVE(lo->ffd_omega, in->theta, lo->prev_theta, 1.0f, cfg->fs);
        LOWPASS(lo->lpf_ffd_omega, lo->ffd_omega, cfg->ffd_lpf_fc, cfg->fs);

        // PD鉴相器
        lo->theta_err = in->theta - out->theta;
        WARP_PI(lo->theta_err);

        pll_exec(pll);
}

#endif // !PLL_H
