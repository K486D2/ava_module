#ifndef FOC_H
#define FOC_H

#ifdef __cplusplus
extern "C" {
#endif

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
  f32_uvw_t fp32_pwm_duty;
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
} theta_t;

typedef struct {
  f32          exec_freq;
  f32          sensor_theta_comp_gain, theta_comp_gain;
  adc_raw_t    adc_offset;
  f32          theta_offset;
  bool         is_adc_cail;
  motor_cfg_t  motor_cfg;
  periph_cfg_t periph_cfg;
} foc_cfg_t;

typedef struct {
  adc_raw_t adc_raw;
  theta_t   theta;
  f32_uvw_t fp32_i_uvw, fp32_v_uvw;
  f32_ab_t  i_ab, v_ab;
  f32_dq_t  i_dq, v_dq;
  f32       v_bus;
} foc_in_t;

typedef struct {
  f32_uvw_t fp32_i_uvw, fp32_v_uvw;
  f32_ab_t  i_ab, v_ab;
  f32_ab_t  v_ab_sv;
  f32_dq_t  i_dq, v_dq;
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

typedef struct {
  u32 NULL_FUNC_PTR : 1;
} foc_fault_t;

typedef struct {
  u32         exec_cnt;
  u32         elapsed;
  f32         elapsed_us;
  foc_fault_t fault;
  u32         adc_cail_cnt;
  foc_state_e state;
  foc_theta_e theta;
  f32_dq_t    ffd_v_dq;

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
  foc_t     *p   = (foc);                                                                          \
  foc_cfg_t *cfg = &p->cfg;                                                                        \
  foc_in_t  *in  = &p->in;                                                                         \
  foc_out_t *out = &p->out;                                                                        \
  foc_lo_t  *lo  = &p->lo;                                                                         \
  foc_ops_t *ops = &p->ops;                                                                        \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);                                                                                  \
  ARG_UNUSED(ops);

#define DECL_FOC_PTRS_PREFIX(foc, prefix)                                                          \
  foc_t     *prefix##_p   = (foc);                                                                 \
  foc_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                      \
  foc_in_t  *prefix##_in  = &prefix##_p->in;                                                       \
  foc_out_t *prefix##_out = &prefix##_p->out;                                                      \
  foc_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                       \
  foc_ops_t *prefix##_ops = &prefix##_p->ops;                                                      \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);                                                                         \
  ARG_UNUSED(prefix##_ops);

static inline void foc_obs(foc_t *foc) {
  DECL_FOC_PTRS(foc);
  DECL_SMO_PTRS_PREFIX(&lo->smo, smo);

  // 观测器电角度计算
  smo_exec_in(smo_p, in->i_ab, out->v_ab);
  in->theta.obs_theta = smo_out->theta;
  in->theta.obs_omega = smo_out->omega;

  // 观测器电角度和传感器电角度误差计算
  in->theta.fusion_theta_err = in->theta.sensor_theta - in->theta.obs_theta;
  WARP_PI(in->theta.fusion_theta_err);
}

static inline void foc_svpwm(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  out->fp32_v_uvw = inv_clarke(out->v_ab_sv);

  if (out->fp32_v_uvw.u > out->fp32_v_uvw.v) {
    out->svpwm.v_max = out->fp32_v_uvw.u;
    out->svpwm.v_min = out->fp32_v_uvw.v;
  } else {
    out->svpwm.v_max = out->fp32_v_uvw.v;
    out->svpwm.v_min = out->fp32_v_uvw.u;
  }

  UPDATE_MIN_MAX(out->svpwm.v_min, out->svpwm.v_max, out->fp32_v_uvw.w);
  out->svpwm.v_avg = (out->svpwm.v_max + out->svpwm.v_min) * 0.5f;
  UVW_ADD(out->svpwm.fp32_pwm_duty, out->fp32_v_uvw, -out->svpwm.v_avg);

  UVW_ADD_IP(out->svpwm.fp32_pwm_duty, 0.5f);
  UVW_CLAMP(out->svpwm.fp32_pwm_duty, cfg->periph_cfg.fp32_pwm_min, cfg->periph_cfg.fp32_pwm_max);
  UVW_MUL(out->svpwm.u32_pwm_duty, out->svpwm.fp32_pwm_duty, cfg->periph_cfg.pwm_cnt_max);
}

static inline void foc_cali(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  if (cfg->is_adc_cail)
    return;

  in->adc_raw = ops->f_get_adc();
  UVW_ADD_VEC_IP(cfg->adc_offset.i32_i_uvw, in->adc_raw.i32_i_uvw);
  if (++lo->adc_cail_cnt >= LF(cfg->periph_cfg.adc_cail_cnt_max)) {
    SELF_RF(cfg->adc_offset.i32_i_uvw.u, cfg->periph_cfg.adc_cail_cnt_max);
    SELF_RF(cfg->adc_offset.i32_i_uvw.v, cfg->periph_cfg.adc_cail_cnt_max);
    SELF_RF(cfg->adc_offset.i32_i_uvw.w, cfg->periph_cfg.adc_cail_cnt_max);
    cfg->is_adc_cail = true;
    lo->state        = FOC_STATE_READY;
  }
}

static inline void foc_ready(foc_t *foc) {
  DECL_FOC_PTRS(foc);
}

static inline void foc_disable(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  ops->f_set_drv(false);

  RESET_OUT(p);
  RESET_OUT(&p->lo.pll);
  RESET_OUT(&p->lo.id_pid);
  RESET_OUT(&p->lo.iq_pid);
  RESET_OUT(&p->lo.smo);
  RESET_OUT(&p->lo.hfi);
}

static inline void foc_enable(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  ops->f_set_drv(true);
}

static void foc_init(foc_t *foc, foc_cfg_t foc_cfg) {
  DECL_FOC_PTRS(foc);

  *cfg = foc_cfg;

  pid_init(&lo->id_pid, lo->id_pid.cfg);
  pid_init(&lo->iq_pid, lo->iq_pid.cfg);
  pll_init(&lo->pll, lo->pll.cfg);
  smo_init(&lo->smo, lo->smo.cfg);
  hfi_init(&lo->hfi, lo->hfi.cfg);
}

static void foc_exec(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  lo->exec_cnt++;

  // ADC采样
  in->adc_raw = ops->f_get_adc();
  UVW_SUB_VEC_IP(in->adc_raw.i32_i_uvw, cfg->adc_offset.i32_i_uvw);
  UVW_MUL(in->fp32_i_uvw, in->adc_raw.i32_i_uvw, cfg->periph_cfg.adc2cur);
  in->v_bus = in->adc_raw.i32_v_bus * cfg->periph_cfg.adc2vbus;

  // 机械角度获取与圈数计算
  in->theta.mech_theta = ops->f_get_theta();
  if (in->theta.mech_theta - in->theta.mech_prev_theta < -TAU * 0.5f)
    in->theta.mech_cycle_cnt++;
  else if (in->theta.mech_theta - in->theta.mech_prev_theta > TAU * 0.5f)
    in->theta.mech_cycle_cnt--;
  in->theta.mech_total_theta = (f32)in->theta.mech_cycle_cnt * TAU + in->theta.mech_theta;
  in->theta.mech_prev_theta  = in->theta.mech_theta;

  // 电角度计算
  in->theta.sensor_theta =
      MECH_TO_ELEC(in->theta.mech_theta, cfg->motor_cfg.npp) - cfg->theta_offset;
  in->theta.sensor_comp_theta =
      cfg->sensor_theta_comp_gain * in->theta.sensor_omega / cfg->exec_freq;
  in->theta.sensor_theta += in->theta.sensor_comp_theta;

  WARP_TAU(in->theta.sensor_theta);

  // 电角速度计算
  DECL_PLL_PTRS_PREFIX(&lo->theta_pll, theta_pll)
  pll_exec_theta_in(theta_pll_p, in->theta.sensor_theta);
  in->theta.sensor_omega = theta_pll_out->lpf_omega;

  // 电角度源选择
  switch (lo->theta) {
  case FOC_THETA_NULL:
    break;
  case FOC_THETA_FORCE:
    in->theta.theta = in->theta.force_theta;
    in->theta.omega = in->theta.force_omega;
    break;
  case FOC_THETA_SENSOR:
    in->theta.theta = in->theta.sensor_theta;
    in->theta.omega = in->theta.sensor_omega;
    break;
  case FOC_THETA_SENSORLESS:
    in->theta.theta = in->theta.obs_theta;
    in->theta.omega = in->theta.obs_omega;
    break;
  case FOC_THETA_SENSORFUSION:
    break;
  default:
    break;
  }

  // FOC状态机
  switch (lo->state) {
  case FOC_STATE_CALI:
    foc_cali(p);
    return;
  case FOC_STATE_READY:
    foc_ready(p);
    return;
  case FOC_STATE_DISABLE:
    foc_disable(p);
    return;
  case FOC_STATE_ENABLE:
    foc_enable(p);
    break;
  default:
    return;
  }

  /* --------------- only FOC_STATE_ENABLE can run below code!!! -------------- */

  // 坐标变换
  in->i_ab = clarke(in->fp32_i_uvw, cfg->periph_cfg.mi);
  foc_obs(p);
  in->i_dq = park(in->i_ab, in->theta.theta);

  // HFI
  DECL_HFI_PTRS_PREFIX(&lo->hfi, hfi)
  hfi_exec_in(hfi_p, in->i_ab);
  in->i_dq.d += hfi_out->id_in;

  // D轴电流环
  DECL_PID_PTRS_PREFIX(&lo->id_pid, id_pid);
  lo->ffd_v_dq.d = -in->theta.omega * cfg->motor_cfg.lq * in->i_dq.q * 0.7f;
  pid_exec_in(id_pid_p, out->i_dq.d, in->i_dq.d, lo->ffd_v_dq.d);
  out->v_dq.d = id_pid_out->val;

  // HFI
  out->v_dq.d += hfi_out->vd_h;
  in->theta.theta = hfi_out->obs_theta;

  // Q轴电流环
  DECL_PID_PTRS_PREFIX(&lo->iq_pid, iq_pid);
  lo->ffd_v_dq.q = in->theta.omega * cfg->motor_cfg.psi * 0.7f;
  pid_exec_in(iq_pid_p, out->i_dq.q, in->i_dq.q, lo->ffd_v_dq.q);
  out->v_dq.q = iq_pid_out->val;

  // 电角度补偿
  in->theta.comp_theta = cfg->theta_comp_gain * in->theta.omega / cfg->exec_freq;

  // 反帕克变换
  out->v_ab = inv_park(out->v_dq, in->theta.theta + in->theta.comp_theta);

  // 标幺
  out->v_ab_sv.a = out->v_ab.a / in->v_bus;
  out->v_ab_sv.b = out->v_ab.b / in->v_bus;

  // 调制发波
  foc_svpwm(p);
  ops->f_set_pwm(cfg->periph_cfg.pwm_cnt_max, out->svpwm.u32_pwm_duty);
}

#ifdef __cplusplus
}
#endif

#endif // !FOC_H
