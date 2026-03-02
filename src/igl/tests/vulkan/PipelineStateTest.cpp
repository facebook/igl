/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <igl/SamplerState.h>
#include <igl/VertexInputState.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class PipelineStateTest : public ::testing::Test {
 public:
  PipelineStateTest() = default;
  ~PipelineStateTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;

  igl::vulkan::VulkanContext& getVulkanContext() {
    auto& device = static_cast<igl::vulkan::Device&>(*iglDev_);
    return device.getVulkanContext();
  }
};

TEST_F(PipelineStateTest, PipelineLayoutCreation) {
  auto& ctx = getVulkanContext();
  EXPECT_TRUE(ctx.getVkDevice() != VK_NULL_HANDLE);
  EXPECT_TRUE(ctx.pipelineCache_ != VK_NULL_HANDLE);
}

TEST_F(PipelineStateTest, CreateSamplerState) {
  Result ret;

  SamplerStateDesc desc;
  desc.minFilter = SamplerMinMagFilter::Linear;
  desc.magFilter = SamplerMinMagFilter::Linear;
  desc.mipFilter = SamplerMipFilter::Linear;
  desc.addressModeU = SamplerAddressMode::Repeat;
  desc.addressModeV = SamplerAddressMode::Repeat;

  auto sampler = iglDev_->createSamplerState(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(sampler, nullptr);
}

TEST_F(PipelineStateTest, CreateVertexInputState) {
  Result ret;

  VertexInputStateDesc desc;
  desc.numAttributes = 1;
  desc.attributes[0].format = VertexAttributeFormat::Float4;
  desc.attributes[0].offset = 0;
  desc.attributes[0].bufferIndex = 0;
  desc.attributes[0].name = "position";
  desc.attributes[0].location = 0;
  desc.numInputBindings = 1;
  desc.inputBindings[0].stride = 16;
  desc.inputBindings[0].sampleFunction = VertexSampleFunction::PerVertex;

  auto vis = iglDev_->createVertexInputState(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(vis, nullptr);
}

TEST_F(PipelineStateTest, PipelineCacheExists) {
  auto& ctx = getVulkanContext();
  EXPECT_TRUE(ctx.pipelineCache_ != VK_NULL_HANDLE);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
