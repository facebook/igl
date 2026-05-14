/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Uniform.h>

namespace igl::tests {

TEST(UniformTest, sizeForUniformType) {
  EXPECT_EQ(sizeForUniformType(UniformType::Invalid), 0u);
  EXPECT_EQ(sizeForUniformType(UniformType::Float), sizeof(float));
  EXPECT_EQ(sizeForUniformType(UniformType::Float2), sizeof(float[2]));
  EXPECT_EQ(sizeForUniformType(UniformType::Float3), sizeof(float[3]));
  EXPECT_EQ(sizeForUniformType(UniformType::Float4), sizeof(float[4]));
  EXPECT_EQ(sizeForUniformType(UniformType::Boolean), sizeof(bool));
  EXPECT_EQ(sizeForUniformType(UniformType::Int), sizeof(int32_t));
  EXPECT_EQ(sizeForUniformType(UniformType::Int2), sizeof(int32_t[2]));
  EXPECT_EQ(sizeForUniformType(UniformType::Int3), sizeof(int32_t[3]));
  EXPECT_EQ(sizeForUniformType(UniformType::Int4), sizeof(int32_t[4]));
  EXPECT_EQ(sizeForUniformType(UniformType::Mat2x2), sizeof(float[2][2]));
  EXPECT_EQ(sizeForUniformType(UniformType::Mat3x3), sizeof(float[3][3]));
  EXPECT_EQ(sizeForUniformType(UniformType::Mat4x4), sizeof(float[4][4]));
}

TEST(UniformTest, sizeForUniformElementType) {
  EXPECT_EQ(sizeForUniformElementType(UniformType::Invalid), 0u);
  EXPECT_EQ(sizeForUniformElementType(UniformType::Float), sizeof(float));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Float2), sizeof(float));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Float3), sizeof(float));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Float4), sizeof(float));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Boolean), sizeof(bool));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Int), sizeof(int32_t));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Int2), sizeof(int32_t));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Int3), sizeof(int32_t));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Int4), sizeof(int32_t));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Mat2x2), sizeof(float));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Mat3x3), sizeof(float));
  EXPECT_EQ(sizeForUniformElementType(UniformType::Mat4x4), sizeof(float));
}

} // namespace igl::tests
