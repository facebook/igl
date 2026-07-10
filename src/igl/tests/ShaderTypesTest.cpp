/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <igl/Shader.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// ShaderInputValidityTest — exhaustive isValid() boundary tests
// ---------------------------------------------------------------------------

TEST(ShaderInputValidityTest, StringInputWithSourceIsValid) {
  const ShaderInput input{.source = "void main() {}", .type = ShaderInputType::String};
  EXPECT_TRUE(input.isValid());
}

TEST(ShaderInputValidityTest, StringInputWithoutSourceIsInvalid) {
  const ShaderInput input{.source = nullptr, .type = ShaderInputType::String};
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputValidityTest, StringInputWithDataPresentIsInvalid) {
  const uint8_t blob = 0xFF;
  const ShaderInput input{
      .source = "void main() {}", .data = &blob, .type = ShaderInputType::String};
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputValidityTest, StringInputWithLengthNonZeroIsInvalid) {
  const ShaderInput input{.source = "void main() {}", .length = 5, .type = ShaderInputType::String};
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputValidityTest, BinaryInputWithDataIsValid) {
  const uint8_t spirv[] = {0x03, 0x02, 0x23, 0x07};
  const ShaderInput input{.data = spirv, .length = sizeof(spirv), .type = ShaderInputType::Binary};
  EXPECT_TRUE(input.isValid());
}

TEST(ShaderInputValidityTest, BinaryInputWithoutDataIsInvalid) {
  const ShaderInput input{.data = nullptr, .length = 4, .type = ShaderInputType::Binary};
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputValidityTest, BinaryInputWithZeroLengthIsInvalid) {
  const uint8_t spirv[] = {0x03, 0x02, 0x23, 0x07};
  const ShaderInput input{.data = spirv, .length = 0, .type = ShaderInputType::Binary};
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputValidityTest, BinaryInputWithSourcePresentIsInvalid) {
  const uint8_t spirv[] = {0x03, 0x02, 0x23, 0x07};
  const ShaderInput input{
      .source = "src", .data = spirv, .length = sizeof(spirv), .type = ShaderInputType::Binary};
  EXPECT_FALSE(input.isValid());
}

// ---------------------------------------------------------------------------
// FunctionConstantValuesAccessorTest — setConstantValue / getConstantValues / getData
// ---------------------------------------------------------------------------

TEST(FunctionConstantValuesAccessorTest, EmptyByDefault) {
  const FunctionConstantValues fcv;
  EXPECT_TRUE(fcv.getConstantValues().empty());
  EXPECT_TRUE(fcv.getData().empty());
}

TEST(FunctionConstantValuesAccessorTest, SetFloat1StoresCorrectData) {
  FunctionConstantValues fcv;
  const float val = 3.14f;
  fcv.setConstantValue(0, ConstantValueType::Float1, &val);

  const auto& entries = fcv.getConstantValues();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].type, ConstantValueType::Float1);

  const auto& data = fcv.getData();
  ASSERT_GE(data.size(), entries[0].offset + sizeof(float));
  float stored = 0.0f;
  std::memcpy(&stored, data.data() + entries[0].offset, sizeof(float));
  EXPECT_FLOAT_EQ(stored, val);
}

TEST(FunctionConstantValuesAccessorTest, SetInt1StoresCorrectData) {
  FunctionConstantValues fcv;
  const int32_t val = 42;
  fcv.setConstantValue(0, ConstantValueType::Int1, &val);

  const auto& entries = fcv.getConstantValues();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].type, ConstantValueType::Int1);

  const auto& data = fcv.getData();
  ASSERT_GE(data.size(), entries[0].offset + sizeof(int32_t));
  int32_t stored = 0;
  std::memcpy(&stored, data.data() + entries[0].offset, sizeof(int32_t));
  EXPECT_EQ(stored, val);
}

TEST(FunctionConstantValuesAccessorTest, SetBoolean1StoresCorrectData) {
  FunctionConstantValues fcv;
  const uint32_t val = 1;
  fcv.setConstantValue(0, ConstantValueType::Boolean1, &val);

  const auto& entries = fcv.getConstantValues();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].type, ConstantValueType::Boolean1);

  const auto& data = fcv.getData();
  ASSERT_GE(data.size(), entries[0].offset + sizeof(uint32_t));
  uint32_t stored = 0;
  std::memcpy(&stored, data.data() + entries[0].offset, sizeof(uint32_t));
  EXPECT_EQ(stored, val);
}

