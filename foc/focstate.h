#ifndef FOCSTATE_H
#define FOCSTATE_H

#include "focdef.h"

static inline  void foc_obs_i_ab(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_obs) {
  case FOC_OBS_SMO: {
    DECL_SMO_PTR_RENAME(&lo->smo, smo);
    smo_exec_in(smo, in->i_ab, out->v_ab);
    in->rotor.obs_theta = smo->out.theta;
    in->rotor.obs_omega = smo->out.omega;
  } break;
  default:
    break;
  }
}

static inline  void foc_obs_i_dq(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_obs) {
  case FOC_OBS_HFI: {
    DECL_HFI_PTR_RENAME(&lo->hfi, hfi);
    hfi_exec_in(hfi, in->i_dq);
    in->rotor.obs_theta = hfi->out.theta;
    in->rotor.obs_omega = hfi->out.omega;
    lo->ref_i_dq.d      = hfi->out.id;
  } break;
  default:
    break;
  }
}

static inline  void foc_obs_v_dq(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_obs) {
  case FOC_OBS_HFI: {
    DECL_HFI_PTR_RENAME(&lo->hfi, hfi)
    out->v_dq.d += hfi->out.vd;
  } break;
  default:
    break;
  }
}

static inline  void foc_svpwm(foc_t *foc) {
  DECL_FOC_PTRS(foc);

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
  UVW_CLAMP(out->svpwm.f32_pwm_duty, cfg->periph_cfg.f32_pwm_min, cfg->periph_cfg.f32_pwm_max);
  UVW_MUL(out->svpwm.u32_pwm_duty, out->svpwm.f32_pwm_duty, cfg->periph_cfg.pwm_full_cnt);
}

static inline  void foc_select_theta(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_theta) {
  case FOC_THETA_FORCE:
    in->rotor.theta = in->rotor.force_theta;
    in->rotor.omega = in->rotor.force_omega;
    break;
  case FOC_THETA_SENSOR:
    in->rotor.theta = in->rotor.sensor_theta;
    in->rotor.omega = in->rotor.sensor_omega;
    break;
  case FOC_THETA_SENSORLESS:
    in->rotor.theta = in->rotor.obs_theta;
    in->rotor.omega = in->rotor.obs_omega;
    break;
  case FOC_THETA_SENSORFUSION:
    break;
  default:
    break;
  }
}

static inline  void foc_ready(foc_t *foc) {
  DECL_FOC_PTRS(foc);
}

