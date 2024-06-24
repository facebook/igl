/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/simdtypes/SimdTypes.h>
#include <cmath>

namespace iglu::simdtypes {

#if defined(__APPLE__)

inline float1 clamp(float1 x, float1 min, float1 max) {
  return simd_clamp(x, min, max);
}

inline float1 fract(float1 x) {
  return simd_fract(x);
}

inline float4x4 inverse(const float4x4& m) {
  return simd_inverse(m);
}

// result = m * v;
inline float4 multiply(const float4x4& m, const float4& v) {
  return simd_mul(m, v);
}

// result = m1 * m2
inline float4x4 multiply(const float4x4& m1, const float4x4& m2) {
  return simd_mul(m1, m2);
}

#else

inline float1 clamp(float1 x, float1 min, float1 max) {
  if (x < min) {
    return min;
  }
  if (x > max) {
    return max;
  }
  return x;
}

inline float1 fract(float1 x) {
  return x - floor(x);
}

inline float4x4 inverse(const float4x4& m) {
  float4x4 result{};

  result.columns[0][0] = m.columns[1][1] * m.columns[2][2] * m.columns[3][3] -
                         m.columns[1][1] * m.columns[2][3] * m.columns[3][2] -
                         m.columns[2][1] * m.columns[1][2] * m.columns[3][3] +
                         m.columns[2][1] * m.columns[1][3] * m.columns[3][2] +
                         m.columns[3][1] * m.columns[1][2] * m.columns[2][3] -
                         m.columns[3][1] * m.columns[1][3] * m.columns[2][2];

  result.columns[1][0] = -m.columns[1][0] * m.columns[2][2] * m.columns[3][3] +
                         m.columns[1][0] * m.columns[2][3] * m.columns[3][2] +
                         m.columns[2][0] * m.columns[1][2] * m.columns[3][3] -
                         m.columns[2][0] * m.columns[1][3] * m.columns[3][2] -
                         m.columns[3][0] * m.columns[1][2] * m.columns[2][3] +
                         m.columns[3][0] * m.columns[1][3] * m.columns[2][2];

  result.columns[2][0] = m.columns[1][0] * m.columns[2][1] * m.columns[3][3] -
                         m.columns[1][0] * m.columns[2][3] * m.columns[3][1] -
                         m.columns[2][0] * m.columns[1][1] * m.columns[3][3] +
                         m.columns[2][0] * m.columns[1][3] * m.columns[3][1] +
                         m.columns[3][0] * m.columns[1][1] * m.columns[2][3] -
                         m.columns[3][0] * m.columns[1][3] * m.columns[2][1];

  result.columns[3][0] = -m.columns[1][0] * m.columns[2][1] * m.columns[3][2] +
                         m.columns[1][0] * m.columns[2][2] * m.columns[3][1] +
                         m.columns[2][0] * m.columns[1][1] * m.columns[3][2] -
                         m.columns[2][0] * m.columns[1][2] * m.columns[3][1] -
                         m.columns[3][0] * m.columns[1][1] * m.columns[2][2] +
                         m.columns[3][0] * m.columns[1][2] * m.columns[2][1];

  result.columns[0][1] = -m.columns[0][1] * m.columns[2][2] * m.columns[3][3] +
                         m.columns[0][1] * m.columns[2][3] * m.columns[3][2] +
                         m.columns[2][1] * m.columns[0][2] * m.columns[3][3] -
                         m.columns[2][1] * m.columns[0][3] * m.columns[3][2] -
                         m.columns[3][1] * m.columns[0][2] * m.columns[2][3] +
                         m.columns[3][1] * m.columns[0][3] * m.columns[2][2];

  result.columns[1][1] = m.columns[0][0] * m.columns[2][2] * m.columns[3][3] -
                         m.columns[0][0] * m.columns[2][3] * m.columns[3][2] -
                         m.columns[2][0] * m.columns[0][2] * m.columns[3][3] +
                         m.columns[2][0] * m.columns[0][3] * m.columns[3][2] +
                         m.columns[3][0] * m.columns[0][2] * m.columns[2][3] -
                         m.columns[3][0] * m.columns[0][3] * m.columns[2][2];

  result.columns[2][1] = -m.columns[0][0] * m.columns[2][1] * m.columns[3][3] +
                         m.columns[0][0] * m.columns[2][3] * m.columns[3][1] +
                         m.columns[2][0] * m.columns[0][1] * m.columns[3][3] -
                         m.columns[2][0] * m.columns[0][3] * m.columns[3][1] -
                         m.columns[3][0] * m.columns[0][1] * m.columns[2][3] +
                         m.columns[3][0] * m.columns[0][3] * m.columns[2][1];

  result.columns[3][1] = m.columns[0][0] * m.columns[2][1] * m.columns[3][2] -
                         m.columns[0][0] * m.columns[2][2] * m.columns[3][1] -
                         m.columns[2][0] * m.columns[0][1] * m.columns[3][2] +
                         m.columns[2][0] * m.columns[0][2] * m.columns[3][1] +
                         m.columns[3][0] * m.columns[0][1] * m.columns[2][2] -
                         m.columns[3][0] * m.columns[0][2] * m.columns[2][1];

  result.columns[0][2] = m.columns[0][1] * m.columns[1][2] * m.columns[3][3] -
                         m.columns[0][1] * m.columns[1][3] * m.columns[3][2] -
                         m.columns[1][1] * m.columns[0][2] * m.columns[3][3] +
                         m.columns[1][1] * m.columns[0][3] * m.columns[3][2] +
                         m.columns[3][1] * m.columns[0][2] * m.columns[1][3] -
                         m.columns[3][1] * m.columns[0][3] * m.columns[1][2];

  result.columns[1][2] = -m.columns[0][0] * m.columns[1][2] * m.columns[3][3] +
                         m.columns[0][0] * m.columns[1][3] * m.columns[3][2] +
                         m.columns[1][0] * m.columns[0][2] * m.columns[3][3] -
                         m.columns[1][0] * m.columns[0][3] * m.columns[3][2] -
                         m.columns[3][0] * m.columns[0][2] * m.columns[1][3] +
                         m.columns[3][0] * m.columns[0][3] * m.columns[1][2];

  result.columns[2][2] = m.columns[0][0] * m.columns[1][1] * m.columns[3][3] -
                         m.columns[0][0] * m.columns[1][3] * m.columns[3][1] -
                         m.columns[1][0] * m.columns[0][1] * m.columns[3][3] +
                         m.columns[1][0] * m.columns[0][3] * m.columns[3][1] +
                         m.columns[3][0] * m.columns[0][1] * m.columns[1][3] -
                         m.columns[3][0] * m.columns[0][3] * m.columns[1][1];

  result.columns[3][2] = -m.columns[0][0] * m.columns[1][1] * m.columns[3][2] +
                         m.columns[0][0] * m.columns[1][2] * m.columns[3][1] +
                         m.columns[1][0] * m.columns[0][1] * m.columns[3][2] -
                         m.columns[1][0] * m.columns[0][2] * m.columns[3][1] -
                         m.columns[3][0] * m.columns[0][1] * m.columns[1][2] +
                         m.columns[3][0] * m.columns[0][2] * m.columns[1][1];

  result.columns[0][3] = -m.columns[0][1] * m.columns[1][2] * m.columns[2][3] +
                         m.columns[0][1] * m.columns[1][3] * m.columns[2][2] +
                         m.columns[1][1] * m.columns[0][2] * m.columns[2][3] -
                         m.columns[1][1] * m.columns[0][3] * m.columns[2][2] -
                         m.columns[2][1] * m.columns[0][2] * m.columns[1][3] +
                         m.columns[2][1] * m.columns[0][3] * m.columns[1][2];

  result.columns[1][3] = m.columns[0][0] * m.columns[1][2] * m.columns[2][3] -
                         m.columns[0][0] * m.columns[1][3] * m.columns[2][2] -
                         m.columns[1][0] * m.columns[0][2] * m.columns[2][3] +
                         m.columns[1][0] * m.columns[0][3] * m.columns[2][2] +
                         m.columns[2][0] * m.columns[0][2] * m.columns[1][3] -
                         m.columns[2][0] * m.columns[0][3] * m.columns[1][2];

  result.columns[2][3] = -m.columns[0][0] * m.columns[1][1] * m.columns[2][3] +
                         m.columns[0][0] * m.columns[1][3] * m.columns[2][1] +
                         m.columns[1][0] * m.columns[0][1] * m.columns[2][3] -
                         m.columns[1][0] * m.columns[0][3] * m.columns[2][1] -
                         m.columns[2][0] * m.columns[0][1] * m.columns[1][3] +
                         m.columns[2][0] * m.columns[0][3] * m.columns[1][1];

  result.columns[3][3] = m.columns[0][0] * m.columns[1][1] * m.columns[2][2] -
                         m.columns[0][0] * m.columns[1][2] * m.columns[2][1] -
                         m.columns[1][0] * m.columns[0][1] * m.columns[2][2] +
                         m.columns[1][0] * m.columns[0][2] * m.columns[2][1] +
                         m.columns[2][0] * m.columns[0][1] * m.columns[1][2] -
                         m.columns[2][0] * m.columns[0][2] * m.columns[1][1];

  float det = m.columns[0][0] * result.columns[0][0] + m.columns[0][1] * result.columns[1][0] +
              m.columns[0][2] * result.columns[2][0] + m.columns[0][3] * result.columns[3][0];

  if (det == 0) {
    return float4x4(1.0);
  }

  det = 1.0f / det;

  for (auto& column : result.columns) {
    for (int j = 0; j < 4; j++) {
      column[j] *= det;
    }
  }

  return result;
}

// result = m * v;
inline float4 multiply(const float4x4& m, const float4& v) {
  float4 result;
  result[0] = m.columns[0][0] * v[0] + m.columns[1][0] * v[1] + m.columns[2][0] * v[2] +
              m.columns[3][0] * v[3];
  result[1] = m.columns[0][1] * v[0] + m.columns[1][1] * v[1] + m.columns[2][1] * v[2] +
              m.columns[3][1] * v[3];
  result[2] = m.columns[0][2] * v[0] + m.columns[1][2] * v[1] + m.columns[2][2] * v[2] +
              m.columns[3][2] * v[3];
  result[3] = m.columns[0][3] * v[0] + m.columns[1][3] * v[1] + m.columns[2][3] * v[2] +
              m.columns[3][3] * v[3];
  return result;
}

// result = m1 * m2
inline float4x4 multiply(const float4x4& m1, const float4x4& m2) {
  float4x4 result{};
  result.columns[0] = multiply(m1, m2.columns[0]);
  result.columns[1] = multiply(m1, m2.columns[1]);
  result.columns[2] = multiply(m1, m2.columns[2]);
  result.columns[3] = multiply(m1, m2.columns[3]);
  return result;
}
#endif
} // namespace iglu::simdtypes
