#ifndef FOCCTL_H
#define FOCCTL_H

#include "focdef.h"

static inline void foc_vol_ctl(foc_t *foc) {
  DECL_FOC_PTRS(foc);
}

static inline void foc_cur_ctl(foc_t *foc) {
  DECL_FOC_PTRS(foc);

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
}

static inline void foc_vel_ctl(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  pid_exec_in(&lo->vel_pid, lo->ref_pvct.vel, lo->fdb_pvct.vel, lo->ref_pvct.ffd_cur);
  lo->ref_i_dq.q = lo->vel_pid.out.val;
}

static inline void foc_pos_ctl(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  pid_exec_in(&lo->pos_pid, lo->ref_pvct.pos, lo->fdb_pvct.pos, lo->ref_pvct.ffd_vel);
  lo->ref_pvct.vel = lo->pos_pid.out.val;
}

static inline void foc_pd_ctl(foc_t *foc) {
  DECL_FOC_PTRS(foc);

  lo->ref_i_dq.q = lo->pd_pid.cfg.kp * (lo->ref_pvct.pos - lo->fdb_pvct.pos) +
                   lo->pd_pid.cfg.kd * (lo->ref_pvct.vel - lo->fdb_pvct.vel) + lo->ref_pvct.tor;
}

#endif // !FOCCTL_H
