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

TEST_F(VulkanRenderPassBuilderTest, WithDepthStencilResolveAttachment) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addDepthStencil(VK_FORMAT_D24_UNORM_S8_UINT,
                          VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VK_ATTACHMENT_STORE_OP_DONT_CARE,
                          VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                          VK_ATTACHMENT_STORE_OP_DONT_CARE,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          VK_SAMPLE_COUNT_4_BIT);
  builder.addDepthStencilResolve(
      VK_FORMAT_D24_UNORM_S8_UINT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE);

  auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
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

TEST_F(VulkanRenderPassBuilderTest, HashInequalityDepthStencil) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addDepthStencil(
      VK_FORMAT_D24_UNORM_S8_UINT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.addDepthStencil(
      VK_FORMAT_D32_SFLOAT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder::HashFunction hasher;
  EXPECT_NE(hasher(builder1), hasher(builder2));
}

TEST_F(VulkanRenderPassBuilderTest, EqualityDepthStencil) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addDepthStencil(
      VK_FORMAT_D24_UNORM_S8_UINT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.addDepthStencil(
      VK_FORMAT_D24_UNORM_S8_UINT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  EXPECT_TRUE(builder1 == builder2);

  igl::vulkan::VulkanRenderPassBuilder builder3;
  builder3.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder3.addDepthStencil(
      VK_FORMAT_D32_SFLOAT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  EXPECT_FALSE(builder1 == builder3);
}

TEST_F(VulkanRenderPassBuilderTest, DepthStencilWithExplicitStencilOps) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addDepthStencil(VK_FORMAT_D24_UNORM_S8_UINT,
                          VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VK_ATTACHMENT_STORE_OP_STORE,
                          VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VK_ATTACHMENT_STORE_OP_STORE);

  auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
}

TEST_F(VulkanRenderPassBuilderTest, EqualityMultiview) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.setMultiviewMasks(0x3, 0x3);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.setMultiviewMasks(0x3, 0x3);

  EXPECT_TRUE(builder1 == builder2);

  igl::vulkan::VulkanRenderPassBuilder builder3;
  builder3.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder3.setMultiviewMasks(0xF, 0xF);

  EXPECT_FALSE(builder1 == builder3);
}

TEST_F(VulkanRenderPassBuilderTest, DefaultBuilderEquality) {
  const igl::vulkan::VulkanRenderPassBuilder a;
  const igl::vulkan::VulkanRenderPassBuilder b;

  EXPECT_TRUE(a == b);

  igl::vulkan::VulkanRenderPassBuilder::HashFunction hasher;
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST_F(VulkanRenderPassBuilderTest, HashInequalityMultiview) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.setMultiviewMasks(0x3, 0x3);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.setMultiviewMasks(0xF, 0xF);

  igl::vulkan::VulkanRenderPassBuilder::HashFunction hasher;
  EXPECT_NE(hasher(builder1), hasher(builder2));
}

TEST_F(VulkanRenderPassBuilderTest, RenderPassCaching) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const auto rp1 = ctx.findRenderPass(builder1);
  const auto rp2 = ctx.findRenderPass(builder2);

  EXPECT_NE(rp1.pass, VK_NULL_HANDLE);
  EXPECT_EQ(rp1.pass, rp2.pass);
}

// Multisubpass render pass tests

TEST_F(VulkanRenderPassBuilderTest, NumSubpassesDefaultsToOne) {
  igl::vulkan::VulkanRenderPassBuilder builder;
  EXPECT_EQ(builder.getNumSubpasses(), 1u);
}

TEST_F(VulkanRenderPassBuilderTest, SetSubpassesReportsCount) {
  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const igl::RenderPassDesc::SubpassDesc subpasses[] = {
      {.colorAttachmentMask = 0b1110, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b0001, .inputAttachmentMask = 0b1110},
  };
  builder.setSubpasses(subpasses, 2);
  EXPECT_EQ(builder.getNumSubpasses(), 2u);
}

