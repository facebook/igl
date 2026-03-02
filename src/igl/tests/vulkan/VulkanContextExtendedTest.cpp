/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <future>
#include <igl/CommandBuffer.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanContextExtendedTest : public ::testing::Test {
 public:
  VulkanContextExtendedTest() = default;
  ~VulkanContextExtendedTest() override = default;

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

TEST_F(VulkanContextExtendedTest, WaitIdle) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto cmdQueue = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
  ASSERT_TRUE(ret.isOk());

  auto cmdBuf = cmdQueue->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());
  cmdQueue->submit(*cmdBuf);

  auto result = ctx.waitIdle();
  EXPECT_TRUE(result.isOk()) << result.message.c_str();
}

TEST_F(VulkanContextExtendedTest, DeferredTaskExecution) {
  auto& ctx = getVulkanContext();

  bool taskExecuted = false;
  std::packaged_task<void()> task([&taskExecuted]() { taskExecuted = true; });

  ctx.deferredTask(std::move(task));

  Result ret;
  auto cmdQueue = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
  ASSERT_TRUE(ret.isOk());
  auto cmdBuf = cmdQueue->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());
  cmdQueue->submit(*cmdBuf);

  ctx.waitDeferredTasks();
}

TEST_F(VulkanContextExtendedTest, GetPipelineCacheData) {
  auto& ctx = getVulkanContext();

  auto cacheData = ctx.getPipelineCacheData();
  // Cache data may be empty if no pipelines have been created, but the call must not crash
  SUCCEED();
}

TEST_F(VulkanContextExtendedTest, GetVkDevice) {
  auto& ctx = getVulkanContext();
  EXPECT_TRUE(ctx.getVkDevice() != nullptr);
}

TEST_F(VulkanContextExtendedTest, GetVkPhysicalDevice) {
  auto& ctx = getVulkanContext();
  EXPECT_TRUE(ctx.getVkPhysicalDevice() != nullptr);
}

TEST_F(VulkanContextExtendedTest, GetVkInstance) {
  auto& ctx = getVulkanContext();
  EXPECT_TRUE(ctx.getVkInstance() != nullptr);
}

TEST_F(VulkanContextExtendedTest, PhysicalDeviceProperties) {
  auto& ctx = getVulkanContext();
  const auto& props = ctx.getVkPhysicalDeviceProperties();

  EXPECT_NE(props.apiVersion, 0u);
  EXPECT_GT(props.limits.maxImageDimension2D, 0u);
}

TEST_F(VulkanContextExtendedTest, CurrentSyncIndex) {
  auto& ctx = getVulkanContext();
  EXPECT_LT(ctx.currentSyncIndex(), ctx.config_.maxResourceCount);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
