#ifndef PID_H
#define PID_H

#ifdef __cpluscplus
extern "C" {
#endif

#include "../util/util.h"

typedef struct {
  fp32 fs;
  fp32 kp;
  fp32 ki;
  fp32 kd;
  fp32 ki_out_max;
  fp32 out_max;
} pid_cfg_t;

typedef struct {
  fp32 ref;
  fp32 ffd;
  fp32 fdb;
} pid_in_t;

typedef struct {
  fp32 val;
} pid_out_t;

typedef struct {
  fp32 err;
  fp32 prev_err;
  fp32 kp_out;
  fp32 ki_out;
  fp32 kd_out;
} pid_lo_t;

typedef struct {
  pid_cfg_t cfg;
  pid_in_t  in;
  pid_out_t out;
  pid_lo_t  lo;
} pid_ctl_t;

#define DECL_PID_PTRS(pid)                                                                         \
  pid_ctl_t *p   = (pid);                                                                          \
  pid_cfg_t *cfg = &p->cfg;                                                                        \
  pid_in_t  *in  = &p->in;                                                                         \
  pid_out_t *out = &p->out;                                                                        \
  pid_lo_t  *lo  = &p->lo;                                                                         \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_PID_PTRS_PREFIX(pid, prefix)                                                          \
  pid_ctl_t *prefix##_p   = (pid);                                                                 \
  pid_cfg_t *prefix##_cfg = &prefix##_p->cfg;                                                      \
  pid_in_t  *prefix##_in  = &prefix##_p->in;                                                       \
  pid_out_t *prefix##_out = &prefix##_p->out;                                                      \
  pid_lo_t  *prefix##_lo  = &prefix##_p->lo;                                                       \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static void pid_init(pid_ctl_t *pid, pid_cfg_t pid_cfg) {
  DECL_PID_PTRS(pid);

  *cfg = pid_cfg;
}

static void pid_exec(pid_ctl_t *pid) {
  DECL_PID_PTRS(pid);

  lo->err = in->ref - in->fdb;

  lo->kp_out = cfg->kp * lo->err;
  INTEGRATOR(lo->ki_out, lo->err, cfg->ki, cfg->fs);
  CLAMP(lo->ki_out, -cfg->ki_out_max, cfg->ki_out_max);
  DERIVATIVE(lo->kd_out, lo->err, lo->prev_err, cfg->kd, cfg->fs);

  out->val = lo->kp_out + lo->ki_out + lo->kd_out + in->ffd;
  CLAMP(out->val, -cfg->out_max, cfg->out_max);
}

static void pid_exec_in(pid_ctl_t *pid, fp32 ref, fp32 fdb, fp32 ffd) {
  DECL_PID_PTRS(pid);

  in->ref = ref;
  in->fdb = fdb;
  in->ffd = ffd;
  pid_exec(p);
}

#ifdef __cpluscplus
}
#endif

#endif // !PID_H
