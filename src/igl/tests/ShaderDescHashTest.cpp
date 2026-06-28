/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <functional>
#include <igl/Shader.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// ShaderCompilerOptions — defaults and hashing
// ---------------------------------------------------------------------------

TEST(ShaderCompilerOptionsHashTest, DefaultFastMathEnabled) {
  const ShaderCompilerOptions opts;
  EXPECT_TRUE(opts.fastMathEnabled);
}

TEST(ShaderCompilerOptionsHashTest, HashConsistency) {
  const ShaderCompilerOptions opts;
  const size_t h1 = std::hash<ShaderCompilerOptions>()(opts);
  const size_t h2 = std::hash<ShaderCompilerOptions>()(opts);
  EXPECT_EQ(h1, h2);
}

TEST(ShaderCompilerOptionsHashTest, EqualObjectsEqualHash) {
  const ShaderCompilerOptions a;
  const ShaderCompilerOptions b;
  EXPECT_EQ(a, b);
  EXPECT_EQ(std::hash<ShaderCompilerOptions>()(a), std::hash<ShaderCompilerOptions>()(b));
}

TEST(ShaderCompilerOptionsHashTest, DifferentFastMathDifferentHash) {
  ShaderCompilerOptions a;
  ShaderCompilerOptions b;
  b.fastMathEnabled = false;
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderCompilerOptions>()(a), std::hash<ShaderCompilerOptions>()(b));
}

// ---------------------------------------------------------------------------
// ShaderModuleInfo — defaults and hashing
// ---------------------------------------------------------------------------

TEST(ShaderModuleInfoHashTest, DefaultStageIsFragment) {
  const ShaderModuleInfo info;
  EXPECT_EQ(info.stage, ShaderStage::Fragment);
}

TEST(ShaderModuleInfoHashTest, DefaultEntryPointEmpty) {
  const ShaderModuleInfo info;
  EXPECT_TRUE(info.entryPoint.empty());
}

TEST(ShaderModuleInfoHashTest, HashConsistency) {
  const ShaderModuleInfo info{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const size_t h1 = std::hash<ShaderModuleInfo>()(info);
  const size_t h2 = std::hash<ShaderModuleInfo>()(info);
  EXPECT_EQ(h1, h2);
}

TEST(ShaderModuleInfoHashTest, EqualObjectsEqualHash) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  EXPECT_EQ(a, b);
  EXPECT_EQ(std::hash<ShaderModuleInfo>()(a), std::hash<ShaderModuleInfo>()(b));
}

TEST(ShaderModuleInfoHashTest, DifferentStageDifferentHash) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Fragment, .entryPoint = "main"};
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderModuleInfo>()(a), std::hash<ShaderModuleInfo>()(b));
}

TEST(ShaderModuleInfoHashTest, DifferentEntryPointDifferentHash) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "other"};
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderModuleInfo>()(a), std::hash<ShaderModuleInfo>()(b));
}

TEST(ShaderModuleInfoHashTest, FunctionConstantsAffectHash) {
  ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const float val = 1.0f;
  b.functionConstantValues.setConstantValue(0, ConstantValueType::Float1, &val);
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderModuleInfo>()(a), std::hash<ShaderModuleInfo>()(b));
}

// ---------------------------------------------------------------------------
// ShaderInput — defaults and hashing
// ---------------------------------------------------------------------------

TEST(ShaderInputHashTest, DefaultValues) {
  const ShaderInput input;
  EXPECT_EQ(input.source, nullptr);
  EXPECT_EQ(input.data, nullptr);
  EXPECT_EQ(input.length, 0u);
  EXPECT_EQ(input.type, ShaderInputType::String);
}

TEST(ShaderInputHashTest, HashConsistency) {
  const char* source = "void main() {}";
  const ShaderInput input{.source = source, .type = ShaderInputType::String};
  const size_t h1 = std::hash<ShaderInput>()(input);
  const size_t h2 = std::hash<ShaderInput>()(input);
  EXPECT_EQ(h1, h2);
}

TEST(ShaderInputHashTest, EqualStringInputsEqualHash) {
  const char* source = "void main() {}";
  const ShaderInput a{.source = source, .type = ShaderInputType::String};
  const ShaderInput b{.source = source, .type = ShaderInputType::String};
  EXPECT_EQ(a, b);
  EXPECT_EQ(std::hash<ShaderInput>()(a), std::hash<ShaderInput>()(b));
}

TEST(ShaderInputHashTest, DifferentSourceDifferentHash) {
  const ShaderInput a{.source = "void main() {}", .type = ShaderInputType::String};
  const ShaderInput b{.source = "void other() {}", .type = ShaderInputType::String};
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderInput>()(a), std::hash<ShaderInput>()(b));
}

