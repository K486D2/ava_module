#ifndef PID_H
#define PID_H

#include "util/util.h"

typedef struct
{
        f32 fs;
        f32 kp, ki, kd;
        f32 ki_out_max, out_max;
} pid_cfg_t;

typedef struct
{
        f32 ref;
        f32 fdb;
        f32 ffd;
} pid_in_t;

typedef struct
{
        f32 val;
} pid_out_t;

typedef struct
{
        f32 err;
        f32 prev_err;
        f32 kp_out, ki_out, kd_out;
} pid_lo_t;

typedef struct
{
        pid_cfg_t cfg;
        pid_in_t  in;
        pid_out_t out;
        pid_lo_t  lo;
} pid_ctl_t;

static inline void pid_init(pid_ctl_t *pid, pid_cfg_t pid_cfg)
{
        DECL_PTRS(pid, cfg);

        *cfg = pid_cfg;
}

static inline void pid_exec(pid_ctl_t *pid)
{
        DECL_PTRS(pid, cfg, in, out, lo);

        lo->err = in->ref - in->fdb;

        lo->kp_out = cfg->kp * lo->err;
        INTEGRATOR(lo->ki_out, lo->err, cfg->ki, cfg->fs);
        CLAMP(lo->ki_out, -cfg->ki_out_max, cfg->ki_out_max);
        DERIVATIVE(lo->kd_out, lo->err, lo->prev_err, cfg->kd, cfg->fs);

        out->val = lo->kp_out + lo->ki_out + lo->kd_out + in->ffd;
        CLAMP(out->val, -cfg->out_max, cfg->out_max);
}

static inline void pid_exec_in(pid_ctl_t *pid, f32 ref, f32 fdb, f32 ffd)
{
        DECL_PTRS(pid, in);

        in->ref = ref;
        in->fdb = fdb;
        in->ffd = ffd;
        pid_exec(pid);
}

#endif // !PID_H
