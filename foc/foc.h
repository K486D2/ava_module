#ifndef FOC_H
#define FOC_H

#include "../controller/controller.h"
#include "../filter/filter.h"
#include "../observer/observer.h"
#include "../transform/transform.h"
#include "../util/util.h"

typedef struct {
  i32_uvw_t i32_i_uvw, i32_v_uvw;
  i32       i32_v_bus;
} adc_raw_t;

typedef struct {
  f32       v_max, v_min, v_avg;
  f32_uvw_t f32_pwm_duty;
  u32_uvw_t u32_pwm_duty;
} svpwm_t;

typedef struct {
  // 电气角度
  f32 theta, comp_theta, omega;
  f32 force_theta, force_omega;
  f32 sensor_theta, sensor_comp_theta, sensor_omega;
  f32 obs_theta, obs_omega;
  f32 fusion_theta_err;
  // 机械角度
  i32 mech_cycle_cnt;
  f32 mech_theta, mech_prev_theta, mech_total_theta;
} rotor_t;

typedef struct {
  f32          exec_freq;
  bool         is_adc_cail, is_theta_cail;
  adc_raw_t    adc_offset;
  f32          theta_offset;
  f32          sensor_theta_comp_gain, theta_comp_gain;
  motor_cfg_t  motor_cfg;
  periph_cfg_t periph_cfg;
} foc_cfg_t;

typedef struct {
  adc_raw_t adc_raw;
  f32       v_bus;
  rotor_t   rotor;
  f32_uvw_t f32_i_uvw;
  f32_ab_t  i_ab;
  f32_dq_t  i_dq;
} foc_in_t;

typedef struct {
  f32_dq_t  v_dq;
  f32_ab_t  v_ab;
  f32_ab_t  v_ab_sv;
  f32_uvw_t f32_v_uvw;
  svpwm_t   svpwm;
} foc_out_t;

typedef enum {
  FOC_STATE_CALI,
  FOC_STATE_READY,
  FOC_STATE_DISABLE,
  FOC_STATE_ENABLE,
} foc_state_e;

typedef enum {
  FOC_THETA_NULL,
  FOC_THETA_FORCE,
  FOC_THETA_SENSOR,
  FOC_THETA_SENSORLESS,
  FOC_THETA_SENSORFUSION,
} foc_theta_e;

typedef enum {
  FOC_OBS_NULL,
  FOC_OBS_SMO,
  FOC_OBS_HFI,
} foc_obs_e;

typedef struct {
  u32 NULL_FUNC_PTR : 1;
} foc_fault_t;

typedef struct {
  f32 elapsed_us;
  u32 exec_cnt;
  u32 adc_cail_cnt;

  f32_dq_t ref_i_dq;
  f32_dq_t ffd_v_dq;

  foc_fault_t fault;

  foc_state_e e_state;
  foc_theta_e e_theta;
  foc_obs_e   e_obs;

  pid_ctl_t    id_pid, iq_pid;
  pll_filter_t pll;
  smo_obs_t    smo;
  hfi_obs_t    hfi;
} foc_lo_t;

typedef adc_raw_t (*foc_get_adc_f)(void);
typedef f32 (*foc_get_theta_f)(void);
typedef void (*foc_set_pwm_f)(u32 pwm_cnt_max, u32_uvw_t u32_pwm_duty);
typedef void (*foc_set_drv_f)(bool enable);

typedef struct {
  foc_get_adc_f   f_get_adc;
  foc_get_theta_f f_get_theta;
  foc_set_pwm_f   f_set_pwm;
  foc_set_drv_f   f_set_drv;
} foc_ops_t;

typedef struct {
  foc_cfg_t cfg;
  foc_in_t  in;
  foc_out_t out;
  foc_lo_t  lo;
  foc_ops_t ops;
} foc_t;

