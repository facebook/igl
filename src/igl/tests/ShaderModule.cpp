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

  auto shaderModule = ShaderModuleCreator::fromStringInput(
      *iglDev_, "hello world", {ShaderStage::Vertex, "Mordor"}, "test", &ret);
  ASSERT_TRUE(!ret.isOk());
  ASSERT_TRUE(shaderModule == nullptr);
}

TEST_F(ShaderModuleTest, CompileShaderModuleReturnNullWithEmptyInput) {
  Result ret;

  auto shaderModule =
      ShaderModuleCreator::fromStringInput(*iglDev_, "", {ShaderStage::Vertex, ""}, "test", &ret);
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
      {ShaderStage::Vertex,
       (be == BackendType::D3D12) ? std::string("main") : std::string("vertexShader")},
      "test",
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(shaderModule != nullptr);
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
      {ShaderStage::Vertex,
       (be2 == BackendType::D3D12) ? std::string("main") : std::string("vertexShader")},
      "test",
      nullptr);
  ASSERT_TRUE(shaderModule != nullptr);
}
} // namespace igl::tests
