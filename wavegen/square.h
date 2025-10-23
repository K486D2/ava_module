#ifndef SQUARE_H
#define SQUARE_H

#include "util/def.h"
#include "util/util.h"

typedef struct {
        f32 fs;
        f32 wave_freq;  // 方波频率 (Hz)
        f32 amp;        // 方波幅度
        f32 phase;      // 当前相位 (rad)
        f32 off;        // 直流偏移量
        f32 duty_cycle; // 占空比 (0~1)
} square_cfg_t;

typedef struct {
        f32 val;
} square_out_t;

typedef struct {
        f32 phase_incr;
} square_lo_t;

typedef struct {
        square_cfg_t cfg;
        square_out_t out;
        square_lo_t  lo;
} square_t;

HAPI void
square_init(square_t *square, square_cfg_t square_cfg)
{
        DECL_PTRS(square, cfg);

        *cfg = square_cfg;
}

HAPI void
square_exec(square_t *square)
{
        DECL_PTRS(square, cfg, out, lo);

        lo->phase_incr = TAU * cfg->wave_freq / cfg->fs;

        if (cfg->phase < (cfg->duty_cycle * TAU))
                out->val = cfg->amp + cfg->off;
        else
                out->val = -cfg->amp + cfg->off;

        cfg->phase += lo->phase_incr;
        WARP_TAU(cfg->phase);
}

#endif // !SQUARE_H
