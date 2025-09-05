#ifndef MAF_H
#define MAF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../container/fifo.h"
#include "../util/util.h"

typedef struct {
  f32 *buf;
  u32  buf_size;
} maf_cfg_t;

typedef struct {
  f32 x;
} maf_in_t;

typedef struct {
  f32 y;
} maf_out_t;

typedef struct {
  fifo_t fifo;
  f64    x_sum;
} maf_lo_t;

typedef struct {
  maf_cfg_t cfg;
  maf_in_t  in;
  maf_out_t out;
  maf_lo_t  lo;
} maf_filter_t;

#define DECL_MAF_PTRS(maf)                                                                         \
  maf_filter_t *p   = (maf);                                                                       \
  maf_cfg_t    *cfg = &p->cfg;                                                                     \
  maf_in_t     *in  = &p->in;                                                                      \
  maf_out_t    *out = &p->out;                                                                     \
  maf_lo_t     *lo  = &p->lo;                                                                      \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_MAF_PTRS_PREFIX(maf, prefix)                                                          \
  maf_filter_t *prefix##_p   = (maf);                                                              \
  maf_cfg_t    *prefix##_cfg = &prefix##_p->cfg;                                                   \
  maf_in_t     *prefix##_in  = &prefix##_p->in;                                                    \
  maf_out_t    *prefix##_out = &prefix##_p->out;                                                   \
  maf_lo_t     *prefix##_lo  = &prefix##_p->lo;                                                    \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static void maf_init(maf_filter_t *maf, maf_cfg_t maf_cfg) {
  DECL_MAF_PTRS(maf);

  *cfg = maf_cfg;
  fifo_init(&lo->fifo, cfg->buf, cfg->buf_size);
}

static void maf_exec(maf_filter_t *maf) {
  DECL_MAF_PTRS(maf);

  f32 prev_x;
  fifo_out(&lo->fifo, &prev_x, sizeof(prev_x));
  lo->x_sum -= prev_x;

  fifo_in(&lo->fifo, &in->x, sizeof(in->x));

  lo->x_sum += in->x;
  lo->x_sum /= (f32)cfg->buf_size;
}

static void maf_exec_in(maf_filter_t *maf, f32 x) {
  DECL_MAF_PTRS(maf);

  in->x = x;
  maf_exec(maf);
}

#ifdef __cplusplus
}
#endif

#endif // !MAF_H
