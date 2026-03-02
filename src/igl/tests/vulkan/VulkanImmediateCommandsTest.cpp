/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanImmediateCommands.h>

#include "../util/TestDevice.h"

#include <set>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanImmediateCommandsTest : public ::testing::Test {
 public:
  VulkanImmediateCommandsTest() = default;
  ~VulkanImmediateCommandsTest() override = default;

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

TEST_F(VulkanImmediateCommandsTest, AcquireCommandBuffer) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper = ctx.immediate_->acquire();
  EXPECT_TRUE(wrapper.cmdBuf != VK_NULL_HANDLE);
  EXPECT_TRUE(wrapper.isEncoding);
  ctx.immediate_->submit(wrapper);
}

TEST_F(VulkanImmediateCommandsTest, SubmitAndWait) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper = ctx.immediate_->acquire();
  ASSERT_TRUE(wrapper.cmdBuf != VK_NULL_HANDLE);

  auto handle = ctx.immediate_->submit(wrapper);
  EXPECT_FALSE(handle.empty());

  VkResult result = ctx.immediate_->wait(handle);
  EXPECT_EQ(result, VK_SUCCESS);
  EXPECT_TRUE(ctx.immediate_->isReady(handle));
}

TEST_F(VulkanImmediateCommandsTest, MultipleSubmitsGetUniqueHandles) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  std::set<uint32_t> submitIds;

  for (int i = 0; i < 3; ++i) {
    const auto& wrapper = ctx.immediate_->acquire();
    auto handle = ctx.immediate_->submit(wrapper);
    EXPECT_FALSE(handle.empty());
    submitIds.insert(handle.submitId);
  }

  EXPECT_EQ(submitIds.size(), 3u);
}

TEST_F(VulkanImmediateCommandsTest, CommandBufferRecycling) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  std::vector<igl::vulkan::VulkanImmediateCommands::SubmitHandle> handles;

  for (int i = 0; i < 4; ++i) {
    const auto& wrapper = ctx.immediate_->acquire();
    auto handle = ctx.immediate_->submit(wrapper);
    handles.push_back(handle);
  }

  for (auto& h : handles) {
    ctx.immediate_->wait(h);
  }

  const auto& wrapper = ctx.immediate_->acquire();
  EXPECT_TRUE(wrapper.cmdBuf != VK_NULL_HANDLE);
  EXPECT_TRUE(wrapper.isEncoding);
  ctx.immediate_->submit(wrapper);
}

TEST_F(VulkanImmediateCommandsTest, GetLastSubmitHandle) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  const auto& wrapper1 = ctx.immediate_->acquire();
  ctx.immediate_->submit(wrapper1);

  const auto& wrapper2 = ctx.immediate_->acquire();
  auto handle2 = ctx.immediate_->submit(wrapper2);

  auto lastHandle = ctx.immediate_->getLastSubmitHandle();
  EXPECT_EQ(lastHandle.submitId, handle2.submitId);
  EXPECT_EQ(lastHandle.bufferIndex, handle2.bufferIndex);
}

TEST_F(VulkanImmediateCommandsTest, IsReadyOnEmptyHandle) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  igl::vulkan::VulkanImmediateCommands::SubmitHandle emptyHandle;
  EXPECT_TRUE(emptyHandle.empty());
  EXPECT_TRUE(ctx.immediate_->isReady(emptyHandle));
}

TEST_F(VulkanImmediateCommandsTest, WaitAllCommandBuffers) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  std::vector<igl::vulkan::VulkanImmediateCommands::SubmitHandle> handles;

  for (int i = 0; i < 4; ++i) {
    const auto& wrapper = ctx.immediate_->acquire();
    auto handle = ctx.immediate_->submit(wrapper);
    handles.push_back(handle);
  }

  ctx.immediate_->waitAll();

  for (auto& h : handles) {
    EXPECT_TRUE(ctx.immediate_->isReady(h));
  }
}

TEST_F(VulkanImmediateCommandsTest, GetNextSubmitHandle) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.immediate_, nullptr);

  // Submit one buffer first so getNextSubmitHandle has a valid state
  const auto& wrapper = ctx.immediate_->acquire();
  ctx.immediate_->submit(wrapper);

  auto nextHandle = ctx.immediate_->getNextSubmitHandle();
  EXPECT_GT(nextHandle.submitId, 0u);
}

TEST_F(VulkanImmediateCommandsTest, SubmitHandleComparison) {
  igl::vulkan::VulkanImmediateCommands::SubmitHandle h1;
  h1.bufferIndex = 0;
  h1.submitId = 1;

  igl::vulkan::VulkanImmediateCommands::SubmitHandle h2;
  h2.bufferIndex = 0;
  h2.submitId = 1;

  EXPECT_EQ(h1, h2);

  h2.submitId = 2;
  EXPECT_NE(h1, h2);
}

TEST_F(VulkanImmediateCommandsTest, SubmitHandleRoundTrip) {
  igl::vulkan::VulkanImmediateCommands::SubmitHandle original;
  original.bufferIndex = 5;
  original.submitId = 42;

  uint64_t packed = original.handle();

  igl::vulkan::VulkanImmediateCommands::SubmitHandle restored(packed);
  EXPECT_EQ(restored.bufferIndex, original.bufferIndex);
  EXPECT_EQ(restored.submitId, original.submitId);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
