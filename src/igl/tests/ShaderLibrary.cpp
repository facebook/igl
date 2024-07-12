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

namespace igl::tests {

class ShaderLibraryTest : public ::testing::Test {
 private:
 public:
  ShaderLibraryTest() = default;
  ~ShaderLibraryTest() override = default;

  //
  // SetUp()
  //
  void SetUp() override {
    // Turn off debug breaks, only use in debug mode
    igl::setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);
  }

  void TearDown() override {}

 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(ShaderLibraryTest, CreateFromSource) {
  Result ret;
  if (!iglDev_->hasFeature(DeviceFeatures::ShaderLibrary)) {
    GTEST_SKIP() << "Shader Libraries are unsupported for this platform.";
    return;
  }

  const char* source = nullptr;
  if (iglDev_->getBackendType() == igl::BackendType::Metal) {
    source = data::shader::MTL_SIMPLE_SHADER;
  } else if (iglDev_->getBackendType() == igl::BackendType::Vulkan) {
    source = data::shader::VULKAN_SIMPLE_VERT_SHADER;
  } else {
    IGL_ASSERT_NOT_REACHED();
  }

  auto shaderLibrary = ShaderLibraryCreator::fromStringInput(
      *iglDev_, source, {{ShaderStage::Vertex, "vertexShader"}}, "", &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(shaderLibrary != nullptr);

  auto vertShaderModule = shaderLibrary->getShaderModule("vertexShader");
  ASSERT_TRUE(vertShaderModule);
}

TEST_F(ShaderLibraryTest, CreateFromSingleModuleReturnNullWithEmptyInput) {
  Result ret;
  if (!iglDev_->hasFeature(DeviceFeatures::ShaderLibrary)) {
    GTEST_SKIP() << "Shader Libraries are unsupported for this platform.";
    return;
  }

  auto shaderLibrary =
      ShaderLibraryCreator::fromStringInput(*iglDev_, "", {{ShaderStage::Vertex, ""}}, "", &ret);
  ASSERT_TRUE(!ret.isOk());
  ASSERT_TRUE(shaderLibrary == nullptr);
}

TEST_F(ShaderLibraryTest, CreateFromSourceMultipleModules) {
  Result ret;
  if (!iglDev_->hasFeature(DeviceFeatures::ShaderLibrary)) {
    GTEST_SKIP() << "Shader Libraries are unsupported for this platform.";
    return;
  }

  const char* source = nullptr;
  if (iglDev_->getBackendType() == igl::BackendType::Metal) {
    source = data::shader::MTL_SIMPLE_SHADER;
  } else if (iglDev_->getBackendType() == igl::BackendType::Vulkan) {
    GTEST_SKIP() << "Vulkan does not support multiple modules from the same source code.";
    return;
  }
  auto shaderLibrary =
      ShaderLibraryCreator::fromStringInput(*iglDev_,
                                            source,
                                            {
                                                {ShaderStage::Vertex, "vertexShader"},
                                                {ShaderStage::Fragment, "fragmentShader"},
                                            },
                                            "",
                                            &ret);

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(shaderLibrary != nullptr);

  auto vertShaderModule = shaderLibrary->getShaderModule("vertexShader");
  ASSERT_TRUE(vertShaderModule);

  auto fragShaderModule = shaderLibrary->getShaderModule("fragmentShader");
  ASSERT_TRUE(fragShaderModule);
}

TEST_F(ShaderLibraryTest, CreateFromSourceNoResult) {
  if (!iglDev_->hasFeature(DeviceFeatures::ShaderLibrary)) {
    GTEST_SKIP() << "Shader Libraries are unsupported for this platform.";
    return;
  }

  const char* source = nullptr;
  if (iglDev_->getBackendType() == igl::BackendType::Metal) {
    source = data::shader::MTL_SIMPLE_SHADER;
  } else if (iglDev_->getBackendType() == igl::BackendType::Vulkan) {
    source = data::shader::VULKAN_SIMPLE_VERT_SHADER;
  } else {
    IGL_ASSERT_NOT_REACHED();
  }

  auto shaderLibrary = ShaderLibraryCreator::fromStringInput(
      *iglDev_, source, {{ShaderStage::Vertex, "vertexShader"}}, "", nullptr);
  ASSERT_TRUE(shaderLibrary != nullptr);

  auto vertShaderModule = shaderLibrary->getShaderModule("vertexShader");
  ASSERT_TRUE(vertShaderModule);
}
} // namespace igl::tests
