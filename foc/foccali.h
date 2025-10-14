#ifndef FOCCALI_H
#define FOCCALI_H

#include "focstate.h"

static inline int foc_adc_cali(foc_t *foc) {
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

static inline void foc_cali(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_cali) {
  case FOC_CALI_INIT: {
    if (foc_adc_cali(foc) < 0)
      break;
    lo->ref_i_dq.d        = cfg->ref_theta_cali_id;
    in->rotor.force_omega = cfg->ref_theta_cali_omega;
    lo->e_mode            = FOC_MODE_CUR;
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
    lo->e_mode                            = FOC_MODE_NULL;
    lo->e_theta                           = FOC_THETA_NULL;
    lo->e_state                           = FOC_STATE_READY;
  } break;
  default: {
    lo->e_state = FOC_STATE_DISABLE;
  } break;
  }
}

#endif // !FOCCALI_H
