/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <igl/ComputeCommandEncoder.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class CommandBufferVulkanTest : public ::testing::Test {
 public:
  CommandBufferVulkanTest() = default;
  ~CommandBufferVulkanTest() override = default;

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

TEST_F(CommandBufferVulkanTest, GetVkCommandBuffer) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(cmdBuf, nullptr);

  auto* vulkanCmdBuf = static_cast<igl::vulkan::CommandBuffer*>(cmdBuf.get());
  EXPECT_TRUE(vulkanCmdBuf->getVkCommandBuffer() != nullptr);

  cmdQueue_->submit(*cmdBuf);
}

TEST_F(CommandBufferVulkanTest, DebugGroupLabels) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(cmdBuf, nullptr);

  cmdBuf->pushDebugGroupLabel("TestGroup", Color(1.0f, 0.0f, 0.0f, 1.0f));
  cmdBuf->popDebugGroupLabel();

  cmdQueue_->submit(*cmdBuf);
}

TEST_F(CommandBufferVulkanTest, CopyBuffer) {
  Result ret;

  BufferDesc srcDesc;
  srcDesc.type = BufferDesc::BufferTypeBits::Storage;
  srcDesc.storage = ResourceStorage::Shared;
  srcDesc.length = 128;
  auto srcBuffer = iglDev_->createBuffer(srcDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  BufferDesc dstDesc;
  dstDesc.type = BufferDesc::BufferTypeBits::Storage;
  dstDesc.storage = ResourceStorage::Shared;
  dstDesc.length = 128;
  auto dstBuffer = iglDev_->createBuffer(dstDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  std::vector<uint32_t> srcData(32, 0xCAFEBABE);
  ret = srcBuffer->upload(srcData.data(), BufferRange(128, 0));
  ASSERT_TRUE(ret.isOk());

  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  cmdBuf->copyBuffer(*srcBuffer, *dstBuffer, 0, 0, 128);
  cmdQueue_->submit(*cmdBuf);

  auto& device = static_cast<igl::vulkan::Device&>(*iglDev_);
  device.getVulkanContext().waitIdle();

  const auto* downloadedData = static_cast<uint32_t*>(dstBuffer->map(BufferRange(128, 0), &ret));
  ASSERT_TRUE(ret.isOk());
  for (size_t i = 0; i < 32; ++i) {
    EXPECT_EQ(downloadedData[i], 0xCAFEBABE);
  }
  dstBuffer->unmap();
}

TEST_F(CommandBufferVulkanTest, WaitUntilCompleted) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(cmdBuf, nullptr);

  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

TEST_F(CommandBufferVulkanTest, CreateComputeCommandEncoder) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(cmdBuf, nullptr);

  auto encoder = cmdBuf->createComputeCommandEncoder();
  EXPECT_NE(encoder, nullptr);
  encoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);
}

TEST_F(CommandBufferVulkanTest, GetNextSubmitHandle) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  auto* vulkanCmdBuf = static_cast<igl::vulkan::CommandBuffer*>(cmdBuf.get());
  auto handle = vulkanCmdBuf->getNextSubmitHandle();
  EXPECT_FALSE(handle.empty());

  cmdQueue_->submit(*cmdBuf);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
