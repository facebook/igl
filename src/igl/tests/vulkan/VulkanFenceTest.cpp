/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanFence.h>

#include "../util/TestDevice.h"

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanFenceTest : public ::testing::Test {
 public:
  VulkanFenceTest() = default;
  ~VulkanFenceTest() override = default;

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

TEST_F(VulkanFenceTest, CreateSignaledFence) {
  auto& ctx = getVulkanContext();
  igl::vulkan::VulkanFence fence(
      ctx.vf_, ctx.getVkDevice(), VK_FENCE_CREATE_SIGNALED_BIT, false, "testSignaledFence");

  EXPECT_NE(fence.vkFence_, VK_NULL_HANDLE);
  EXPECT_FALSE(fence.exportable());

  EXPECT_TRUE(fence.wait(0));
}

TEST_F(VulkanFenceTest, CreateUnsignaledFence) {
  auto& ctx = getVulkanContext();
  igl::vulkan::VulkanFence fence(ctx.vf_, ctx.getVkDevice(), 0, false, "testUnsignaledFence");

  EXPECT_NE(fence.vkFence_, VK_NULL_HANDLE);
  EXPECT_FALSE(fence.exportable());
}

TEST_F(VulkanFenceTest, ResetFence) {
  auto& ctx = getVulkanContext();
  igl::vulkan::VulkanFence fence(
      ctx.vf_, ctx.getVkDevice(), VK_FENCE_CREATE_SIGNALED_BIT, false, "testResetFence");

  ASSERT_NE(fence.vkFence_, VK_NULL_HANDLE);
  EXPECT_TRUE(fence.wait(0));

  EXPECT_TRUE(fence.reset());
}

TEST_F(VulkanFenceTest, ExportableFence) {
#if IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
  auto& ctx = getVulkanContext();
  igl::vulkan::VulkanFence fence(
      ctx.vf_, ctx.getVkDevice(), VK_FENCE_CREATE_SIGNALED_BIT, true, "testExportableFence");

  EXPECT_NE(fence.vkFence_, VK_NULL_HANDLE);
  EXPECT_TRUE(fence.exportable());
#else
  GTEST_SKIP() << "Exportable fences may not be supported on this platform.";
#endif
}

TEST_F(VulkanFenceTest, MoveConstruction) {
  auto& ctx = getVulkanContext();
  igl::vulkan::VulkanFence original(
      ctx.vf_, ctx.getVkDevice(), VK_FENCE_CREATE_SIGNALED_BIT, false, "testMoveCtor");

  VkFence originalHandle = original.vkFence_;
  ASSERT_NE(originalHandle, VK_NULL_HANDLE);

  igl::vulkan::VulkanFence moved(std::move(original));

  EXPECT_EQ(moved.vkFence_, originalHandle);
  // After move, original is in a valid but unspecified state - don't access it
}

TEST_F(VulkanFenceTest, MoveAssignment) {
  auto& ctx = getVulkanContext();
  igl::vulkan::VulkanFence original(
      ctx.vf_, ctx.getVkDevice(), VK_FENCE_CREATE_SIGNALED_BIT, false, "testMoveAssign1");
  igl::vulkan::VulkanFence target(ctx.vf_, ctx.getVkDevice(), 0, false, "testMoveAssign2");

  VkFence originalHandle = original.vkFence_;
  ASSERT_NE(originalHandle, VK_NULL_HANDLE);

  target = std::move(original);

  EXPECT_EQ(target.vkFence_, originalHandle);
  // After move, original is in a valid but unspecified state - don't access it
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
