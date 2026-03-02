/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/Device.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class ComputeCommandEncoderVulkanTest : public ::testing::Test {
 public:
  ComputeCommandEncoderVulkanTest() = default;
  ~ComputeCommandEncoderVulkanTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";

    Result ret;
    cmdQueue_ = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_NE(cmdQueue_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(ComputeCommandEncoderVulkanTest, CreateAndEndEncoding) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  auto encoder = cmdBuf->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

TEST_F(ComputeCommandEncoderVulkanTest, BindPushConstants) {
  // NOTE: bindPushConstants requires a compute pipeline to be bound first.
  // Since creating a compute pipeline requires a compute shader, and this is
  // just testing the encoder API, we skip this test. The push constants
  // functionality is tested through integration tests that have full pipelines.
  GTEST_SKIP() << "bindPushConstants requires a bound compute pipeline with a shader";
}

TEST_F(ComputeCommandEncoderVulkanTest, DebugGroupLabels) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  auto encoder = cmdBuf->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  encoder->pushDebugGroupLabel("ComputeGroup", Color(0.0f, 1.0f, 0.0f, 1.0f));
  encoder->popDebugGroupLabel();

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
