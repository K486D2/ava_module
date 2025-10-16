#ifndef MAF_H
#define MAF_H

#include "ds/spsc.h"
#include "util/util.h"

typedef struct {
        f32 *buf;
        u32  cap;
} maf_cfg_t;

typedef struct {
        f32 x;
} maf_in_t;

typedef struct {
        f32 y;
} maf_out_t;

typedef struct {
        spsc_t spsc;
        f64    x_sum;
} maf_lo_t;

typedef struct {
        maf_cfg_t cfg;
        maf_in_t  in;
        maf_out_t out;
        maf_lo_t  lo;
} maf_filter_t;

static inline void maf_init(maf_filter_t *maf, maf_cfg_t maf_cfg) {
        DECL_PTRS(maf, cfg, lo);

        *cfg = maf_cfg;
        spsc_init(&lo->spsc, cfg->buf, cfg->cap, SPSC_POLICY_REJECT);
}

static inline void maf_exec(maf_filter_t *maf) {
        DECL_PTRS(maf, cfg, in, lo);

        f32 prev_x;
        spsc_read(&lo->spsc, &prev_x, sizeof(prev_x));
        lo->x_sum -= prev_x;

        spsc_write(&lo->spsc, &in->x, sizeof(in->x));

        lo->x_sum += in->x;
        lo->x_sum /= (f32)cfg->cap;
}

static inline void maf_exec_in(maf_filter_t *maf, f32 x) {
        DECL_PTRS(maf, in);

        in->x = x;
        maf_exec(maf);
}

#endif // !MAF_H
