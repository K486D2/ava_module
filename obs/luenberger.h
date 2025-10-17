#ifndef LUENBERGER_H
#define LUENBERGER_H

#include "filter/pll.h"
#include "util/util.h"

typedef struct
{
        f32         fs;
        motor_cfg_t motor;
        f32         wc;
        f32         damp;
} lbg_cfg_t;

typedef struct
{
        f32 theta;
        f32 elec_tor;
} lbg_in_t;

typedef struct
{
        f32 est_theta, est_omega;
        f32 est_load_tor;
        f32 sum_tor;
} lbg_out_t;

typedef struct
{
        lbg_cfg_t cfg;

        f32 g1;
        f32 kp, ki;

        f32 theta_err, mech_theta_err;
        f32 ki_out;
        f32 est_omega;
} lbg_lo_t;

typedef struct
{
        lbg_cfg_t cfg;
        lbg_in_t  in;
        lbg_out_t out;
        lbg_lo_t  lo;
} lbg_obs_t;

static void lbg_init(lbg_obs_t *lbg, lbg_cfg_t lbg_cfg)
{
        DECL_PTRS(lbg, cfg, lo);

        *cfg = lo->cfg = lbg_cfg;

        lo->g1 = 2.0f * cfg->wc;
        lo->kp = 2.0f * SQ(cfg->wc) * cfg->motor.j * cfg->damp;
        lo->ki = SQ(cfg->wc) * cfg->wc * cfg->motor.j;
}

static void lbg_exec(lbg_obs_t *lbg)
{
        CFG_CHECK(lbg, lbg_init);
        DECL_PTRS(lbg, cfg, in, out, lo);

        // 电角度
        lo->theta_err = in->theta - out->est_theta;
        WARP_PI(lo->theta_err);

        // 机械角度
        lo->mech_theta_err = lo->theta_err / (f32)cfg->motor.npp;

        // 负载力矩
        INTEGRATOR(lo->ki_out, lo->mech_theta_err, lo->ki, cfg->fs);
        CLAMP(lo->ki_out, -cfg->motor.max_tor, cfg->motor.max_tor);
        out->est_load_tor = -lo->ki_out;

        out->sum_tor = in->elec_tor + lo->kp * lo->mech_theta_err + lo->ki_out;

        INTEGRATOR(lo->est_omega, out->sum_tor, 1.0f / cfg->motor.j, cfg->fs);
        out->est_omega = lo->g1 * lo->mech_theta_err + lo->est_omega;

        INTEGRATOR(out->est_theta, out->est_omega, (f32)cfg->motor.npp, cfg->fs);
        WARP_TAU(out->est_theta);
}

static void lbg_exec_in(lbg_obs_t *lbg, f32 theta, f32 elec_tor)
{
        DECL_PTRS(lbg, in);

        in->theta    = theta;
        in->elec_tor = elec_tor;

        lbg_exec(lbg);
}

#endif // !LUENBERGER_H
