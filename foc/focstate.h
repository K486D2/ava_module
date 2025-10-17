#ifndef FOCSTATE_H
#define FOCSTATE_H

#include "focctl.h"
#include "focobs.h"

static inline void
foc_svpwm(foc_t *foc)
{
        DECL_PTRS(foc, cfg, out);

        out->f32_v_uvw = inv_clarke(out->v_ab_sv);

        if (out->f32_v_uvw.u > out->f32_v_uvw.v) {
                out->svpwm.v_max = out->f32_v_uvw.u;
                out->svpwm.v_min = out->f32_v_uvw.v;
        } else {
                out->svpwm.v_max = out->f32_v_uvw.v;
                out->svpwm.v_min = out->f32_v_uvw.u;
        }
        if (out->f32_v_uvw.w < out->svpwm.v_min)
                out->svpwm.v_min = out->f32_v_uvw.w;
        else if (out->f32_v_uvw.w > out->svpwm.v_max)
                out->svpwm.v_max = out->f32_v_uvw.w;

        out->svpwm.v_avg = 0.5f * (out->svpwm.v_max + out->svpwm.v_min);
        UVW_ADD(out->svpwm.f32_pwm_duty, out->f32_v_uvw, -out->svpwm.v_avg);

        UVW_ADD_IP(out->svpwm.f32_pwm_duty, 0.5f);
        UVW_CLAMP(
            out->svpwm.f32_pwm_duty, cfg->periph_cfg.f32_pwm_min, cfg->periph_cfg.f32_pwm_max);
        UVW_MUL(out->svpwm.u32_pwm_duty, out->svpwm.f32_pwm_duty, cfg->periph_cfg.pwm_full_cnt);
}

static inline void
foc_select_theta(foc_t *foc)
{
        DECL_PTRS(foc, in, lo);

        switch (lo->e_theta) {
                case FOC_THETA_FORCE: {
                        in->rotor.theta = in->rotor.force_theta;
                        in->rotor.omega = in->rotor.force_omega;
                        break;
                }
                case FOC_THETA_SENSOR: {
                        in->rotor.theta = in->rotor.sensor_theta;
                        in->rotor.omega = in->rotor.sensor_omega;
                        break;
                }
                case FOC_THETA_SENSORLESS: {
                        in->rotor.theta = in->rotor.obs_theta;
                        in->rotor.omega = in->rotor.obs_omega;
                        break;
                }
                case FOC_THETA_SENSORFUSION:
                        break;
                default:
                        break;
        }
}

static inline void
foc_ready(foc_t *foc)
{
        ARG_UNUSED(foc);
}

static inline void
foc_disable(foc_t *foc)
{
        DECL_PTRS(foc, ops, in);

        ops->f_set_drv(false);

        memset(&in->i_ab, 0, sizeof(in->i_ab));
        memset(&in->i_dq, 0, sizeof(in->i_dq));
        memset(&in->f32_i_uvw, 0, sizeof(in->f32_i_uvw));

        RESET_OUT(foc);
        RESET_OUT(&foc->lo.id_pid);
        RESET_OUT(&foc->lo.iq_pid);
        RESET_OUT(&foc->lo.smo);
        RESET_OUT(&foc->lo.hfi);
}

static inline void
foc_enable(foc_t *foc)
{
        DECL_PTRS(foc, cfg, ops, in, out, lo);

        ops->f_set_drv(true);

        // ADC采样
        in->adc_raw = ops->f_get_adc();
        UVW_SUB_VEC_IP(in->adc_raw.i32_i_uvw, cfg->adc_offset.i32_i_uvw);
        UVW_MUL(in->f32_i_uvw, in->adc_raw.i32_i_uvw, cfg->periph_cfg.adc2cur);
        in->v_bus = in->adc_raw.i32_v_bus * cfg->periph_cfg.adc2vbus;

        // 克拉克变换
        in->i_ab = clarke(in->f32_i_uvw, cfg->periph_cfg.mi);

        // 无感观测器(iab)
        foc_obs_i_ab(foc);

        // 电角度源选择
        foc_select_theta(foc);

        // 帕克变换
        in->i_dq = park(in->i_ab, in->rotor.theta);

        // 电磁力矩计算
        lo->fdb_pvct.elec_tor = CPYSGN(poly_eval(cfg->motor_cfg.cur2tor,
                                                 ARRAY_SIZE(cfg->motor_cfg.cur2tor) - 1,
                                                 ABS(in->i_dq.q)),
                                       in->i_dq.q);

        // 无感观测器(idq)
        foc_obs_i_dq(foc);

        // 观测器电角度和传感器电角度误差计算
        in->rotor.fusion_theta_err = in->rotor.sensor_theta - in->rotor.obs_theta;
        WARP_PI(in->rotor.fusion_theta_err);

        foc_select_mode(foc);

        switch (lo->e_mode) {
                case FOC_MODE_VOL: {
                        foc_vol_ctl(foc);
                        break;
                }
                case FOC_MODE_CUR: {
                        foc_cur_ctl(foc);
                        foc_vol_ctl(foc);
                        break;
                }
                case FOC_MODE_VEL: {
                        foc_vel_ctl(foc);
                        foc_cur_ctl(foc);
                        foc_vol_ctl(foc);
                        break;
                }
                case FOC_MODE_POS: {
                        foc_pos_ctl(foc);
                        foc_vel_ctl(foc);
                        foc_cur_ctl(foc);
                        foc_vol_ctl(foc);
                        break;
                }
                case FOC_MODE_PD: {
                        foc_pd_ctl(foc);
                        foc_cur_ctl(foc);
                        foc_vol_ctl(foc);
                        break;
                }
                case FOC_MODE_ASC: {
                        foc_cur_ctl(foc);
                        foc_vol_ctl(foc);
                        break;
                }
                default:
                        break;
        }

        // 无感观测器(vdq)
        foc_obs_v_dq(foc);

        // 电角度补偿
        in->rotor.comp_theta = cfg->theta_comp_gain * in->rotor.omega / cfg->exec_freq;

        // 反帕克变换
        out->v_ab = inv_park(out->v_dq, in->rotor.theta + in->rotor.comp_theta);

        // 标幺
        out->v_ab_sv.a = out->v_ab.a / in->v_bus;
        out->v_ab_sv.b = out->v_ab.b / in->v_bus;

        // 调制发波
        foc_svpwm(foc);
        ops->f_set_pwm(cfg->periph_cfg.pwm_full_cnt, out->svpwm.u32_pwm_duty);
}

#endif // !FOCSTATE_H
