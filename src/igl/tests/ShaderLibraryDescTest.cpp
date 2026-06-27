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
// ShaderStagesType
// ---------------------------------------------------------------------------

TEST(ShaderStagesTypeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(ShaderStagesType::Render), 0u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderStagesType::Compute), 1u);
}

// ---------------------------------------------------------------------------
// ShaderLibraryDesc
// ---------------------------------------------------------------------------

TEST(ShaderLibraryDescTest, DefaultConstruction) {
  const ShaderLibraryDesc desc;
  EXPECT_TRUE(desc.moduleInfo.empty());
  EXPECT_TRUE(desc.debugName.empty());
}

TEST(ShaderLibraryDescTest, FromStringInput) {
  const char* source = "void main() {}";
  const auto desc = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "testLib");
  EXPECT_FALSE(desc.moduleInfo.empty());
  EXPECT_EQ(desc.moduleInfo.size(), 1u);
  EXPECT_EQ(desc.moduleInfo[0].stage, ShaderStage::Vertex);
  EXPECT_EQ(desc.moduleInfo[0].entryPoint, "main");
  EXPECT_EQ(desc.debugName, "testLib");
  EXPECT_TRUE(desc.input.isValid());
  EXPECT_EQ(desc.input.type, ShaderInputType::String);
  EXPECT_EQ(desc.input.source, source);
}

TEST(ShaderLibraryDescTest, FromBinaryInput) {
  const uint8_t spirv[] = {0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00};
  const auto desc = ShaderLibraryDesc::fromBinaryInput(
      spirv, sizeof(spirv), {{.stage = ShaderStage::Compute, .entryPoint = "main"}}, "computeLib");
  EXPECT_EQ(desc.moduleInfo.size(), 1u);
  EXPECT_EQ(desc.moduleInfo[0].stage, ShaderStage::Compute);
  EXPECT_EQ(desc.debugName, "computeLib");
  EXPECT_TRUE(desc.input.isValid());
  EXPECT_EQ(desc.input.type, ShaderInputType::Binary);
  EXPECT_EQ(desc.input.data, spirv);
  EXPECT_EQ(desc.input.length, sizeof(spirv));
}

TEST(ShaderLibraryDescTest, EqualityReflexive) {
  const auto desc =
      ShaderLibraryDesc::fromStringInput("src", {{.stage = ShaderStage::Fragment}}, "lib");
  EXPECT_EQ(desc, desc);
}

TEST(ShaderLibraryDescTest, EqualitySameValues) {
  const char* source = "void main() {}";
  const auto a = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  const auto b = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  EXPECT_EQ(a, b);
}

TEST(ShaderLibraryDescTest, InequalityDifferentDebugName) {
  const char* source = "void main() {}";
  const auto a =
      ShaderLibraryDesc::fromStringInput(source, {{.stage = ShaderStage::Vertex}}, "libA");
  const auto b =
      ShaderLibraryDesc::fromStringInput(source, {{.stage = ShaderStage::Vertex}}, "libB");
  EXPECT_NE(a, b);
}

TEST(ShaderLibraryDescTest, MultipleModuleInfos) {
  const auto desc = ShaderLibraryDesc::fromStringInput(
      "src",
      {{.stage = ShaderStage::Vertex, .entryPoint = "vs_main"},
       {.stage = ShaderStage::Fragment, .entryPoint = "fs_main"}},
      "multiLib");
  EXPECT_EQ(desc.moduleInfo.size(), 2u);
  EXPECT_EQ(desc.moduleInfo[0].entryPoint, "vs_main");
  EXPECT_EQ(desc.moduleInfo[1].entryPoint, "fs_main");
}

// ---------------------------------------------------------------------------
// ShaderStagesDesc
// ---------------------------------------------------------------------------

TEST(ShaderStagesDescTest, DefaultConstruction) {
  const ShaderStagesDesc desc;
  EXPECT_EQ(desc.vertexModule, nullptr);
  EXPECT_EQ(desc.fragmentModule, nullptr);
  EXPECT_EQ(desc.computeModule, nullptr);
  EXPECT_EQ(desc.taskModule, nullptr);
  EXPECT_EQ(desc.meshModule, nullptr);
  EXPECT_EQ(desc.type, ShaderStagesType::Render);
}

} // namespace igl::tests
