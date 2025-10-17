#ifndef SINE_H
#define SINE_H

#include "util/def.h"
#include "util/util.h"

typedef struct {
        f32 fs;
        f32 wave_freq;
        f32 amp;
        f32 phase;
        f32 offset;
} sine_cfg_t;

typedef struct {
        f32 val;
} sine_out_t;

typedef struct {
        f32 phase_incr;
} sine_lo_t;

typedef struct {
        sine_cfg_t cfg;
        sine_out_t out;
        sine_lo_t  lo;
} sine_t;

static inline void
sine_init(sine_t *sine, sine_cfg_t sine_cfg)
{
        DECL_PTRS(sine, cfg);

        *cfg = sine_cfg;
}

static inline void
sine_exec(sine_t *sine)
{
        DECL_PTRS(sine, cfg, out, lo);

        lo->phase_incr = TAU * cfg->wave_freq / cfg->fs;
        out->val       = cfg->amp * SIN(cfg->phase) + cfg->offset;
        cfg->phase += lo->phase_incr;
        WARP_TAU(cfg->phase);
}

#endif // !SINE_H
