#ifndef MATHDEF_H
#define MATHDEF_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ARM_MATH
#include "arm_math.h"
#endif

#include <math.h>

#include "fastmath.h"
#include "typedef.h"

#ifdef FAST_MATH
#define SIN(x)       fast_sinf(x)
#define COS(x)       fast_cosf(x)
#define TAN(x)       fast_tanf(x)
#define EXP(x)       fast_expf(x)
#define ABS(x)       fast_absf(x)
#define SQRT(x)      fast_sqrtf(x)
#define MOD(x, y)    fast_modf(x, y) // __hardfp_fmodf
#define CPYSGN(x, y) copysignf(x, y)
#elif defined(ARM_MATH)
#define SIN(x)         arm_sin_f32(x)
#define COS(x)         arm_cos_f32(x)
#define ATAN2(y, x, r) arm_atan2_f32(y, x, r)
#define ABS(x)         fast_absf(x)
#define EXP(x)         fast_expf(x)
#define SQRT(x)        fast_sqrtf(x)
#define MOD(x, y)      fast_modf(x, y) // __hardfp_fmodf
#define CPYSGN(x, y)   copysignf(x, y)
#else
#define SIN(x)       sinf(x)
#define COS(x)       cosf(x)
#define EXP(x)       expf(x)
#define ATAN2(y, x)  atan2f(y, x)
#define ABS(x)       fabsf(x)
#define SQRT(x)      sqrtf(x)
#define MOD(x, y)    fmodf(x, y) // __hardfp_fmodf
#define CPYSGN(x, y) copysignf(x, y)
#endif

#ifndef PI
#define PI 3.1415926F
#endif
#define TAU                            6.2831853F
#define E                              2.7182818F
#define DIV_PI_BY_2                    1.5707963F
#define LN2                            0.6931471F
#define DIV_2_BY_3                     0.6666666F
#define SQRT_2                         1.4142135F
#define SQRT_3                         1.7320508F
#define DIV_1_BY_SQRT_3                0.5773502F
#define DIV_SQRT_3_BY_2                0.8660254F

#define IS_NAN(x)                      (isnan(x))
#define IS_IN_RANGE(val, min, max)     ((val) >= (min) && (val) <= (max))

#define SGN(x)                         ((x) == 0.0F ? 0.0F : (x) > 0.0F ? 1.0F : -1.0F)

#define SQ(x)                          ((x) * (x))         // 平方
#define K(x)                           ((x) * 1000U)       // 乘以 1K
#define M(x)                           ((x) * 1000000U)    // 乘以 1M
#define G(x)                           ((x) * 1000000000U) // 乘以 1G

#define MIN(x, y)                      (((x) < (y)) ? (x) : (y))
#define MAX(x, y)                      (((x) > (y)) ? (x) : (y))

#define RAD_TO_DEG(rad)                ((rad) * 57.2957795F)
#define DEG_TO_RAD(deg)                ((deg) / 57.2957795F)

#define US_TO_S(us)                    ((us) / 1000000U)

#define HZ_TO_S(hz)                    (1.0F / (hz))
#define HZ_TO_MS(hz)                   (1.0F / (hz) * 1000U)
#define HZ_TO_US(hz)                   (1.0F / (hz) * 1000000U)

#define MECH_TO_ELEC(theta, npp)       ((theta) * (npp))
#define ELEC_TO_MECH(theta, npp)       ((theta) / (npp))

#define LF(n)                          (1U << (n))
#define RF(n)                          (1U >> (n))

#define SELF_LF(x, n)                  ((x) <<= (n))
#define SELF_RF(x, n)                  ((x) >>= (n))

#define INTEGRATOR(ret, val, gain, fs) (ret) += (gain) * (val) / (fs)

#define DERIVATIVE(ret, val, prev_val, gain, fs)                                                   \
  do {                                                                                             \
    (ret)      = (gain) * ((val) - (prev_val)) * (fs);                                             \
    (prev_val) = (val);                                                                            \
  } while (0)

#define THETA_DERIVATIVE(ret, theta, prev_theta, gain, fs)                                         \
  do {                                                                                             \
    fp32 theta_diff = (theta) - (prev_theta);                                                      \
    WARP_PI(theta_diff);                                                                           \
    (ret)        = (gain) * (theta_diff) * (fs);                                                   \
    (prev_theta) = (theta);                                                                        \
  } while (0)

#define LOWPASS(ret, val, fc, fs)                                                                  \
  do {                                                                                             \
    fp32 rc    = 1.0F / (TAU * fc);                                                                \
    fp32 alpha = 1.0F / (1.0F + rc * fs);                                                          \
    (ret)      = (alpha) * (val) + (1.0F - alpha) * (ret);                                         \
  } while (0)

