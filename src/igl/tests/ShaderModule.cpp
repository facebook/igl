/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "data/ShaderData.h"
#include "util/Common.h"

#include <igl/Shader.h>
#include <igl/ShaderCreator.h>

namespace igl::tests {

class ShaderModuleTest : public ::testing::Test {
 private:
 public:
  ShaderModuleTest() = default;
  ~ShaderModuleTest() override = default;

  //
  // SetUp()
  //
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  const std::string backend_ = IGL_BACKEND_TYPE;
};

//
// CompileShaderModuleReturnNull
//
// This test makes sure that when an invalid shader is given,
// compileShaderModule() gives back a nullptr. We had cases where even
// with an invalid shader, the module still returns a partially
// initialized object.
//
TEST_F(ShaderModuleTest, CompileShaderModuleReturnNull) {
  Result ret;

  // Vulkan backend has hard coded asserts that we cannot get past.
  // Manually verified that it will assert if this test were to go
  // through, and therefore it's catching the failure.
  if (backend_ == util::kBackendVul) {
    return;
  }

  auto shaderModule =
      ShaderModuleCreator::fromStringInput(*iglDev_,
                                           "hello world",
                                           {.stage = ShaderStage::Vertex, .entryPoint = "Mordor"},
                                           "test",
                                           &ret);
  ASSERT_TRUE(!ret.isOk());
  ASSERT_TRUE(shaderModule == nullptr);
}

TEST_F(ShaderModuleTest, CompileShaderModuleReturnNullWithEmptyInput) {
  Result ret;

  auto shaderModule = ShaderModuleCreator::fromStringInput(
      *iglDev_, "", {.stage = ShaderStage::Vertex, .entryPoint = ""}, "test", &ret);
  ASSERT_TRUE(!ret.isOk());
  ASSERT_TRUE(shaderModule == nullptr);
}

TEST_F(ShaderModuleTest, CompileShaderModule) {
  Result ret;

  const char* source = nullptr;

  const auto be = iglDev_->getBackendType();
  if (be == BackendType::OpenGL) {
    source = data::shader::kOglSimpleVertShader.data();
  } else if (be == BackendType::Metal) {
    source = data::shader::kMtlSimpleShader.data();
  } else if (be == BackendType::Vulkan) {
    source = data::shader::kVulkanSimpleVertShader.data();
  } else if (be == BackendType::D3D12) {
    // Minimal HLSL vertex shader for D3D12 backend
    source = R"(
struct VSIn { float4 position_in : POSITION; float2 uv_in : TEXCOORD0; };
struct VSOut { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut vertexShader(VSIn i) { VSOut o; o.position = i.position_in; o.uv = i.uv_in; return o; }
VSOut main(VSIn i) { return vertexShader(i); }
)";
  } else {
    ASSERT_TRUE(0);
  }

  auto shaderModule = ShaderModuleCreator::fromStringInput(
      *iglDev_,
      source,
      {.stage = ShaderStage::Vertex,
       .entryPoint = (be == BackendType::D3D12) ? std::string("main")
                                                : std::string("vertexShader")},
      "test",
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(shaderModule != nullptr);
}

TEST_F(ShaderModuleTest, ShaderCompilerOptionsEquality) {
  ShaderCompilerOptions options1;
  ShaderCompilerOptions options2;

  // Default values should be equal
  EXPECT_TRUE(options1 == options2);
  EXPECT_FALSE(options1 != options2);

  // Modify one and they should be unequal
  options2.fastMathEnabled = false;
  EXPECT_FALSE(options1 == options2);
  EXPECT_TRUE(options1 != options2);

  // Make them equal again
  options1.fastMathEnabled = false;
  EXPECT_TRUE(options1 == options2);
  EXPECT_FALSE(options1 != options2);
}

TEST_F(ShaderModuleTest, ShaderModuleDescEquality) {
  ShaderModuleDesc desc1 = ShaderModuleDesc::fromStringInput(
      "test source", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debugName");
  ShaderModuleDesc desc2 = ShaderModuleDesc::fromStringInput(
      "test source", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "debugName");

  // Same content should be equal
  EXPECT_TRUE(desc1 == desc2);
  EXPECT_FALSE(desc1 != desc2);

  // Different debug name should be unequal
  ShaderModuleDesc desc3 = ShaderModuleDesc::fromStringInput(
      "test source", {.stage = ShaderStage::Vertex, .entryPoint = "main"}, "differentDebugName");
  EXPECT_FALSE(desc1 == desc3);
  EXPECT_TRUE(desc1 != desc3);
}

TEST_F(ShaderModuleTest, ShaderLibraryDescEquality) {
  std::vector<ShaderModuleInfo> moduleInfo1 = {
      {.stage = ShaderStage::Vertex, .entryPoint = "vertMain"}};
  std::vector<ShaderModuleInfo> moduleInfo2 = {
      {.stage = ShaderStage::Vertex, .entryPoint = "vertMain"}};

  ShaderLibraryDesc desc1 =
      ShaderLibraryDesc::fromStringInput("test source", moduleInfo1, "debugName");
  ShaderLibraryDesc desc2 =
      ShaderLibraryDesc::fromStringInput("test source", moduleInfo2, "debugName");

  // Same content should be equal
  EXPECT_TRUE(desc1 == desc2);
  EXPECT_FALSE(desc1 != desc2);

  // Different debug name should be unequal
  ShaderLibraryDesc desc3 =
      ShaderLibraryDesc::fromStringInput("test source", moduleInfo1, "differentDebugName");
  EXPECT_FALSE(desc1 == desc3);
  EXPECT_TRUE(desc1 != desc3);
}

TEST(ShaderInputTest, IsValidStringInput) {
  const char* source = "some shader source";
  const ShaderInput input{.source = source, .type = ShaderInputType::String};
  EXPECT_TRUE(input.isValid());

  // A string input without a source is invalid.
  const ShaderInput noSource{.type = ShaderInputType::String};
  EXPECT_FALSE(noSource.isValid());

  // A string input that also carries binary data is invalid.
  const uint32_t blob = 0xDEADBEEF;
  const ShaderInput mixed{
      .source = source, .data = &blob, .length = sizeof(blob), .type = ShaderInputType::String};
  EXPECT_FALSE(mixed.isValid());
}

TEST(ShaderInputTest, IsValidBinaryInput) {
  const uint32_t blob = 0xDEADBEEF;
  const ShaderInput input{.data = &blob, .length = sizeof(blob), .type = ShaderInputType::Binary};
  EXPECT_TRUE(input.isValid());

  // A binary input without data is invalid.
  const ShaderInput noData{.length = sizeof(blob), .type = ShaderInputType::Binary};
  EXPECT_FALSE(noData.isValid());

  // A binary input with zero length is invalid.
  const ShaderInput zeroLength{.data = &blob, .length = 0, .type = ShaderInputType::Binary};
  EXPECT_FALSE(zeroLength.isValid());

  // A binary input that also carries a source is invalid.
  const ShaderInput mixed{
      .source = "src", .data = &blob, .length = sizeof(blob), .type = ShaderInputType::Binary};
  EXPECT_FALSE(mixed.isValid());
}

TEST(ShaderInputTest, EqualityStringInput) {
  const char* source = "shader source";
  const ShaderInput a{.source = source, .type = ShaderInputType::String};
  const ShaderInput b{.source = source, .type = ShaderInputType::String};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);

  // Equality compares string content, not pointer identity.
  const std::string copy = source;
  const ShaderInput c{.source = copy.c_str(), .type = ShaderInputType::String};
  EXPECT_TRUE(a == c);

  // Different source content compares unequal.
  const ShaderInput d{.source = "other source", .type = ShaderInputType::String};
  EXPECT_FALSE(a == d);
  EXPECT_TRUE(a != d);
}

TEST(ShaderInputTest, EqualityBinaryInput) {
  const uint32_t blobA = 0xAABBCCDD;
  const uint32_t blobB = 0xAABBCCDD; // identical bytes, distinct storage
  const uint32_t blobC = 0x11223344;
  const ShaderInput a{.data = &blobA, .length = sizeof(blobA), .type = ShaderInputType::Binary};
  const ShaderInput b{.data = &blobB, .length = sizeof(blobB), .type = ShaderInputType::Binary};
  const ShaderInput c{.data = &blobC, .length = sizeof(blobC), .type = ShaderInputType::Binary};

  // Equality compares the data bytes, not pointer identity.
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
  EXPECT_TRUE(a != c);
}

TEST(ShaderInputTest, EqualityDiffersByType) {
  const uint32_t blob = 0x12345678;
  const ShaderInput str{.source = "src", .type = ShaderInputType::String};
  const ShaderInput bin{.data = &blob, .length = sizeof(blob), .type = ShaderInputType::Binary};
  EXPECT_FALSE(str == bin);
  EXPECT_TRUE(str != bin);
}

TEST(ShaderInputTest, EqualityDiffersByOptions) {
  const char* source = "src";
  const ShaderInput a{.source = source, .type = ShaderInputType::String};
  ShaderInput b{.source = source, .type = ShaderInputType::String};
  b.options.fastMathEnabled = !b.options.fastMathEnabled;
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(ShaderModuleInfoTest, Equality) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(ShaderModuleInfoTest, InequalityByStage) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Fragment, .entryPoint = "main"};
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(ShaderModuleInfoTest, InequalityByEntryPoint) {
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  const ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "other"};
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(ShaderModuleInfoTest, InequalityByFunctionConstantValues) {
  const float value = 1.0f;
  const ShaderModuleInfo a{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  ShaderModuleInfo b{.stage = ShaderStage::Vertex, .entryPoint = "main"};
  b.functionConstantValues.setConstantValue(0, ConstantValueType::Float1, &value);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(ShaderModuleInfoTest, EqualityIgnoresDebugName) {
  const ShaderModuleInfo a{
      .stage = ShaderStage::Vertex, .entryPoint = "main", .debugName = "nameA"};
  const ShaderModuleInfo b{
      .stage = ShaderStage::Vertex, .entryPoint = "main", .debugName = "nameB"};
  // debugName is intentionally excluded from ShaderModuleInfo equality.
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST_F(ShaderModuleTest, CompileShaderModuleNoResult) {
  const char* source = nullptr;
  const auto be2 = iglDev_->getBackendType();
  if (be2 == BackendType::OpenGL) {
    source = data::shader::kOglSimpleVertShader.data();
  } else if (be2 == BackendType::Metal) {
    source = data::shader::kMtlSimpleShader.data();
  } else if (be2 == BackendType::Vulkan) {
    source = data::shader::kVulkanSimpleVertShader.data();
  } else if (be2 == BackendType::D3D12) {
    // Minimal HLSL vertex shader for D3D12 backend
    source = R"(
struct VSIn { float4 position_in : POSITION; float2 uv_in : TEXCOORD0; };
struct VSOut { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut vertexShader(VSIn i) { VSOut o; o.position = i.position_in; o.uv = i.uv_in; return o; }
VSOut main(VSIn i) { return vertexShader(i); }
)";
  } else {
    ASSERT_TRUE(0);
  }

  auto shaderModule = ShaderModuleCreator::fromStringInput(
      *iglDev_,
      source,
      {.stage = ShaderStage::Vertex,
       .entryPoint = (be2 == BackendType::D3D12) ? std::string("main")
                                                 : std::string("vertexShader")},
      "test",
      nullptr);
  ASSERT_TRUE(shaderModule != nullptr);
}
} // namespace igl::tests
