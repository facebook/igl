/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <igl/Shader.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// ShaderModuleDesc
// ---------------------------------------------------------------------------

TEST(ShaderModuleDescTest, DefaultConstruction) {
  const ShaderModuleDesc desc;
  EXPECT_EQ(desc.info.stage, ShaderStage::Fragment);
  EXPECT_TRUE(desc.info.entryPoint.empty());
  EXPECT_EQ(desc.input.source, nullptr);
  EXPECT_EQ(desc.input.data, nullptr);
  EXPECT_EQ(desc.input.length, 0u);
  EXPECT_EQ(desc.input.type, ShaderInputType::String);
  EXPECT_TRUE(desc.debugName.empty());
}

TEST(ShaderModuleDescTest, FromStringInputPopulatesFields) {
  const char* source = "void main() {}";
  const ShaderModuleDesc desc = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "vertShader");

  EXPECT_EQ(desc.input.type, ShaderInputType::String);
  EXPECT_EQ(desc.input.source, source);
  EXPECT_EQ(desc.input.data, nullptr);
  EXPECT_EQ(desc.input.length, 0u);
  EXPECT_EQ(desc.info.stage, ShaderStage::Vertex);
  EXPECT_EQ(desc.info.entryPoint, "main");
  EXPECT_EQ(desc.debugName, "vertShader");
}

TEST(ShaderModuleDescTest, FromBinaryInputPopulatesFields) {
  const uint8_t binary[] = {0xDE, 0xAD, 0xBE, 0xEF};
  const ShaderModuleDesc desc = ShaderModuleDesc::fromBinaryInput(
      binary, sizeof(binary), {.stage = ShaderStage::Fragment, .entryPoint = "frag"}, "fragShader");

  EXPECT_EQ(desc.input.type, ShaderInputType::Binary);
  EXPECT_EQ(desc.input.data, binary);
  EXPECT_EQ(desc.input.length, sizeof(binary));
  EXPECT_EQ(desc.input.source, nullptr);
  EXPECT_EQ(desc.info.stage, ShaderStage::Fragment);
  EXPECT_EQ(desc.info.entryPoint, "frag");
  EXPECT_EQ(desc.debugName, "fragShader");
}

TEST(ShaderModuleDescTest, EqualityReflexive) {
  const ShaderModuleDesc desc = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  EXPECT_EQ(desc, desc);
}

TEST(ShaderModuleDescTest, EqualitySameValues) {
  const ShaderModuleDesc a = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  const ShaderModuleDesc b = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  EXPECT_EQ(a, b);
}

TEST(ShaderModuleDescTest, InequalityByDebugName) {
  const ShaderModuleDesc a = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "nameA");
  const ShaderModuleDesc b = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "nameB");
  EXPECT_NE(a, b);
}

TEST(ShaderModuleDescTest, InequalityByStage) {
  const ShaderModuleDesc a = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  const ShaderModuleDesc b = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Fragment, .entryPoint = "main"}, "debug");
  EXPECT_NE(a, b);
}

TEST(ShaderModuleDescTest, InequalityByEntryPoint) {
  const ShaderModuleDesc a = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "vertexMain"}, "debug");
  const ShaderModuleDesc b = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "otherMain"}, "debug");
  EXPECT_NE(a, b);
}

TEST(ShaderModuleDescTest, InequalityByInputSource) {
  const ShaderModuleDesc a = ShaderModuleDesc::fromStringInput(
      "sourceA", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  const ShaderModuleDesc b = ShaderModuleDesc::fromStringInput(
      "sourceB", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  EXPECT_NE(a, b);
}

TEST(ShaderModuleDescTest, InequalityByInputType) {
  const uint8_t binary[] = {0x01, 0x02, 0x03, 0x04};
  const ShaderModuleDesc str = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  const ShaderModuleDesc bin = ShaderModuleDesc::fromBinaryInput(
      binary, sizeof(binary), {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  EXPECT_NE(str, bin);
}

TEST(ShaderModuleDescTest, InequalityByFunctionConstantValues) {
  ShaderModuleDesc a = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  ShaderModuleDesc b = a;
  const int32_t value = 1;
  b.info.functionConstantValues.setConstantValue(0, ConstantValueType::Int1, &value);
  EXPECT_NE(a, b);
}

TEST(ShaderModuleDescTest, InequalityByInputOptions) {
  ShaderModuleDesc a = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  ShaderModuleDesc b = a;
  b.input.options.fastMathEnabled = !a.input.options.fastMathEnabled;
  EXPECT_NE(a, b);
}

TEST(ShaderModuleDescTest, InequalityByInputDataAndLength) {
  const uint8_t binaryA[] = {0x01, 0x02, 0x03, 0x04};
  const uint8_t binaryB[] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E};
  const ShaderModuleDesc a = ShaderModuleDesc::fromBinaryInput(
      binaryA, sizeof(binaryA), {.stage = ShaderStage::Fragment, .entryPoint = "frag"}, "debug");
  const ShaderModuleDesc b = ShaderModuleDesc::fromBinaryInput(
      binaryB, sizeof(binaryB), {.stage = ShaderStage::Fragment, .entryPoint = "frag"}, "debug");
  // Differs in both input.data pointer and input.length.
  EXPECT_NE(a, b);
}

TEST(ShaderModuleDescTest, HashEqualObjectsHaveSameHash) {
  const ShaderModuleDesc a = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  const ShaderModuleDesc b = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debug");
  const std::hash<ShaderModuleDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

} // namespace igl::tests
