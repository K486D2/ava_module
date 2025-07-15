#ifndef TYPEDEF_H
#define TYPEDEF_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u8;
typedef signed char   i8;

typedef unsigned short u16;
typedef signed short   i16;

typedef unsigned int u32;
typedef signed int   i32;

//* on Windows, (unsigned long int) is 32bit
typedef unsigned long long u64;
typedef signed long long   i64;

typedef float  fp32;
typedef double fp64;

#ifndef __cplusplus
#define bool  u8
#define true  1
#define false 0
#endif

typedef struct {
  u32 u;
  u32 v;
  u32 w;
} u32_uvw_t;

typedef struct {
  i32 u;
  i32 v;
  i32 w;
} i32_uvw_t;

typedef struct {
  fp32 u;
  fp32 v;
  fp32 w;
} fp32_uvw_t;

typedef struct {
  fp32 a;
  fp32 b;
} fp32_ab_t;

typedef struct {
  fp32 d;
  fp32 q;
} fp32_dq_t;

typedef struct {
  u32  npp;
  fp32 ld;
  fp32 lq;
  fp32 ls;
  fp32 rs;
  fp32 psi;
} motor_cfg_t;

typedef struct {
  /* ADC */
  u32  adc_cnt_max;
  u32  adc_cail_cnt_max;
  fp32 cur_range, vbus_range;
  fp32 adc2cur, adc2vbus;

  /* PWM */
  u32  timer_freq;
  u32  pwm_freq;
  u32  pwm_cnt_max;
  fp32 mi;
  fp32 fp32_pwm_min, fp32_pwm_max;
} periph_cfg_t;

#ifdef __cplusplus
}
#endif

#endif // !TYPEDEF_H
