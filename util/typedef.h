#ifndef TYPEDEF_H
#define TYPEDEF_H

typedef unsigned char u8;
typedef signed char   i8;

typedef unsigned short u16;
typedef signed short   i16;

typedef unsigned int u32;
typedef signed int   i32;

//* on Windows, (unsigned long int) is 32bit
typedef unsigned long long u64;
typedef signed long long   i64;

typedef float  f32;
typedef double f64;

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
  f32 u;
  f32 v;
  f32 w;
} f32_uvw_t;

typedef struct {
  f32 a;
  f32 b;
} f32_ab_t;

typedef struct {
  f32 d;
  f32 q;
} f32_dq_t;

typedef struct {
  u32 npp;
  f32 ld;
  f32 lq;
  f32 rs;
  f32 psi; // Wb
  f32 wc;  // 电流环带宽
  f32 j;   // 转子惯量
  f32 cur2tor[4], tor2cur[4], max_tor;
} motor_cfg_t;

typedef struct {
  /* ADC */
  u32 adc_full_cnt;
  u32 adc_cali_cnt_max, theta_cali_cnt_max;
  f32 cur_range, vbus_range;
  f32 adc2cur, adc2vbus;
  u32 cur_gain;
  f32 cur_offset;

  /* PWM */
  u32 timer_freq;
  u32 pwm_freq;
  u32 pwm_full_cnt;
  f32 mi;
  f32 f32_pwm_min, f32_pwm_max;
} periph_cfg_t;

#endif // !TYPEDEF_H
