/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanRenderPassBuilder.h>

#include "../util/TestDevice.h"

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanRenderPassBuilderTest : public ::testing::Test {
 public:
  VulkanRenderPassBuilderTest() = default;
  ~VulkanRenderPassBuilderTest() override = default;

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

TEST_F(VulkanRenderPassBuilderTest, SingleColorAttachment) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
}

TEST_F(VulkanRenderPassBuilderTest, ColorWithDepthStencil) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addDepthStencil(
      VK_FORMAT_D24_UNORM_S8_UINT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
}

TEST_F(VulkanRenderPassBuilderTest, MultipleColorAttachments) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);

  auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
}

TEST_F(VulkanRenderPassBuilderTest, WithResolveAttachment) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(VK_FORMAT_R8G8B8A8_UNORM,
                   VK_ATTACHMENT_LOAD_OP_CLEAR,
                   VK_ATTACHMENT_STORE_OP_STORE,
                   VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_SAMPLE_COUNT_4_BIT);
  builder.addColorResolve(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE);

  auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
}

TEST_F(VulkanRenderPassBuilderTest, MultiviewRenderPass) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.setMultiviewMasks(0x3, 0x3);

  auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
}

TEST_F(VulkanRenderPassBuilderTest, HashEquality) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder::HashFunction hasher;
  EXPECT_EQ(hasher(builder1), hasher(builder2));
}

TEST_F(VulkanRenderPassBuilderTest, HashInequality) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_B8G8R8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder::HashFunction hasher;
  EXPECT_NE(hasher(builder1), hasher(builder2));
}

TEST_F(VulkanRenderPassBuilderTest, EqualityOperator) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  EXPECT_TRUE(builder1 == builder2);

  igl::vulkan::VulkanRenderPassBuilder builder3;
  builder3.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);

  EXPECT_FALSE(builder1 == builder3);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