#define DECL_FOC_PTRS(foc)                                                                         \
  foc_cfg_t *cfg = &(foc)->cfg;                                                                    \
  foc_in_t  *in  = &(foc)->in;                                                                     \
  foc_out_t *out = &(foc)->out;                                                                    \
  foc_lo_t  *lo  = &(foc)->lo;                                                                     \
  foc_ops_t *ops = &(foc)->ops;                                                                    \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);                                                                                  \
  ARG_UNUSED(ops);

#define DECL_FOC_PTR_RENAME(foc, name)                                                             \
  foc_t *(name) = (foc);                                                                           \
  ARG_UNUSED(name);

static inline void foc_obs_i_ab(foc_t *foc) {
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

static inline void foc_obs_i_dq(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_obs) {
  case FOC_OBS_HFI: {
    DECL_HFI_PTR_RENAME(&lo->hfi, hfi);
    hfi_exec_in(hfi, in->i_dq);
    in->rotor.obs_theta = hfi->out.theta;
    in->rotor.obs_omega = hfi->out.omega;
    lo->ref_i_dq.d      = hfi->out.id_in;
  } break;
  default:
    break;
  }
}

static inline void foc_obs_v_dq(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_obs) {
  case FOC_OBS_HFI: {
    DECL_HFI_PTR_RENAME(&lo->hfi, hfi)
    out->v_dq.d += hfi->out.vd_h;
  } break;
  default:
    break;
  }
}

static inline void foc_svpwm(foc_t *foc) {
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
  UVW_MUL(out->svpwm.u32_pwm_duty, out->svpwm.f32_pwm_duty, cfg->periph_cfg.pwm_cnt_max);
}

static inline void foc_select_theta(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  switch (lo->e_theta) {
  case FOC_THETA_NULL:
    break;
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

static inline void foc_cali(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  if (cfg->is_adc_cail)
    return;

  ops->f_set_drv(true);
  in->adc_raw = ops->f_get_adc();
  UVW_ADD_VEC_IP(cfg->adc_offset.i32_i_uvw, in->adc_raw.i32_i_uvw);
  if (++lo->adc_cail_cnt >= LF(cfg->periph_cfg.adc_cail_cnt_max)) {
    cfg->adc_offset.i32_i_uvw.u >>= cfg->periph_cfg.adc_cail_cnt_max;
    cfg->adc_offset.i32_i_uvw.v >>= cfg->periph_cfg.adc_cail_cnt_max;
    cfg->adc_offset.i32_i_uvw.w >>= cfg->periph_cfg.adc_cail_cnt_max;
    cfg->is_adc_cail = true;
    ops->f_set_drv(false);
    lo->e_state = FOC_STATE_READY;
  }
}

static inline void foc_ready(foc_t *foc) {
  DECL_FOC_PTRS(foc);
}

static inline void foc_disable(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  ops->f_set_drv(false);

  RESET_OUT(foc);
  RESET_OUT(&foc->lo.pll);
  RESET_OUT(&foc->lo.id_pid);
  RESET_OUT(&foc->lo.iq_pid);
  RESET_OUT(&foc->lo.smo);
  RESET_OUT(&foc->lo.hfi);
}

static inline void foc_enable(foc_t *foc) {
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
  ops->f_set_pwm(cfg->periph_cfg.pwm_cnt_max, out->svpwm.u32_pwm_duty);
}

static void foc_init(foc_t *foc, foc_cfg_t foc_cfg) {
  DECL_FOC_PTRS(foc);

  *cfg = foc_cfg;

  cfg->motor_cfg.ls           = 0.5f * (cfg->motor_cfg.ld + cfg->motor_cfg.lq);
  cfg->periph_cfg.adc2cur     = cfg->periph_cfg.cur_range / cfg->periph_cfg.adc_cnt_max;
  cfg->periph_cfg.adc2vbus    = cfg->periph_cfg.vbus_range / cfg->periph_cfg.adc_cnt_max;
  cfg->periph_cfg.pwm_cnt_max = cfg->periph_cfg.timer_freq / cfg->periph_cfg.pwm_freq;

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
}

#endif // !FOC_H
