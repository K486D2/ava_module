#ifndef MATHDEF_H
#define MATHDEF_H

#ifdef ARM_MATH
#include "arm_math.h"
#endif

#if defined(__linux__) || defined(_WIN32)
#include <immintrin.h>
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
#define LOG(x)       log(x)
#elif defined(ARM_MATH)
#define SIN(x)       arm_sin_f32(x)
#define COS(x)       arm_cos_f32(x)
#define ATAN2(y, x)  atan2f(y, x)
#define ABS(x)       fast_absf(x)
#define EXP(x)       fast_expf(x)
#define SQRT(x)      fast_sqrtf(x)
#define MOD(x, y)    fast_modf(x, y) // __hardfp_fmodf
#define CPYSGN(x, y) copysignf(x, y)
#define LOG(x)       log(x)
#else
#define SIN(x)       sinf(x)
#define SINH(x)      sinhf(x)
#define COS(x)       cosf(x)
#define EXP(x)       expf(x)
#define ATAN2(y, x)  atan2f(y, x)
#define ABS(x)       fabsf(x)
#define SQRT(x)      sqrtf(x)
#define MOD(x, y)    fmodf(x, y) // __hardfp_fmodf
#define CPYSGN(x, y) copysignf(x, y)
#define LOG(x)       logf(x)
#endif

#ifndef PI
#define PI 3.1415926F
#endif

#ifndef E
#define E 2.7182818F
#endif

#define TAU             6.2831853F
#define DIV_PI_BY_2     1.5707963F
#define LN2             0.6931471F
#define DIV_2_BY_3      0.6666666F
#define SQRT_2          1.4142135F
#define SQRT_3          1.7320508F
#define DIV_1_BY_SQRT_3 0.5773502F
#define DIV_SQRT_3_BY_2 0.8660254F

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define IS_NAN(x)                      (isnan(x))
#define IS_IN_RANGE(val, min, max)     ((val) >= (min) && (val) <= (max))
#define IS_EQ(x, y)                    (ABS((x) - (y)) <= (1e-6f * MAX(ABS(x), ABS(y)) + 1e-6f))

#define SGN(x)                         ((x) == 0.0F ? 0.0F : (x) > 0.0F ? 1.0F : -1.0F)

#define SQ(x)                          ((x) * (x))         // 平方
#define K(x)                           ((x) * 1000U)       // 乘以 1K
#define M(x)                           ((x) * 1000000U)    // 乘以 1M
#define G(x)                           ((x) * 1000000000U) // 乘以 1G

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

#define INTEGRATOR(ret, val, gain, fs) (ret) += (gain) * (val) / (fs)

#define DERIVATIVE(ret, val, prev_val, gain, fs)                                                   \
  do {                                                                                             \
    (ret)      = (gain) * ((val) - (prev_val)) * (fs);                                             \
    (prev_val) = (val);                                                                            \
  } while (0)

#define THETA_DERIVATIVE(ret, theta, prev_theta, gain, fs)                                         \
  do {                                                                                             \
    f32 theta_diff = (theta) - (prev_theta);                                                       \
    WARP_PI(theta_diff);                                                                           \
    (ret)        = (gain) * (theta_diff) * (fs);                                                   \
    (prev_theta) = (theta);                                                                        \
  } while (0)

#define LOWPASS(ret, val, fc, fs)                                                                  \
  do {                                                                                             \
    f32 rc    = 1.0F / (TAU * fc);                                                                 \
    f32 alpha = 1.0F / (1.0F + rc * fs);                                                           \
    (ret)     = (alpha) * (val) + (1.0F - alpha) * (ret);                                          \
  } while (0)

#define CLAMP(ret, min, max) (ret) = ((ret) <= (min)) ? (min) : MIN(ret, max)

#define UVW_CLAMP(ret, min, max)                                                                   \
  do {                                                                                             \
    CLAMP((ret).u, (min), (max));                                                                  \
    CLAMP((ret).v, (min), (max));                                                                  \
    CLAMP((ret).w, (min), (max));                                                                  \
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

static inline void find_max(const float *arr, size_t n, float *max_val, size_t *max_idx) {
  if (n == 0) {
    *max_val = 0.0f;
    *max_idx = 0;
    return;
  }

#if defined(__AVX__)
  size_t i       = 0;
  __m256 max_vec = _mm256_set1_ps(arr[0]);
  float  tmp_max = arr[0];
  size_t idx_max = 0;

  for (; i + 7 < n; i += 8) {
    __m256 v = _mm256_loadu_ps(arr + i);
    max_vec  = _mm256_max_ps(max_vec, v);

    float tmp[8];
    _mm256_storeu_ps(tmp, max_vec);
    for (int j = 0; j < 8; j++) {
      if (tmp[j] > tmp_max) {
        tmp_max = tmp[j];
        idx_max = i + j;
      }
    }
  }

  for (; i < n; i++) {
    if (arr[i] > tmp_max) {
      tmp_max = arr[i];
      idx_max = i;
    }
  }

  *max_val = tmp_max;
  *max_idx = idx_max;

#elif defined(__SSE__)
  size_t i       = 0;
  __m128 max_vec = _mm_set1_ps(arr[0]);
  float  tmp_max = arr[0];
  size_t idx_max = 0;

  for (; i + 3 < n; i += 4) {
    __m128 v = _mm_loadu_ps(arr + i);
    max_vec  = _mm_max_ps(max_vec, v);

    float tmp[4];
    _mm_storeu_ps(tmp, max_vec);
    for (int j = 0; j < 4; j++) {
      if (tmp[j] > tmp_max) {
        tmp_max = tmp[j];
        idx_max = i + j;
      }
    }
  }

  for (; i < n; i++) {
    if (arr[i] > tmp_max) {
      tmp_max = arr[i];
      idx_max = i;
    }
  }

  *max_val = tmp_max;
  *max_idx = idx_max;

#else
  float  tmp_max = arr[0];
  size_t idx_max = 0;
  for (size_t i = 1; i < n; i++) {
    if (arr[i] > tmp_max) {
      tmp_max = arr[i];
      idx_max = i;
    }
  }
  *max_val = tmp_max;
  *max_idx = idx_max;
#endif
}

#endif // !MATHDEF_H