TEST(ShaderInputHashTest, EqualBinaryInputsEqualHash) {
  const uint32_t blobA = 0x12345678;
  const uint32_t blobB = 0x12345678;
  const ShaderInput a{.data = &blobA, .length = sizeof(blobA), .type = ShaderInputType::Binary};
  const ShaderInput b{.data = &blobB, .length = sizeof(blobB), .type = ShaderInputType::Binary};
  EXPECT_EQ(a, b);
  EXPECT_EQ(std::hash<ShaderInput>()(a), std::hash<ShaderInput>()(b));
}

TEST(ShaderInputHashTest, DifferentBinaryDataDifferentHash) {
  const uint32_t blobA = 0x12345678;
  const uint32_t blobB = 0xAABBCCDD;
  const ShaderInput a{.data = &blobA, .length = sizeof(blobA), .type = ShaderInputType::Binary};
  const ShaderInput b{.data = &blobB, .length = sizeof(blobB), .type = ShaderInputType::Binary};
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderInput>()(a), std::hash<ShaderInput>()(b));
}

TEST(ShaderInputHashTest, DifferentOptionsNotEqual) {
  ShaderCompilerOptions optsA;
  optsA.fastMathEnabled = true;
  ShaderCompilerOptions optsB;
  optsB.fastMathEnabled = false;
  const ShaderInput a{
      .source = "void main() {}", .options = optsA, .type = ShaderInputType::String};
  const ShaderInput b{
      .source = "void main() {}", .options = optsB, .type = ShaderInputType::String};
  EXPECT_NE(a, b);
}

// ---------------------------------------------------------------------------
// ShaderModuleDesc — fromBinaryInput fields and hashing
// ---------------------------------------------------------------------------

TEST(ShaderModuleDescHashTest, FromBinaryInputFields) {
  const uint8_t spirv[] = {0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00};
  const auto desc = ShaderModuleDesc::fromBinaryInput(
      spirv, sizeof(spirv), {.stage = ShaderStage::Compute, .entryPoint = "main"}, "computeShader");
  EXPECT_EQ(desc.info.stage, ShaderStage::Compute);
  EXPECT_EQ(desc.info.entryPoint, "main");
  EXPECT_EQ(desc.debugName, "computeShader");
  EXPECT_TRUE(desc.input.isValid());
  EXPECT_EQ(desc.input.type, ShaderInputType::Binary);
  EXPECT_EQ(desc.input.data, spirv);
  EXPECT_EQ(desc.input.length, sizeof(spirv));
}

TEST(ShaderModuleDescHashTest, FromStringInputFields) {
  const char* source = "void main() {}";
  const auto desc = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "vertShader");
  EXPECT_EQ(desc.info.stage, ShaderStage::Vertex);
  EXPECT_EQ(desc.info.entryPoint, "main");
  EXPECT_EQ(desc.debugName, "vertShader");
  EXPECT_TRUE(desc.input.isValid());
  EXPECT_EQ(desc.input.type, ShaderInputType::String);
  EXPECT_EQ(desc.input.source, source);
}

TEST(ShaderModuleDescHashTest, HashConsistency) {
  const auto desc = ShaderModuleDesc::fromStringInput(
      "src", {.stage = ShaderStage::Fragment, .entryPoint = "main"}, "frag");
  const size_t h1 = std::hash<ShaderModuleDesc>()(desc);
  const size_t h2 = std::hash<ShaderModuleDesc>()(desc);
  EXPECT_EQ(h1, h2);
}

TEST(ShaderModuleDescHashTest, EqualDescsEqualHash) {
  const char* source = "void main() {}";
  const auto a = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "shader");
  const auto b = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "shader");
  EXPECT_EQ(a, b);
  EXPECT_EQ(std::hash<ShaderModuleDesc>()(a), std::hash<ShaderModuleDesc>()(b));
}

TEST(ShaderModuleDescHashTest, DifferentDebugNameDifferentHash) {
  const char* source = "void main() {}";
  const auto a = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "shaderA");
  const auto b = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "shaderB");
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderModuleDesc>()(a), std::hash<ShaderModuleDesc>()(b));
}

TEST(ShaderModuleDescHashTest, DifferentStageDifferentHash) {
  const char* source = "void main() {}";
  const auto a = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "shader");
  const auto b = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Fragment, .entryPoint = "main"}, "shader");
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderModuleDesc>()(a), std::hash<ShaderModuleDesc>()(b));
}

TEST(ShaderModuleDescHashTest, DifferentInputDifferentHash) {
  const auto a = ShaderModuleDesc::fromStringInput(
      "source A", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "shader");
  const auto b = ShaderModuleDesc::fromStringInput(
      "source B", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "shader");
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderModuleDesc>()(a), std::hash<ShaderModuleDesc>()(b));
}

// ---------------------------------------------------------------------------
// ShaderLibraryDesc — hashing
// ---------------------------------------------------------------------------

