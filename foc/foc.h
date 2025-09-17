#ifndef FOC_H
#define FOC_H

#include "focdef.h"
#include "focstate.h"

static void foc_init(foc_t *foc, foc_cfg_t foc_cfg) {
  DECL_FOC_PTRS(foc);

  *cfg = foc_cfg;

  cfg->periph_cfg.adc2cur      = cfg->periph_cfg.cur_range / cfg->periph_cfg.adc_full_cnt;
  cfg->periph_cfg.adc2vbus     = cfg->periph_cfg.vbus_range / cfg->periph_cfg.adc_full_cnt;
  cfg->periph_cfg.pwm_full_cnt = cfg->periph_cfg.timer_freq / cfg->periph_cfg.pwm_freq;

  lo->pll.cfg.fs = cfg->exec_freq;

  lo->smo.cfg.fs = lo->smo.lo.pll.cfg.fs = cfg->exec_freq;
  lo->smo.cfg.motor_cfg                  = cfg->motor_cfg;

  lo->hfi.cfg.fs = lo->hfi.lo.pll.cfg.fs = cfg->exec_freq;

  pid_cfg_t cur_pid_cfg = {
      .fs = cfg->exec_freq,
      .kp = cfg->motor_cfg.wc * cfg->motor_cfg.ld,
      .ki = cfg->motor_cfg.wc * cfg->motor_cfg.rs,
  };

  pid_init(&lo->id_pid, cur_pid_cfg);
  pid_init(&lo->iq_pid, cur_pid_cfg);

  pid_init(&lo->pd_pid, cfg->pd_cfg);
  pid_init(&lo->vel_pid, cfg->vel_cfg);
  pid_init(&lo->pos_pid, cfg->pos_cfg);

  pll_init(&lo->pll, lo->pll.cfg);
  smo_init(&lo->smo, lo->smo.cfg);
  hfi_init(&lo->hfi, lo->hfi.cfg);
}

static void foc_exec(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  lo->exec_cnt++;

  // 机械角度获取与圈数计算
  in->rotor.mech_theta = ops->f_get_theta();
  if (in->rotor.mech_theta - in->rotor.mech_prev_theta < -TAU * 0.5f)
    in->rotor.mech_cycle_cnt++;
  else if (in->rotor.mech_theta - in->rotor.mech_prev_theta > TAU * 0.5f)
    in->rotor.mech_cycle_cnt--;
  in->rotor.mech_total_theta = (f32)in->rotor.mech_cycle_cnt * TAU + in->rotor.mech_theta;
  in->rotor.mech_prev_theta  = in->rotor.mech_theta;

  // 电角度计算
  in->rotor.sensor_theta =
      MECH_TO_ELEC(in->rotor.mech_theta, cfg->motor_cfg.npp) - cfg->theta_offset;
  in->rotor.sensor_comp_theta =
      cfg->sensor_theta_comp_gain * in->rotor.sensor_omega / cfg->exec_freq;
  in->rotor.sensor_theta += in->rotor.sensor_comp_theta;
  WARP_TAU(in->rotor.sensor_theta);

  // 电角速度计算
  DECL_PLL_PTR_RENAME(&lo->pll, pll)
  pll_exec_theta_in(pll, in->rotor.sensor_theta);
  in->rotor.sensor_omega = pll->out.lpf_omega;

  // 机械角速度计算
  in->rotor.mech_omega = ELEC_TO_MECH(in->rotor.sensor_omega, cfg->motor_cfg.npp);

  if (lo->e_theta == FOC_THETA_SENSOR) {
    in->rotor.theta = in->rotor.sensor_theta;
    in->rotor.omega = in->rotor.sensor_omega;
  }

  // FOC状态切换
  switch (lo->e_state) {
  case FOC_STATE_CALI:
    foc_cali(foc);
    break;
  case FOC_STATE_READY:
    foc_ready(foc);
    break;
  case FOC_STATE_DISABLE:
    foc_disable(foc);
    break;
  case FOC_STATE_ENABLE:
    foc_enable(foc);
    break;
  default:
    break;
  }

  lo->fdb_pvct.pos = in->rotor.mech_total_theta;
  lo->fdb_pvct.vel = in->rotor.mech_omega;
  lo->fdb_pvct.cur = in->i_dq.q;
}

#endif // !FOC_H