#define CLAMP(ret, min, max) (ret) = ((ret) <= (min)) ? (min) : MIN(ret, max)

#define UVW_CLAMP(ret, min, max)                                                                   \
  do {                                                                                             \
    CLAMP((ret).u, (min), (max));                                                                  \
    CLAMP((ret).v, (min), (max));                                                                  \
    CLAMP((ret).w, (min), (max));                                                                  \
  } while (0)

#define UPDATE_MIN_MAX(min, max, val)                                                              \
  do {                                                                                             \
    if ((val) < (min))                                                                             \
      (min) = (val);                                                                               \
    else if ((val) > (max))                                                                        \
      (max) = (val);                                                                               \
  } while (0)

#define WARP_PI(rad)                                                                               \
  do {                                                                                             \
    if (ABS(rad) > TAU)                                                                            \
      (rad) = MOD((rad), TAU);                                                                     \
    if ((rad) > PI)                                                                                \
      (rad) -= TAU;                                                                                \
    else if ((rad) < -PI)                                                                          \
      (rad) += TAU;                                                                                \
  } while (0)

#define WARP_TAU(rad)                                                                              \
  do {                                                                                             \
    if (ABS(rad) > TAU)                                                                            \
      (rad) = MOD((rad), TAU);                                                                     \
    if ((rad) < 0.0F)                                                                              \
      (rad) += TAU;                                                                                \
  } while (0)

#define UVW_ADD_VEC_IP(x, y)                                                                       \
  do {                                                                                             \
    (x).u = (x).u + (y).u;                                                                         \
    (x).v = (x).v + (y).v;                                                                         \
    (x).w = (x).w + (y).w;                                                                         \
  } while (0)

#define UVW_SUB_VEC_IP(x, y)                                                                       \
  do {                                                                                             \
    (x).u = (x).u - (y).u;                                                                         \
    (x).v = (x).v - (y).v;                                                                         \
    (x).w = (x).w - (y).w;                                                                         \
  } while (0)

#define UVW_MUL_VEC_IP(x, y)                                                                       \
  do {                                                                                             \
    (x).u = (x).u * (y).u;                                                                         \
    (x).v = (x).v * (y).v;                                                                         \
    (x).w = (x).w * (y).w;                                                                         \
  } while (0)

#define UVW_DIV_VEC_IP(x, y)                                                                       \
  do {                                                                                             \
    (x).u = (x).u / (y).u;                                                                         \
    (x).v = (x).v / (y).v;                                                                         \
    (x).w = (x).w / (y).w;                                                                         \
  } while (0)

#define UVW_ADD_IP(x, y)                                                                           \
  do {                                                                                             \
    (x).u = (x).u + (y);                                                                           \
    (x).v = (x).v + (y);                                                                           \
    (x).w = (x).w + (y);                                                                           \
  } while (0)

#define UVW_MUL_IP(x, y)                                                                           \
  do {                                                                                             \
    (x).u = (x).u * (y);                                                                           \
    (x).v = (x).v * (y);                                                                           \
    (x).w = (x).w * (y);                                                                           \
  } while (0)

#define UVW_ADD(ret, x, y)                                                                         \
  do {                                                                                             \
    (ret).u = (x).u + (y);                                                                         \
    (ret).v = (x).v + (y);                                                                         \
    (ret).w = (x).w + (y);                                                                         \
  } while (0)

#define UVW_MUL(ret, x, y)                                                                         \
  do {                                                                                             \
    (ret).u = (x).u * (y);                                                                         \
    (ret).v = (x).v * (y);                                                                         \
    (ret).w = (x).w * (y);                                                                         \
  } while (0)

#define AB_ADD_VEC(ret, x, y)                                                                      \
  do {                                                                                             \
    (ret).a = (x).a + (y).a;                                                                       \
    (ret).b = (x).b + (y).b;                                                                       \
  } while (0)

#define AB_SUB_VEC(ret, x, y)                                                                      \
  do {                                                                                             \
    (ret).a = (x).a - (y).a;                                                                       \
    (ret).b = (x).b - (y).b;                                                                       \
  } while (0)

#define AB_MUL_VEC(ret, x, y)                                                                      \
  do {                                                                                             \
    (ret).a = (x).a * (y).a;                                                                       \
    (ret).b = (x).b * (y).b;                                                                       \
  } while (0)

#define AB_DIV_VEC(ret, x, y)                                                                      \
  do {                                                                                             \
    (ret).a = (x).a / (y).a;                                                                       \
    (ret).b = (x).b / (y).b;                                                                       \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif // !MATHDEF_H