TEST_F(VulkanRenderPassBuilderTest, DeferredLayoutPreserveAndDependencies) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

  const igl::RenderPassDesc::SubpassDesc subpasses[] = {
      {.colorAttachmentMask = 0b0001, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b1110, .inputAttachmentMask = 0b0001},
  };
  builder.setSubpasses(subpasses, 2);

  EXPECT_EQ(builder.getPreserveMask(0), 0u);
  EXPECT_EQ(builder.getPreserveMask(1), 0u);
  EXPECT_EQ(builder.getNumDependencies(), 2u);

  const auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
  EXPECT_EQ(rp.numSubpasses, 2u);
}

TEST_F(VulkanRenderPassBuilderTest, ThreeSubpassChainPreserveMasks) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const igl::RenderPassDesc::SubpassDesc subpasses[] = {
      {.colorAttachmentMask = 0b0011, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b0100, .inputAttachmentMask = 0b0001},
      {.colorAttachmentMask = 0b1000, .inputAttachmentMask = 0b0110},
  };
  builder.setSubpasses(subpasses, 3);

  EXPECT_EQ(builder.getPreserveMask(0), 0u);
  EXPECT_EQ(builder.getPreserveMask(1), 0b0010u);
  EXPECT_EQ(builder.getPreserveMask(2), 0b0001u);
  EXPECT_EQ(builder.getNumDependencies(), 5u);

  const auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
  EXPECT_EQ(rp.numSubpasses, 3u);
}

TEST_F(VulkanRenderPassBuilderTest, ThreeSubpassChainWithoutStorePreserve) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

  const igl::RenderPassDesc::SubpassDesc subpasses[] = {
      {.colorAttachmentMask = 0b0011, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b0100, .inputAttachmentMask = 0b0001},
      {.colorAttachmentMask = 0b1000, .inputAttachmentMask = 0b0110},
  };
  builder.setSubpasses(subpasses, 3);

  EXPECT_EQ(builder.getPreserveMask(0), 0u);
  EXPECT_EQ(builder.getPreserveMask(1), 0b0010u);
  EXPECT_EQ(builder.getPreserveMask(2), 0u);
  EXPECT_EQ(builder.getNumDependencies(), 5u);

  const auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
}

TEST_F(VulkanRenderPassBuilderTest, FourSubpassChainPreserveMask) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const igl::RenderPassDesc::SubpassDesc subpasses[] = {
      {.colorAttachmentMask = 0b0001, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b0010, .inputAttachmentMask = 0b0001},
      {.colorAttachmentMask = 0b0100, .inputAttachmentMask = 0b0010},
      {.colorAttachmentMask = 0b1000, .inputAttachmentMask = 0b0101},
  };
  builder.setSubpasses(subpasses, 4);

  EXPECT_EQ(builder.getPreserveMask(2), 0b0001u);
  EXPECT_EQ(builder.getNumDependencies(), 6u);

  const auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
  EXPECT_EQ(rp.numSubpasses, 4u);
}

TEST_F(VulkanRenderPassBuilderTest, LegacyBuilderHasOneDependency) {
  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  EXPECT_EQ(builder.getNumDependencies(), 1u);
}

TEST_F(VulkanRenderPassBuilderTest, SubpassMasksAffectEqualityAndHash) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const igl::RenderPassDesc::SubpassDesc subpasses1[] = {
      {.colorAttachmentMask = 0b0110, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b1001, .inputAttachmentMask = 0b0110},
  };
  builder1.setSubpasses(subpasses1, 2);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const igl::RenderPassDesc::SubpassDesc subpasses2[] = {
      {.colorAttachmentMask = 0b0110, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b1001, .inputAttachmentMask = 0b0001},
  };
  builder2.setSubpasses(subpasses2, 2);

  EXPECT_FALSE(builder1 == builder2);
  igl::vulkan::VulkanRenderPassBuilder::HashFunction hasher;
  EXPECT_NE(hasher(builder1), hasher(builder2));
}

