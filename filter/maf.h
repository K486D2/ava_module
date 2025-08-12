#ifndef MAF_H
#define MAF_H

#ifdef __cpluscplus
extern "C" {
#endif

#include "../container/fifo.h"
#include "../util/util.h"

typedef struct {
  f32 *buf;
  u32  buf_size;
} maf_cfg_t;

typedef struct {
  f32 raw_val;
} maf_in_t;

typedef struct {
  f32 filter_val;
} maf_out_t;

typedef struct {
  fifo_t fifo;
  f64    sum_val;
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

  f32 old_val;
  fifo_out(&lo->fifo, &old_val, sizeof(old_val));
  lo->sum_val -= old_val;

  fifo_in(&lo->fifo, &in->raw_val, sizeof(in->raw_val));

  lo->sum_val += in->raw_val;

  lo->sum_val /= (f32)cfg->buf_size;
}

static void maf_exec_in(maf_filter_t *maf, f32 raw_val) {
  DECL_MAF_PTRS(maf);

  in->raw_val = raw_val;
  maf_exec(maf);
}

#ifdef __cpluscplus
}
#endif

#endif // !MAF_H
