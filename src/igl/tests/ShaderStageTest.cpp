/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Shader.h>

namespace igl::tests {

TEST(ShaderStageTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(ShaderStage::Vertex), 0u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderStage::Fragment), 1u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderStage::Compute), 2u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderStage::Task), 3u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderStage::Mesh), 4u);
}

TEST(ShaderStageTest, ValuesAreDistinct) {
  const uint8_t vertex = static_cast<uint8_t>(ShaderStage::Vertex);
  const uint8_t fragment = static_cast<uint8_t>(ShaderStage::Fragment);
  const uint8_t compute = static_cast<uint8_t>(ShaderStage::Compute);
  const uint8_t task = static_cast<uint8_t>(ShaderStage::Task);
  const uint8_t mesh = static_cast<uint8_t>(ShaderStage::Mesh);
  EXPECT_NE(vertex, fragment);
  EXPECT_NE(vertex, compute);
  EXPECT_NE(fragment, compute);
  EXPECT_NE(task, mesh);
}

TEST(ConstantValueTypeTest, InvalidIsZero) {
  EXPECT_EQ(static_cast<uint8_t>(ConstantValueType::Invalid), 0u);
}

TEST(ConstantValueTypeTest, GetSizeInvalid) {
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Invalid), 0u);
}

TEST(ConstantValueTypeTest, GetSizeFloatTypes) {
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Float1), 4u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Float2), 8u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Float3), 12u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Float4), 16u);
}

TEST(ConstantValueTypeTest, GetSizeBooleanTypes) {
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Boolean1), 4u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Boolean2), 8u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Boolean3), 12u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Boolean4), 16u);
}

TEST(ConstantValueTypeTest, GetSizeIntTypes) {
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Int1), 4u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Int2), 8u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Int3), 12u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Int4), 16u);
}

TEST(ConstantValueTypeTest, GetSizeMatrixTypes) {
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Mat2x2), 16u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Mat3x3), 36u);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Mat4x4), 64u);
}

TEST(ConstantValueTypeTest, ScalarSizesAreConsistent) {
  const size_t floatSize = getConstantValueSize(ConstantValueType::Float1);
  const size_t boolSize = getConstantValueSize(ConstantValueType::Boolean1);
  const size_t intSize = getConstantValueSize(ConstantValueType::Int1);
  EXPECT_EQ(floatSize, boolSize);
  EXPECT_EQ(floatSize, intSize);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Float2), floatSize * 2);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Float3), floatSize * 3);
  EXPECT_EQ(getConstantValueSize(ConstantValueType::Float4), floatSize * 4);
}

} // namespace igl::tests