TEST(FunctionConstantValuesAccessorTest, SetFloat4StoresCorrectData) {
  FunctionConstantValues fcv;
  const float val[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  fcv.setConstantValue(0, ConstantValueType::Float4, val);

  const auto& entries = fcv.getConstantValues();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].type, ConstantValueType::Float4);

  const auto& data = fcv.getData();
  const size_t expectedSize = getConstantValueSize(ConstantValueType::Float4);
  ASSERT_GE(data.size(), entries[0].offset + expectedSize);
  EXPECT_EQ(std::memcmp(data.data() + entries[0].offset, val, expectedSize), 0);
}

TEST(FunctionConstantValuesAccessorTest, SetMat2x2StoresCorrectSize) {
  FunctionConstantValues fcv;
  const float val[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  fcv.setConstantValue(0, ConstantValueType::Mat2x2, val);

  const auto& entries = fcv.getConstantValues();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].type, ConstantValueType::Mat2x2);

  const size_t expectedSize = getConstantValueSize(ConstantValueType::Mat2x2);
  EXPECT_GE(fcv.getData().size(), entries[0].offset + expectedSize);
}

TEST(FunctionConstantValuesAccessorTest, OverwriteSameBindingUpdatesData) {
  FunctionConstantValues fcv;
  const float v1 = 1.0f;
  const float v2 = 2.0f;
  fcv.setConstantValue(0, ConstantValueType::Float1, &v1);
  fcv.setConstantValue(0, ConstantValueType::Float1, &v2);

  const auto& entries = fcv.getConstantValues();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].type, ConstantValueType::Float1);

  const auto& data = fcv.getData();
  ASSERT_GE(data.size(), entries[0].offset + sizeof(float));
  float stored = 0.0f;
  std::memcpy(&stored, data.data() + entries[0].offset, sizeof(float));
  EXPECT_FLOAT_EQ(stored, v2);
}

TEST(FunctionConstantValuesAccessorTest, MultipleBindingsCreateSeparateEntries) {
  FunctionConstantValues fcv;
  const float v0 = 1.0f;
  const int32_t v1 = 42;
  fcv.setConstantValue(0, ConstantValueType::Float1, &v0);
  fcv.setConstantValue(1, ConstantValueType::Int1, &v1);

  const auto& entries = fcv.getConstantValues();
  ASSERT_GE(entries.size(), 2u);
  EXPECT_EQ(entries[0].type, ConstantValueType::Float1);
  EXPECT_EQ(entries[1].type, ConstantValueType::Int1);
}

TEST(FunctionConstantValuesAccessorTest, GetConstantValuesReportsCorrectType) {
  FunctionConstantValues fcv;
  const float val[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  fcv.setConstantValue(0, ConstantValueType::Mat3x3, val);

  const auto& entries = fcv.getConstantValues();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].type, ConstantValueType::Mat3x3);
}

// ---------------------------------------------------------------------------
// ShaderCompilerOptionsEqualityTest
// ---------------------------------------------------------------------------

TEST(ShaderCompilerOptionsEqualityTest, SameFastMathAreEqual) {
  const ShaderCompilerOptions a{.fastMathEnabled = true};
  const ShaderCompilerOptions b{.fastMathEnabled = true};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(ShaderCompilerOptionsEqualityTest, DifferentFastMathAreNotEqual) {
  const ShaderCompilerOptions a{.fastMathEnabled = true};
  const ShaderCompilerOptions b{.fastMathEnabled = false};
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

// ---------------------------------------------------------------------------
// ShaderStagesTypeRangeTest
// ---------------------------------------------------------------------------

TEST(ShaderStagesTypeRangeTest, RenderMeshShaderIsDistinct) {
  EXPECT_NE(ShaderStagesType::RenderMeshShader, ShaderStagesType::Render);
  EXPECT_NE(ShaderStagesType::RenderMeshShader, ShaderStagesType::Compute);
}

} // namespace igl::tests
