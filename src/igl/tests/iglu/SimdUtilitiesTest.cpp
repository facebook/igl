/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/simdtypes/SimdUtilities.h>

#include <IGLU/simdtypes/SimdTypes.h>

namespace igl::tests {

using iglu::simdtypes::float4;
using iglu::simdtypes::float4x4;

// ---- clamp ----

TEST(SimdUtilitiesTest, ClampWithinRange) {
  EXPECT_FLOAT_EQ(iglu::simdtypes::clamp(0.5f, 0.0f, 1.0f), 0.5f);
}

TEST(SimdUtilitiesTest, ClampBelowMin) {
  EXPECT_FLOAT_EQ(iglu::simdtypes::clamp(-1.0f, 0.0f, 1.0f), 0.0f);
}

TEST(SimdUtilitiesTest, ClampAboveMax) {
  EXPECT_FLOAT_EQ(iglu::simdtypes::clamp(2.0f, 0.0f, 1.0f), 1.0f);
}

TEST(SimdUtilitiesTest, ClampAtBoundaries) {
  EXPECT_FLOAT_EQ(iglu::simdtypes::clamp(0.0f, 0.0f, 1.0f), 0.0f);
  EXPECT_FLOAT_EQ(iglu::simdtypes::clamp(1.0f, 0.0f, 1.0f), 1.0f);
}

TEST(SimdUtilitiesTest, ClampNegativeRange) {
  EXPECT_FLOAT_EQ(iglu::simdtypes::clamp(-5.0f, -10.0f, -2.0f), -5.0f);
  EXPECT_FLOAT_EQ(iglu::simdtypes::clamp(-15.0f, -10.0f, -2.0f), -10.0f);
  EXPECT_FLOAT_EQ(iglu::simdtypes::clamp(0.0f, -10.0f, -2.0f), -2.0f);
}

// ---- fract ----

TEST(SimdUtilitiesTest, FractPositive) {
  EXPECT_NEAR(iglu::simdtypes::fract(1.75f), 0.75f, 1e-6f);
}

TEST(SimdUtilitiesTest, FractNegative) {
  const float result = iglu::simdtypes::fract(-0.25f);
  EXPECT_NEAR(result, 0.75f, 1e-6f);
}

TEST(SimdUtilitiesTest, FractZero) {
  EXPECT_NEAR(iglu::simdtypes::fract(0.0f), 0.0f, 1e-6f);
}

TEST(SimdUtilitiesTest, FractInteger) {
  EXPECT_NEAR(iglu::simdtypes::fract(3.0f), 0.0f, 1e-6f);
}

TEST(SimdUtilitiesTest, FractLargeValue) {
  EXPECT_NEAR(iglu::simdtypes::fract(100.3f), 0.3f, 1e-4f);
}

// ---- multiply (matrix * vector) ----

TEST(SimdUtilitiesTest, MultiplyIdentityVector) {
  const float4x4 identity(1.0f);
  const float4 v = {1.0f, 2.0f, 3.0f, 1.0f};
  const float4 result = iglu::simdtypes::multiply(identity, v);
  EXPECT_NEAR(result[0], 1.0f, 1e-6f);
  EXPECT_NEAR(result[1], 2.0f, 1e-6f);
  EXPECT_NEAR(result[2], 3.0f, 1e-6f);
  EXPECT_NEAR(result[3], 1.0f, 1e-6f);
}

TEST(SimdUtilitiesTest, MultiplyScaleVector) {
  const float4 diag = {2.0f, 3.0f, 4.0f, 1.0f};
  const float4x4 scale(diag);
  const float4 v = {1.0f, 1.0f, 1.0f, 1.0f};
  const float4 result = iglu::simdtypes::multiply(scale, v);
  EXPECT_NEAR(result[0], 2.0f, 1e-6f);
  EXPECT_NEAR(result[1], 3.0f, 1e-6f);
  EXPECT_NEAR(result[2], 4.0f, 1e-6f);
  EXPECT_NEAR(result[3], 1.0f, 1e-6f);
}

TEST(SimdUtilitiesTest, MultiplyZeroVector) {
  const float4x4 m(1.0f);
  const float4 v = {0.0f, 0.0f, 0.0f, 0.0f};
  const float4 result = iglu::simdtypes::multiply(m, v);
  EXPECT_NEAR(result[0], 0.0f, 1e-6f);
  EXPECT_NEAR(result[1], 0.0f, 1e-6f);
  EXPECT_NEAR(result[2], 0.0f, 1e-6f);
  EXPECT_NEAR(result[3], 0.0f, 1e-6f);
}

// ---- multiply (matrix * matrix) ----

TEST(SimdUtilitiesTest, MultiplyIdentityMatrices) {
  const float4x4 identity(1.0f);
  const float4x4 result = iglu::simdtypes::multiply(identity, identity);
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      const float expected = (col == row) ? 1.0f : 0.0f;
      EXPECT_NEAR(result.columns[col][row], expected, 1e-6f);
    }
  }
}