TEST_F(VulkanRenderPassBuilderTest, SubpassMasksIdenticalAffectEqualityAndHash) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const igl::RenderPassDesc::SubpassDesc subpasses[] = {
      {.colorAttachmentMask = 0b01, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0b01},
  };
  builder1.setSubpasses(subpasses, 2);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.setSubpasses(subpasses, 2);

  EXPECT_TRUE(builder1 == builder2);
  igl::vulkan::VulkanRenderPassBuilder::HashFunction hasher;
  EXPECT_EQ(hasher(builder1), hasher(builder2));
}

TEST_F(VulkanRenderPassBuilderTest, CompatibilityKeyIgnoresLoadOpsAndInitialLayout) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(VK_FORMAT_R8G8B8A8_UNORM,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(VK_FORMAT_R8G8B8A8_UNORM,
                    VK_ATTACHMENT_LOAD_OP_LOAD,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  const auto key1 = builder1.compatibilityKey();
  const auto key2 = builder2.compatibilityKey();

  EXPECT_TRUE(key1 == key2);
}

TEST_F(VulkanRenderPassBuilderTest, CompatibilityKeyIgnoresStoreOpsForSingleSubpass) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

  const auto key1 = builder1.compatibilityKey();
  const auto key2 = builder2.compatibilityKey();

  EXPECT_TRUE(key1 == key2);
}

TEST_F(VulkanRenderPassBuilderTest, CompatibilityKeyIncludesPreserveMasks) {
  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const igl::RenderPassDesc::SubpassDesc subpasses1[] = {
      {.colorAttachmentMask = 0b0011, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b0100, .inputAttachmentMask = 0b0001},
      {.colorAttachmentMask = 0b1000, .inputAttachmentMask = 0b0110},
  };
  builder1.setSubpasses(subpasses1, 3);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

  const igl::RenderPassDesc::SubpassDesc subpasses2[] = {
      {.colorAttachmentMask = 0b0011, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b0100, .inputAttachmentMask = 0b0001},
      {.colorAttachmentMask = 0b1000, .inputAttachmentMask = 0b0110},
  };
  builder2.setSubpasses(subpasses2, 3);

  const auto key1 = builder1.compatibilityKey();
  const auto key2 = builder2.compatibilityKey();

  EXPECT_FALSE(key1 == key2);
}

TEST_F(VulkanRenderPassBuilderTest, FindRenderPassMultiSubpassCreatesPass) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder;
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE);

  const igl::RenderPassDesc::SubpassDesc subpasses[] = {
      {.colorAttachmentMask = 0b01, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0b01},
  };
  builder.setSubpasses(subpasses, 2);

  const auto rp = ctx.findRenderPass(builder);
  EXPECT_NE(rp.pass, VK_NULL_HANDLE);
  EXPECT_EQ(rp.numSubpasses, 2u);
}

TEST_F(VulkanRenderPassBuilderTest, FindRenderPassSharesCompatIndexAcrossLoadOps) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(VK_FORMAT_R8G8B8A8_UNORM,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(VK_FORMAT_R8G8B8A8_UNORM,
                    VK_ATTACHMENT_LOAD_OP_LOAD,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  const auto rp1 = ctx.findRenderPass(builder1);
  const auto rp2 = ctx.findRenderPass(builder2);

  EXPECT_NE(rp1.index, rp2.index);
  EXPECT_EQ(rp1.compatIndex, rp2.compatIndex);
}

TEST_F(VulkanRenderPassBuilderTest, FindRenderPassSeparatesSingleAndMultiSubpass) {
  auto& ctx = getVulkanContext();

  igl::vulkan::VulkanRenderPassBuilder builder1;
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder1.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  igl::vulkan::VulkanRenderPassBuilder builder2;
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);
  builder2.addColor(
      VK_FORMAT_R8G8B8A8_UNORM, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

  const igl::RenderPassDesc::SubpassDesc subpasses[] = {
      {.colorAttachmentMask = 0b01, .inputAttachmentMask = 0},
      {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0b01},
  };
  builder2.setSubpasses(subpasses, 2);

  const auto rp1 = ctx.findRenderPass(builder1);
  const auto rp2 = ctx.findRenderPass(builder2);

  EXPECT_NE(rp1.compatIndex, rp2.compatIndex);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
