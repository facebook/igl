/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanPipelineBuilder.h>

#include "../util/TestDevice.h"

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanPipelineBuilderTest : public ::testing::Test {
 public:
  VulkanPipelineBuilderTest() = default;
  ~VulkanPipelineBuilderTest() override = default;

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

TEST_F(VulkanPipelineBuilderTest, DepthBiasEnable) {
  igl::vulkan::VulkanPipelineBuilder builder;
  auto& result = builder.depthBiasEnable(true);
  EXPECT_EQ(&result, &builder);
}

TEST_F(VulkanPipelineBuilderTest, StencilStateOps) {
  igl::vulkan::VulkanPipelineBuilder builder;
  auto& result = builder.stencilStateOps(VK_STENCIL_FACE_FRONT_BIT,
                                         VK_STENCIL_OP_KEEP,
                                         VK_STENCIL_OP_REPLACE,
                                         VK_STENCIL_OP_INCREMENT_AND_CLAMP,
                                         VK_COMPARE_OP_ALWAYS);
  EXPECT_EQ(&result, &builder);
}

TEST_F(VulkanPipelineBuilderTest, CullModeVariants) {
  igl::vulkan::VulkanPipelineBuilder builder1;
  auto& r1 = builder1.cullMode(VK_CULL_MODE_NONE);
  EXPECT_EQ(&r1, &builder1);

  igl::vulkan::VulkanPipelineBuilder builder2;
  auto& r2 = builder2.cullMode(VK_CULL_MODE_FRONT_BIT);
  EXPECT_EQ(&r2, &builder2);

  igl::vulkan::VulkanPipelineBuilder builder3;
  auto& r3 = builder3.cullMode(VK_CULL_MODE_BACK_BIT);
  EXPECT_EQ(&r3, &builder3);

  igl::vulkan::VulkanPipelineBuilder builder4;
  auto& r4 = builder4.cullMode(VK_CULL_MODE_FRONT_AND_BACK);
  EXPECT_EQ(&r4, &builder4);
}

TEST_F(VulkanPipelineBuilderTest, PolygonModeLine) {
  igl::vulkan::VulkanPipelineBuilder builder;
  auto& result = builder.polygonMode(VK_POLYGON_MODE_LINE);
  EXPECT_EQ(&result, &builder);
}

TEST_F(VulkanPipelineBuilderTest, MultipleColorBlendAttachments) {
  igl::vulkan::VulkanPipelineBuilder builder;

  std::vector<VkPipelineColorBlendAttachmentState> blendStates(3);
  for (auto& state : blendStates) {
    state = {};
    state.blendEnable = VK_TRUE;
    state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.colorBlendOp = VK_BLEND_OP_ADD;
    state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    state.alphaBlendOp = VK_BLEND_OP_ADD;
    state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  }

  auto& result = builder.colorBlendAttachmentStates(blendStates);
  EXPECT_EQ(&result, &builder);
}

TEST_F(VulkanPipelineBuilderTest, PrimitiveTopologyVariants) {
  igl::vulkan::VulkanPipelineBuilder builder;
  auto& result = builder.primitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
  EXPECT_EQ(&result, &builder);
}

TEST_F(VulkanPipelineBuilderTest, DynamicStates) {
  igl::vulkan::VulkanPipelineBuilder builder;
  std::vector<VkDynamicState> states = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS};
  auto& result = builder.dynamicStates(states);
  EXPECT_EQ(&result, &builder);
}

TEST_F(VulkanPipelineBuilderTest, DepthCompareOp) {
  igl::vulkan::VulkanPipelineBuilder builder;
  auto& result = builder.depthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL, true);
  EXPECT_EQ(&result, &builder);
}

TEST_F(VulkanPipelineBuilderTest, ComputePipelineBasic) {
  igl::vulkan::VulkanComputePipelineBuilder builder{};
  (void)builder; // Verify construction succeeds
  EXPECT_GE(igl::vulkan::VulkanComputePipelineBuilder::getNumPipelinesCreated(), 0u);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
