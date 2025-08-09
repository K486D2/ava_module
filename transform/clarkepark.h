#ifndef CLARKEPARK_H
#define CLARKEPARK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../util/def.h"

// clang-format off
/**
 * @brief 克拉克变换
 * 
 * @details $\begin{bmatrix}i_\alpha \\i_\beta\end{bmatrix}=\sqrt{\frac{2}{3}}\begin{bmatrix}1 & -\frac{1}{2} & -\frac{1}{2} \\0 & \frac{\sqrt{3}}{2} & -\frac{\sqrt{3}}{2}\end{bmatrix}\begin{bmatrix}i_u \\i_v \\i_w\end{bmatrix}$
 * 
 * @param fp32_abc 
 * @param mi 调制比 
 * @return f32_ab_t 
 */
// clang-format on
static inline f32_ab_t clarke(f32_uvw_t fp32_abc, f32 mi) {
  f32_ab_t fp32_ab;
  fp32_ab.a = mi * (fp32_abc.u - 0.5f * (fp32_abc.v + fp32_abc.w));
  fp32_ab.b = mi * (fp32_abc.v - fp32_abc.w) * DIV_SQRT_3_BY_2;
  return fp32_ab;
}

static inline f32_uvw_t inv_clarke(f32_ab_t fp32_ab) {
  f32_uvw_t fp32_uvw;
  f32       fp32_a = -(fp32_ab.a * 0.5f);
  f32       fp32_b = fp32_ab.b * DIV_SQRT_3_BY_2;
  fp32_uvw.u       = fp32_ab.a;
  fp32_uvw.v       = fp32_a + fp32_b;
  fp32_uvw.w       = fp32_a - fp32_b;
  return fp32_uvw;
}

static inline f32_dq_t park(f32_ab_t fp32_ab, f32 theta) {
  f32_dq_t fp32_dq;
  fp32_dq.d = COS(theta) * fp32_ab.a + SIN(theta) * fp32_ab.b;
  fp32_dq.q = COS(theta) * fp32_ab.b - SIN(theta) * fp32_ab.a;
  return fp32_dq;
}

static inline f32_ab_t inv_park(f32_dq_t fp32_dq, f32 theta) {
  f32_ab_t fp32_ab;
  fp32_ab.a = COS(theta) * fp32_dq.d - SIN(theta) * fp32_dq.q;
  fp32_ab.b = SIN(theta) * fp32_dq.d + COS(theta) * fp32_dq.q;
  return fp32_ab;
}

#ifdef __cplusplus
}
#endif

#endif // !CLARKEPARK_H