static inline  void foc_disable(foc_t *foc) {
  DECL_FOC_PTRS(foc);

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

static inline  void foc_vel_ctl(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  pid_exec_in(&lo->vel_pid, lo->ref_pvct.vel, lo->fdb_pvct.vel, lo->ref_pvct.ffd_cur);
  lo->ref_i_dq.q = lo->vel_pid.out.val;
}

static inline  void foc_pos_ctl(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  pid_exec_in(&lo->pos_pid, lo->ref_pvct.pos, lo->fdb_pvct.pos, lo->ref_pvct.ffd_vel);
  lo->ref_pvct.vel = lo->pos_pid.out.val;

  foc_vel_ctl(foc);
}

static inline  void foc_pd_ctl(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  lo->ref_i_dq.q = lo->pd_pid.cfg.kp * (lo->ref_pvct.pos - lo->fdb_pvct.pos) +
                   lo->pd_pid.cfg.kd * (lo->ref_pvct.vel - lo->fdb_pvct.vel) + lo->ref_pvct.tor;
}

static inline  void foc_enable(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  ops->f_set_drv(true);

  // ADC采样
  in->adc_raw = ops->f_get_adc();
  UVW_SUB_VEC_IP(in->adc_raw.i32_i_uvw, cfg->adc_offset.i32_i_uvw);
  UVW_MUL(in->f32_i_uvw, in->adc_raw.i32_i_uvw, cfg->periph_cfg.adc2cur);
  in->v_bus = in->adc_raw.i32_v_bus * cfg->periph_cfg.adc2vbus;

  // 克拉克变换
  in->i_ab = clarke(in->f32_i_uvw, cfg->periph_cfg.mi);

  // 无感观测器(alpha-beta)
  foc_obs_i_ab(foc);

  // 电角度源选择
  foc_select_theta(foc);

  // 帕克变换
  in->i_dq = park(in->i_ab, in->rotor.theta);

  // 无感观测器(d-q)
  foc_obs_i_dq(foc);

  // 观测器电角度和传感器电角度误差计算
  in->rotor.fusion_theta_err = in->rotor.sensor_theta - in->rotor.obs_theta;
  WARP_PI(in->rotor.fusion_theta_err);

  switch (lo->e_mode) {
  case FOC_MODE_CUR: {
    lo->ref_i_dq.q = lo->ref_pvct.cur;
  } break;
  case FOC_MODE_PD: {
    foc_pd_ctl(foc);
  } break;
  case FOC_MODE_VEL: {
    foc_vel_ctl(foc);
  } break;
  case FOC_MODE_POS: {
    foc_pos_ctl(foc);
  } break;
  case FOC_MODE_ASC: {
  } break;
  default:
    break;
  }

  // Q轴电流环
  DECL_PID_PTR_RENAME(&lo->iq_pid, iq_pid);
  iq_pid->cfg.ki_out_max = iq_pid->cfg.out_max = in->v_bus / SQRT_3 * cfg->periph_cfg.f32_pwm_max;
  lo->ffd_v_dq.q                               = in->rotor.omega * cfg->motor_cfg.psi * 0.7f;
  pid_exec_in(iq_pid, lo->ref_i_dq.q, in->i_dq.q, lo->ffd_v_dq.q);
  out->v_dq.q = iq_pid->out.val;

  // D轴电流环
  DECL_PID_PTR_RENAME(&lo->id_pid, id_pid);
  id_pid->cfg.ki_out_max = id_pid->cfg.out_max = in->v_bus / SQRT_3 * cfg->periph_cfg.f32_pwm_max;
  lo->ffd_v_dq.d = -in->rotor.omega * cfg->motor_cfg.lq * in->i_dq.q * 0.7f;
  pid_exec_in(id_pid, lo->ref_i_dq.d, in->i_dq.d, lo->ffd_v_dq.d);
  out->v_dq.d = id_pid->out.val;

  // 高频注入
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

static inline  int foc_adc_cali(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  ops->f_set_drv(true);
  in->adc_raw = ops->f_get_adc();

  UVW_ADD_VEC_IP(cfg->adc_offset.i32_i_uvw, in->adc_raw.i32_i_uvw);
  if (++lo->adc_cali_cnt >= LF(cfg->periph_cfg.adc_cali_cnt_max)) {
    cfg->adc_offset.i32_i_uvw.u >>= cfg->periph_cfg.adc_cali_cnt_max;
    cfg->adc_offset.i32_i_uvw.v >>= cfg->periph_cfg.adc_cali_cnt_max;
    cfg->adc_offset.i32_i_uvw.w >>= cfg->periph_cfg.adc_cali_cnt_max;
    ops->f_set_drv(false);
    return 0;
  }
  return -MEBUSY;
}

static inline  void foc_cali(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_cali) {
  case FOC_CALI_INIT: {
    if (foc_adc_cali(foc) < 0)
      break;
    lo->ref_i_dq.d        = cfg->ref_theta_cali_id;
    in->rotor.force_omega = cfg->ref_theta_cali_omega;
    lo->e_theta           = FOC_THETA_FORCE;
    lo->e_cali            = FOC_CALI_CW;
  } break;
  case FOC_CALI_CW: {
    foc_enable(foc);
    if (in->rotor.force_theta >= TAU) {
      in->rotor.force_theta = TAU;
      if (++lo->theta_cali_hold_cnt >= cfg->periph_cfg.theta_cali_cnt_max) {
        lo->theta_offset_sum += in->rotor.sensor_theta;
        lo->theta_cali_hold_cnt = 0;
        if (++lo->theta_cali_cnt >= cfg->motor_cfg.npp)
          lo->e_cali = FOC_CALI_CCW;
        else
          in->rotor.force_theta = 0.0f;
      }
    } else if (lo->theta_cali_hold_cnt == 0)
      in->rotor.force_theta += in->rotor.force_omega / cfg->exec_freq;
  } break;
  case FOC_CALI_CCW: {
    foc_enable(foc);
    if (in->rotor.force_theta <= 0.0f) {
      in->rotor.force_theta = 0.0f;
      if (++lo->theta_cali_hold_cnt >= cfg->periph_cfg.theta_cali_cnt_max) {
        lo->theta_offset_sum += in->rotor.sensor_theta;
        lo->theta_cali_hold_cnt = 0;
        if (++lo->theta_cali_cnt >= cfg->motor_cfg.npp * 2)
          lo->e_cali = FOC_CALI_FINISH;
        else
          in->rotor.force_theta = TAU;
      }
    } else if (lo->theta_cali_hold_cnt == 0)
      in->rotor.force_theta -= in->rotor.force_omega / cfg->exec_freq;
  } break;
  case FOC_CALI_FINISH: {
    foc_disable(foc);
    cfg->theta_offset = lo->theta_offset_sum / lo->theta_cali_cnt;
    lo->adc_cali_cnt = lo->theta_cali_cnt = 0;
    lo->ref_i_dq.d                        = 0.0f;
    in->rotor.force_theta                 = 0.0f;
    in->rotor.force_omega                 = 0.0f;
    lo->e_theta                           = FOC_THETA_NULL;
    lo->e_state                           = FOC_STATE_READY;
  } break;
  default: {
    lo->e_state = FOC_STATE_DISABLE;
  } break;
  }
}

#endif // !FOCSTATE_H
