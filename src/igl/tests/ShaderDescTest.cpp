/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Shader.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// ShaderCompilerOptions
// ---------------------------------------------------------------------------

TEST(ShaderCompilerOptionsTest, DefaultConstructionFastMathEnabled) {
  const ShaderCompilerOptions opts;
  EXPECT_TRUE(opts.fastMathEnabled);
}

TEST(ShaderCompilerOptionsTest, EqualityReflexive) {
  const ShaderCompilerOptions opts;
  EXPECT_EQ(opts, opts);
}

TEST(ShaderCompilerOptionsTest, EqualitySameValues) {
  const ShaderCompilerOptions a;
  const ShaderCompilerOptions b;
  EXPECT_EQ(a, b);
}

TEST(ShaderCompilerOptionsTest, InequalityDifferentFastMath) {
  ShaderCompilerOptions a;
  ShaderCompilerOptions b;
  b.fastMathEnabled = false;
  EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// ShaderInput
// ---------------------------------------------------------------------------

TEST(ShaderInputTest, DefaultConstruction) {
  const ShaderInput input;
  EXPECT_EQ(input.source, nullptr);
  EXPECT_EQ(input.data, nullptr);
  EXPECT_EQ(input.length, 0u);
  EXPECT_EQ(input.type, ShaderInputType::String);
}

TEST(ShaderInputTest, InvalidWhenDefaultConstructed) {
  const ShaderInput input;
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputTest, ValidStringInput) {
  ShaderInput input;
  input.type = ShaderInputType::String;
  input.source = "void main() {}";
  EXPECT_TRUE(input.isValid());
}

TEST(ShaderInputTest, InvalidStringInputWithData) {
  ShaderInput input;
  input.type = ShaderInputType::String;
  input.source = "void main() {}";
  const uint8_t dummy = 0;
  input.data = &dummy;
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputTest, InvalidStringInputWithLength) {
  ShaderInput input;
  input.type = ShaderInputType::String;
  input.source = "void main() {}";
  input.length = 10;
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputTest, ValidBinaryInput) {
  const uint8_t spirv[] = {0x03, 0x02, 0x23, 0x07};
  ShaderInput input;
  input.type = ShaderInputType::Binary;
  input.data = spirv;
  input.length = sizeof(spirv);
  EXPECT_TRUE(input.isValid());
}

TEST(ShaderInputTest, InvalidBinaryInputNoData) {
  ShaderInput input;
  input.type = ShaderInputType::Binary;
  input.length = 4;
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputTest, InvalidBinaryInputZeroLength) {
  const uint8_t spirv[] = {0x03, 0x02, 0x23, 0x07};
  ShaderInput input;
  input.type = ShaderInputType::Binary;
  input.data = spirv;
  input.length = 0;
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputTest, InvalidBinaryInputWithSource) {
  const uint8_t spirv[] = {0x03, 0x02, 0x23, 0x07};
  ShaderInput input;
  input.type = ShaderInputType::Binary;
  input.data = spirv;
  input.length = sizeof(spirv);
  input.source = "should not be set";
  EXPECT_FALSE(input.isValid());
}

TEST(ShaderInputTest, EqualityReflexive) {
  ShaderInput input;
  input.type = ShaderInputType::String;
  input.source = "test";
  EXPECT_EQ(input, input);
}

TEST(ShaderInputTest, EqualitySameValues) {
  ShaderInput a;
  a.type = ShaderInputType::String;
  a.source = "test";
  ShaderInput b;
  b.type = ShaderInputType::String;
  b.source = "test";
  EXPECT_EQ(a, b);
}

TEST(ShaderInputTest, InequalityDifferentType) {
  ShaderInput a;
  a.type = ShaderInputType::String;
  a.source = "test";
  ShaderInput b;
  b.type = ShaderInputType::Binary;
  EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// ShaderModuleInfo
// ---------------------------------------------------------------------------

TEST(ShaderModuleInfoTest, DefaultConstruction) {
  const ShaderModuleInfo info;
  EXPECT_EQ(info.stage, ShaderStage::Fragment);
  EXPECT_TRUE(info.entryPoint.empty());
  EXPECT_TRUE(info.debugName.empty());
}

TEST(ShaderModuleInfoTest, EqualityReflexive) {
  const ShaderModuleInfo info;
  EXPECT_EQ(info, info);
}

TEST(ShaderModuleInfoTest, InequalityDifferentStage) {
  ShaderModuleInfo a;
  ShaderModuleInfo b;
  b.stage = ShaderStage::Vertex;
  EXPECT_NE(a, b);
}

TEST(ShaderModuleInfoTest, InequalityDifferentEntryPoint) {
  ShaderModuleInfo a;
  ShaderModuleInfo b;
  b.entryPoint = "main0";
  EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// ShaderStage enum
// ---------------------------------------------------------------------------

TEST(ShaderStageTest, EnumValuesCoreStages) {
  EXPECT_EQ(static_cast<uint8_t>(ShaderStage::Vertex), 0u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderStage::Fragment), 1u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderStage::Compute), 2u);
}

// ---------------------------------------------------------------------------
// ShaderInputType enum
// ---------------------------------------------------------------------------

TEST(ShaderInputTypeTest, EnumValuesStringBinary) {
  EXPECT_EQ(static_cast<uint8_t>(ShaderInputType::String), 0u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderInputType::Binary), 1u);
}

} // namespace igl::tests
