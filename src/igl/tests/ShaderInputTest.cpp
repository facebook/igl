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
// ShaderCompilerOptionsHashTest
// ---------------------------------------------------------------------------

TEST(ShaderCompilerOptionsHashTest, Consistency) {
  const ShaderCompilerOptions opts;
  const std::hash<ShaderCompilerOptions> hasher;
  EXPECT_EQ(hasher(opts), hasher(opts));
}

TEST(ShaderCompilerOptionsHashTest, EqualObjectsHaveSameHash) {
  const ShaderCompilerOptions a{.fastMathEnabled = false};
  const ShaderCompilerOptions b{.fastMathEnabled = false};
  const std::hash<ShaderCompilerOptions> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(ShaderCompilerOptionsHashTest, DifferentObjectsHaveDifferentHash) {
  const ShaderCompilerOptions a{.fastMathEnabled = true};
  const ShaderCompilerOptions b{.fastMathEnabled = false};
  const std::hash<ShaderCompilerOptions> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

// ---------------------------------------------------------------------------
// ShaderModuleInfoHashTest
// ---------------------------------------------------------------------------

TEST(ShaderModuleInfoHashTest, Consistency) {
  const ShaderModuleInfo info{.stage = ShaderStage::Compute, .entryPoint = "main"};
  const std::hash<ShaderModuleInfo> hasher;
  EXPECT_EQ(hasher(info), hasher(info));
}

TEST(ShaderModuleInfoHashTest, EqualObjectsHaveSameHash) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const std::hash<ShaderModuleInfo> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(ShaderModuleInfoHashTest, DifferentStageProducesDifferentHash) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Fragment, .entryPoint = "main"};
  const std::hash<ShaderModuleInfo> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(ShaderModuleInfoHashTest, DifferentEntryPointProducesDifferentHash) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "other"};
  const std::hash<ShaderModuleInfo> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(ShaderModuleInfoHashTest, DifferentFunctionConstantsProducesDifferentHash) {
  const float value = 1.0f;
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  b.functionConstantValues.setConstantValue(0, ConstantValueType::Float1, &value);
  const std::hash<ShaderModuleInfo> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

// ---------------------------------------------------------------------------
// ShaderInputHashTest
// ---------------------------------------------------------------------------

TEST(ShaderInputHashTest, Consistency) {
  const char* source = "void main() {}";
  const ShaderInput input{.source = source, .type = ShaderInputType::String};
  const std::hash<ShaderInput> hasher;
  EXPECT_EQ(hasher(input), hasher(input));
}

TEST(ShaderInputHashTest, EqualStringInputsHaveSameHash) {
  const char* source = "void main() {}";
  const ShaderInput a{.source = source, .type = ShaderInputType::String};
  const ShaderInput b{.source = source, .type = ShaderInputType::String};
  const std::hash<ShaderInput> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(ShaderInputHashTest, DifferentTypeProducesDifferentHash) {
  const ShaderInput a{.type = ShaderInputType::String};
  const ShaderInput b{.type = ShaderInputType::Binary};
  const std::hash<ShaderInput> hasher;
  EXPECT_NE(hasher(a), hasher(b));
}

// ---------------------------------------------------------------------------
// ShaderModuleDescHashTest
// ---------------------------------------------------------------------------

TEST(ShaderModuleDescHashTest, Consistency) {
  const char* source = "void main() {}";
  const auto desc = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "vs");
  const std::hash<ShaderModuleDesc> hasher;
  EXPECT_EQ(hasher(desc), hasher(desc));
}

TEST(ShaderModuleDescHashTest, EqualObjectsHaveSameHash) {
  const char* source = "void main() {}";
  const auto a = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "vs");
  const auto b = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "vs");
  const std::hash<ShaderModuleDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(ShaderModuleDescHashTest, DifferentDebugNameProducesDifferentHash) {
  const char* source = "void main() {}";
  const auto a = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "nameA");
  const auto b = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "nameB");
  const std::hash<ShaderModuleDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(ShaderModuleDescHashTest, DifferentStageProducesDifferentHash) {
  const char* source = "void main() {}";
  const auto a = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "shader");
  const auto b = ShaderModuleDesc::fromStringInput(
      source, {.stage = ShaderStage::Fragment, .entryPoint = "main"}, "shader");
  const std::hash<ShaderModuleDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

// ---------------------------------------------------------------------------
// ShaderLibraryDescHashTest
// ---------------------------------------------------------------------------

TEST(ShaderLibraryDescHashTest, Consistency) {
  const auto desc = ShaderLibraryDesc::fromStringInput(
      "void main() {}", {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  const std::hash<ShaderLibraryDesc> hasher;
  EXPECT_EQ(hasher(desc), hasher(desc));
}

TEST(ShaderLibraryDescHashTest, EqualObjectsHaveSameHash) {
  const char* source = "void main() {}";
  const auto a = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  const auto b = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "lib");
  const std::hash<ShaderLibraryDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(ShaderLibraryDescHashTest, DifferentDebugNameProducesDifferentHash) {
  const char* source = "void main() {}";
  const auto a = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "libA");
  const auto b = ShaderLibraryDesc::fromStringInput(
      source, {{.stage = ShaderStage::Vertex, .entryPoint = "main"}}, "libB");
  const std::hash<ShaderLibraryDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

// ---------------------------------------------------------------------------
// FunctionConstantValuesHashTest
// ---------------------------------------------------------------------------

TEST(FunctionConstantValuesHashTest, Consistency) {
  FunctionConstantValues fcv;
  const float value = 3.14f;
  fcv.setConstantValue(0, ConstantValueType::Float1, &value);
  const std::hash<FunctionConstantValues> hasher;
  EXPECT_EQ(hasher(fcv), hasher(fcv));
}

TEST(FunctionConstantValuesHashTest, EqualObjectsHaveSameHash) {
  FunctionConstantValues a;
  FunctionConstantValues b;
  const float value = 3.14f;
  a.setConstantValue(0, ConstantValueType::Float1, &value);
  b.setConstantValue(0, ConstantValueType::Float1, &value);
  const std::hash<FunctionConstantValues> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(FunctionConstantValuesHashTest, DifferentValuesProduceDifferentHash) {
  FunctionConstantValues a;
  FunctionConstantValues b;
  const float v1 = 1.0f;
  const float v2 = 2.0f;
  a.setConstantValue(0, ConstantValueType::Float1, &v1);
  b.setConstantValue(0, ConstantValueType::Float1, &v2);
  const std::hash<FunctionConstantValues> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(FunctionConstantValuesHashTest, DifferentTypesProduceDifferentHash) {
  FunctionConstantValues a;
  FunctionConstantValues b;
  const uint32_t value = 42;
  a.setConstantValue(0, ConstantValueType::Int1, &value);
  b.setConstantValue(0, ConstantValueType::Float1, &value);
  const std::hash<FunctionConstantValues> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(FunctionConstantValuesHashTest, DifferentBindingsProduceDifferentHash) {
  FunctionConstantValues a;
  FunctionConstantValues b;
  const float value = 1.0f;
  a.setConstantValue(0, ConstantValueType::Float1, &value);
  b.setConstantValue(1, ConstantValueType::Float1, &value);
  const std::hash<FunctionConstantValues> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(FunctionConstantValuesHashTest, EmptyObjectsHaveSameHash) {
  const FunctionConstantValues a;
  const FunctionConstantValues b;
  const std::hash<FunctionConstantValues> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

} // namespace igl::tests
