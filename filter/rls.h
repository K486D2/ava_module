#ifndef RLS_H
#define RLS_H

#include "../util/util.h"

#define MAX_ORDER 8

typedef struct {
  u32 order;
  f32 lambda;
  f32 delta;
} rls_cfg_t;

typedef struct {
  f32 x;
  f32 ref;
} rls_in_t;

typedef struct {
  f32 y_hat;
} rls_out_t;

typedef struct {
  f32 err;
  f32 denom;
  f32 w[MAX_ORDER];
  f32 x[MAX_ORDER];
  f32 p[MAX_ORDER][MAX_ORDER];
  f32 px[MAX_ORDER];
  f32 k[MAX_ORDER];
  f32 temp[MAX_ORDER][MAX_ORDER];
  f32 xtp[MAX_ORDER];
} rls_lo_t;

typedef struct {
  rls_cfg_t cfg;
  rls_in_t  in;
  rls_out_t out;
  rls_lo_t  lo;
} rls_filter_t;

#define DECL_RLS_PTRS(rls)                                                                         \
  rls_filter_t *p   = (rls);                                                                       \
  rls_cfg_t    *cfg = &p->cfg;                                                                     \
  rls_in_t     *in  = &p->in;                                                                      \
  rls_out_t    *out = &p->out;                                                                     \
  rls_lo_t     *lo  = &p->lo;                                                                      \
  ARG_UNUSED(p);                                                                                   \
  ARG_UNUSED(cfg);                                                                                 \
  ARG_UNUSED(in);                                                                                  \
  ARG_UNUSED(out);                                                                                 \
  ARG_UNUSED(lo);

#define DECL_RLS_PTRS_PREFIX(rls, prefix)                                                          \
  rls_filter_t *prefix##_p   = (rls);                                                              \
  rls_cfg_t    *prefix##_cfg = &prefix##_p->cfg;                                                   \
  rls_in_t     *prefix##_in  = &prefix##_p->in;                                                    \
  rls_out_t    *prefix##_out = &prefix##_p->out;                                                   \
  rls_lo_t     *prefix##_lo  = &prefix##_p->lo;                                                    \
  ARG_UNUSED(prefix##_p);                                                                          \
  ARG_UNUSED(prefix##_cfg);                                                                        \
  ARG_UNUSED(prefix##_in);                                                                         \
  ARG_UNUSED(prefix##_out);                                                                        \
  ARG_UNUSED(prefix##_lo);

static void rls_init(rls_filter_t *rls, rls_cfg_t rls_cfg) {
  DECL_RLS_PTRS(rls);

  *cfg = rls_cfg;
}

static void rls_exec(rls_filter_t *rls) {
  DECL_RLS_PTRS(rls);

  for (u32 i = cfg->order - 1; i > 0; i--)
    lo->x[i] = lo->x[i - 1];
  lo->x[0] = in->x;

  for (u32 i = 0; i < cfg->order; i++)
    out->y_hat += lo->w[i] * lo->x[i];
  lo->err = in->ref - out->y_hat;

  for (u32 i = 0; i < cfg->order; i++) {
    for (int j = 0; j < cfg->order; j++)
      lo->px[i] += lo->p[i][j] * lo->x[j];
  }

  for (u32 i = 0; i < cfg->order; i++)
    lo->denom += lo->x[i] * lo->px[i];

  for (u32 i = 0; i < cfg->order; i++)
    lo->k[i] = lo->px[i] / lo->denom;

  for (u32 i = 0; i < cfg->order; i++)
    lo->w[i] += lo->k[i] * lo->err;

  for (u32 j = 0; j < cfg->order; j++) {
    for (u32 i = 0; i < cfg->order; i++)
      lo->xtp[j] += lo->x[i] * lo->p[i][j];
  }

  for (u32 i = 0; i < cfg->order; i++) {
    for (u32 j = 0; j < cfg->order; j++)
      lo->temp[i][j] = lo->k[i] * lo->xtp[j];
  }

  for (u32 i = 0; i < cfg->order; i++) {
    for (u32 j = 0; j < cfg->order; j++)
      lo->p[i][j] = (lo->p[i][j] - lo->temp[i][j]) / cfg->lambda;
  }
}

#endif // !RLS_H