TEST(SimdUtilitiesTest, MultiplyScaleMatrices) {
  const float4 diag1 = {2.0f, 2.0f, 2.0f, 1.0f};
  const float4 diag2 = {3.0f, 3.0f, 3.0f, 1.0f};
  const float4x4 s1(diag1);
  const float4x4 s2(diag2);
  const float4x4 result = iglu::simdtypes::multiply(s1, s2);
  EXPECT_NEAR(result.columns[0][0], 6.0f, 1e-6f);
  EXPECT_NEAR(result.columns[1][1], 6.0f, 1e-6f);
  EXPECT_NEAR(result.columns[2][2], 6.0f, 1e-6f);
  EXPECT_NEAR(result.columns[3][3], 1.0f, 1e-6f);
  EXPECT_NEAR(result.columns[0][1], 0.0f, 1e-6f);
  EXPECT_NEAR(result.columns[1][0], 0.0f, 1e-6f);
}

// ---- inverse ----

TEST(SimdUtilitiesTest, InverseIdentity) {
  const float4x4 identity(1.0f);
  const float4x4 result = iglu::simdtypes::inverse(identity);
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      const float expected = (col == row) ? 1.0f : 0.0f;
      EXPECT_NEAR(result.columns[col][row], expected, 1e-6f);
    }
  }
}

TEST(SimdUtilitiesTest, InverseScale) {
  const float4 diag = {2.0f, 4.0f, 8.0f, 1.0f};
  const float4x4 scale(diag);
  const float4x4 inv = iglu::simdtypes::inverse(scale);
  EXPECT_NEAR(inv.columns[0][0], 0.5f, 1e-6f);
  EXPECT_NEAR(inv.columns[1][1], 0.25f, 1e-6f);
  EXPECT_NEAR(inv.columns[2][2], 0.125f, 1e-6f);
  EXPECT_NEAR(inv.columns[3][3], 1.0f, 1e-6f);
}

TEST(SimdUtilitiesTest, InverseRoundTrip) {
  const float4 diag = {3.0f, 5.0f, 7.0f, 1.0f};
  const float4x4 m(diag);
  const float4x4 inv = iglu::simdtypes::inverse(m);
  const float4x4 product = iglu::simdtypes::multiply(m, inv);
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      const float expected = (col == row) ? 1.0f : 0.0f;
      EXPECT_NEAR(product.columns[col][row], expected, 1e-5f);
    }
  }
}

TEST(SimdUtilitiesTest, InverseTranslation) {
  float4x4 translation(1.0f);
  translation.columns[3] = float4{5.0f, -3.0f, 2.0f, 1.0f};
  const float4x4 inv = iglu::simdtypes::inverse(translation);
  EXPECT_NEAR(inv.columns[3][0], -5.0f, 1e-5f);
  EXPECT_NEAR(inv.columns[3][1], 3.0f, 1e-5f);
  EXPECT_NEAR(inv.columns[3][2], -2.0f, 1e-5f);
  EXPECT_NEAR(inv.columns[3][3], 1.0f, 1e-5f);
}

TEST(SimdUtilitiesTest, MultiplyTranslationVector) {
  float4x4 translation(1.0f);
  translation.columns[3] = float4{10.0f, 20.0f, 30.0f, 1.0f};
  const float4 point = {1.0f, 2.0f, 3.0f, 1.0f};
  const float4 result = iglu::simdtypes::multiply(translation, point);
  EXPECT_NEAR(result[0], 11.0f, 1e-6f);
  EXPECT_NEAR(result[1], 22.0f, 1e-6f);
  EXPECT_NEAR(result[2], 33.0f, 1e-6f);
  EXPECT_NEAR(result[3], 1.0f, 1e-6f);
}

TEST(SimdUtilitiesTest, InverseNonTrivialRoundTrip) {
  const float4 c0 = {1.0f, 0.0f, 0.0f, 0.0f};
  const float4 c1 = {0.0f, 0.0f, 1.0f, 0.0f};
  const float4 c2 = {0.0f, -1.0f, 0.0f, 0.0f};
  const float4 c3 = {4.0f, 5.0f, 6.0f, 1.0f};
  const float4x4 m(c0, c1, c2, c3);
  const float4x4 inv = iglu::simdtypes::inverse(m);
  const float4x4 product = iglu::simdtypes::multiply(m, inv);
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      const float expected = (col == row) ? 1.0f : 0.0f;
      EXPECT_NEAR(product.columns[col][row], expected, 1e-5f);
    }
  }
}

} // namespace igl::tests