TEST(ShaderLibraryDescHashTest, HashConsistency) {
  const auto desc = ShaderLibraryDesc::fromStringInput(
      "src", {{.stage = ShaderStage::Vertex, .entryPoint = "vs"}}, "lib");
  const size_t h1 = std::hash<ShaderLibraryDesc>()(desc);
  const size_t h2 = std::hash<ShaderLibraryDesc>()(desc);
  EXPECT_EQ(h1, h2);
}

TEST(ShaderLibraryDescHashTest, EqualDescsEqualHash) {
  const char* source = "void main() {}";
  const auto a = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  const auto b = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  EXPECT_EQ(a, b);
  EXPECT_EQ(std::hash<ShaderLibraryDesc>()(a), std::hash<ShaderLibraryDesc>()(b));
}

TEST(ShaderLibraryDescHashTest, DifferentDebugNameDifferentHash) {
  const char* source = "void main() {}";
  const auto a = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "libA");
  const auto b = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "libB");
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderLibraryDesc>()(a), std::hash<ShaderLibraryDesc>()(b));
}

TEST(ShaderLibraryDescHashTest, DifferentModuleInfoDifferentHash) {
  const char* source = "void main() {}";
  const auto a = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  const auto b = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Fragment, .entryPoint = "main"}}, "lib");
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderLibraryDesc>()(a), std::hash<ShaderLibraryDesc>()(b));
}

TEST(ShaderLibraryDescHashTest, DifferentInputDifferentHash) {
  const auto a = ShaderLibraryDesc::fromStringInput(
      "source A", {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  const auto b = ShaderLibraryDesc::fromStringInput(
      "source B", {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<ShaderLibraryDesc>()(a), std::hash<ShaderLibraryDesc>()(b));
}

// ---------------------------------------------------------------------------
// FunctionConstantValues — hashing
// ---------------------------------------------------------------------------

TEST(FunctionConstantValuesHashTest, EmptyHashConsistency) {
  const FunctionConstantValues fcv;
  const size_t h1 = std::hash<FunctionConstantValues>()(fcv);
  const size_t h2 = std::hash<FunctionConstantValues>()(fcv);
  EXPECT_EQ(h1, h2);
}

TEST(FunctionConstantValuesHashTest, EqualContentEqualHash) {
  FunctionConstantValues a;
  FunctionConstantValues b;
  const float val = 3.14f;
  a.setConstantValue(0, ConstantValueType::Float1, &val);
  b.setConstantValue(0, ConstantValueType::Float1, &val);
  EXPECT_EQ(a, b);
  EXPECT_EQ(std::hash<FunctionConstantValues>()(a), std::hash<FunctionConstantValues>()(b));
}

TEST(FunctionConstantValuesHashTest, DifferentValueDifferentHash) {
  FunctionConstantValues a;
  FunctionConstantValues b;
  const float v1 = 1.0f;
  const float v2 = 2.0f;
  a.setConstantValue(0, ConstantValueType::Float1, &v1);
  b.setConstantValue(0, ConstantValueType::Float1, &v2);
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<FunctionConstantValues>()(a), std::hash<FunctionConstantValues>()(b));
}

TEST(FunctionConstantValuesHashTest, DifferentBindingDifferentHash) {
  FunctionConstantValues a;
  FunctionConstantValues b;
  const int32_t val = 42;
  a.setConstantValue(0, ConstantValueType::Int1, &val);
  b.setConstantValue(1, ConstantValueType::Int1, &val);
  EXPECT_NE(a, b);
  EXPECT_NE(std::hash<FunctionConstantValues>()(a), std::hash<FunctionConstantValues>()(b));
}

TEST(FunctionConstantValuesHashTest, GapBytesIgnoredInHash) {
  FunctionConstantValues a;
  FunctionConstantValues b;
  const float vFloat1 = 7.0f;
  const float vFloat4[4] = {1.0f, 2.0f, 3.0f, 4.0f};

  a.setConstantValue(0, ConstantValueType::Float4, &vFloat4);

  b.setConstantValue(0, ConstantValueType::Float1, &vFloat1);
  b.setConstantValue(0, ConstantValueType::Float4, &vFloat4);

  EXPECT_EQ(a, b);
  EXPECT_EQ(std::hash<FunctionConstantValues>()(a), std::hash<FunctionConstantValues>()(b));
}

// ---------------------------------------------------------------------------
// ShaderInputType — enum values
// ---------------------------------------------------------------------------

TEST(ShaderInputTypeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(ShaderInputType::String), 0u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderInputType::Binary), 1u);
}

TEST(ShaderInputTypeTest, ValuesAreDistinct) {
  EXPECT_NE(ShaderInputType::String, ShaderInputType::Binary);
}

} // namespace igl::tests
