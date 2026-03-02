/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <igl/DepthStencilState.h>
#include <igl/Device.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class DeviceExtendedVulkanTest : public ::testing::Test {
 public:
  DeviceExtendedVulkanTest() = default;
  ~DeviceExtendedVulkanTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
};

TEST_F(DeviceExtendedVulkanTest, CreateDepthStencilState) {
  Result ret;

  DepthStencilStateDesc desc;
  desc.isDepthWriteEnabled = true;
  desc.compareFunction = CompareFunction::Less;

  auto state = iglDev_->createDepthStencilState(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(state, nullptr);
}

TEST_F(DeviceExtendedVulkanTest, CreateDepthStencilStateWithStencil) {
  Result ret;

  DepthStencilStateDesc desc;
  desc.isDepthWriteEnabled = true;
  desc.compareFunction = CompareFunction::LessEqual;
  desc.frontFaceStencil.stencilCompareFunction = CompareFunction::AlwaysPass;
  desc.frontFaceStencil.stencilFailureOperation = StencilOperation::Keep;
  desc.frontFaceStencil.depthFailureOperation = StencilOperation::Keep;
  desc.frontFaceStencil.depthStencilPassOperation = StencilOperation::Replace;
  desc.backFaceStencil = desc.frontFaceStencil;

  auto state = iglDev_->createDepthStencilState(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(state, nullptr);
}

TEST_F(DeviceExtendedVulkanTest, GetShaderVersion) {
  auto version = iglDev_->getShaderVersion();
  EXPECT_GT(version.majorVersion, 0u);
}

TEST_F(DeviceExtendedVulkanTest, GetBackendVersion) {
  auto version = iglDev_->getBackendVersion();
  EXPECT_GT(version.majorVersion, 0u);
}

TEST_F(DeviceExtendedVulkanTest, GetCurrentDrawCount) {
  auto drawCount = iglDev_->getCurrentDrawCount();
  EXPECT_EQ(drawCount, 0u);
}

TEST_F(DeviceExtendedVulkanTest, GetBackendType) {
  EXPECT_EQ(iglDev_->getBackendType(), BackendType::Vulkan);
}

TEST_F(DeviceExtendedVulkanTest, HasFeatureTexture3D) {
  EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Texture3D));
}

TEST_F(DeviceExtendedVulkanTest, GetFeatureLimitsMaxTextureDimension) {
  size_t maxDim = 0;
  bool result = iglDev_->getFeatureLimits(DeviceFeatureLimits::MaxTextureDimension1D2D, maxDim);
  EXPECT_TRUE(result);
  EXPECT_GT(maxDim, 0u);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
