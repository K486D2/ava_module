#ifndef ADRC_H
#define ADRC_H

#include "util/typedef.h"

typedef struct {
        f32 fs;
} adrc_cfg_t;

typedef struct {

} adrc_in_t;

typedef struct {

} adrc_out_t;

typedef struct {

} adrc_lo_t;

typedef struct {
        adrc_cfg_t cfg;
        adrc_in_t  in;
        adrc_out_t out;
        adrc_lo_t  lo;
} adrc_ctl_t;

#endif // !ADRC_H
