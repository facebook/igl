/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "data/ShaderData.h"
#include "util/Common.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl {
namespace tests {

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

 public:
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
  if (backend_ == util::BACKEND_VUL) {
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
  if (backend_ == util::BACKEND_OGL) {
    source = data::shader::OGL_SIMPLE_VERT_SHADER;
  } else if (backend_ == util::BACKEND_MTL) {
    source = data::shader::MTL_SIMPLE_SHADER;
  } else if (backend_ == util::BACKEND_VUL) {
    source = data::shader::VULKAN_SIMPLE_VERT_SHADER;
  } else {
    ASSERT_TRUE(0);
  }

  auto shaderModule = ShaderModuleCreator::fromStringInput(
      *iglDev_, source, {ShaderStage::Vertex, "vertexShader"}, "test", &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(shaderModule != nullptr);
}

TEST_F(ShaderModuleTest, CompileShaderModuleNoResult) {
  const char* source = nullptr;
  if (backend_ == util::BACKEND_OGL) {
    source = data::shader::OGL_SIMPLE_VERT_SHADER;
  } else if (backend_ == util::BACKEND_MTL) {
    source = data::shader::MTL_SIMPLE_SHADER;
  } else if (backend_ == util::BACKEND_VUL) {
    source = data::shader::VULKAN_SIMPLE_VERT_SHADER;
  } else {
    ASSERT_TRUE(0);
  }

  auto shaderModule = ShaderModuleCreator::fromStringInput(
      *iglDev_, source, {ShaderStage::Vertex, "vertexShader"}, "test", nullptr);
  ASSERT_TRUE(shaderModule != nullptr);
}
} // namespace tests
} // namespace igl
