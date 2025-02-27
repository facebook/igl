/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/DeviceFeatureSet.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl::tests {

class DeviceFeatureSetMTLTest : public ::testing::Test {
 public:
  DeviceFeatureSetMTLTest() = default;
  ~DeviceFeatureSetMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    mtlDevice_ = MTLCreateSystemDefaultDevice();
    ASSERT_NE(mtlDevice_, nullptr);
  }

  void TearDown() override {}

 public:
  id<MTLDevice> mtlDevice_;
};

// Test Cases

TEST_F(DeviceFeatureSetMTLTest, HasFeatureTest) {
  const auto mtlDeviceFeatureSet = metal::DeviceFeatureSet(mtlDevice_);

  // We currently expect all these to be "true", i.e. available features on Metal
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::MultiSample), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::MultiSampleResolve), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::TextureFilterAnisotropic), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::MapBufferRange), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::MultipleRenderTargets), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::StandardDerivative), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::TextureFormatRG), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::ReadWriteFramebuffer), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::TextureNotPot), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::TextureHalfFloat), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::TextureFloat), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::Texture2DArray), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::Texture3D), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::ShaderTextureLod), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::DepthShaderRead), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::MinMaxBlend), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::TexturePartialMipChain), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::ShaderLibrary), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::BindBytes), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::SRGB), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::SRGBSwapchain), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::DrawIndexedIndirect), true);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::ExplicitBinding), true);

  // We currently expect all these to be "false", i.e. NOT available on Metal
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::Multiview), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::BindUniform), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::BufferDeviceAddress), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::ShaderTextureLodExt), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::TextureExternalImage), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::TextureArrayExt), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::ExplicitBindingExt), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::ValidationLayersEnabled), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasFeature(DeviceFeatures::ExternalMemoryObjects), false);
}

TEST_F(DeviceFeatureSetMTLTest, HasRequirementTest) {
  const auto mtlDeviceFeatureSet = metal::DeviceFeatureSet(mtlDevice_);

  // We expect these to fail on Metal because the requirements here are OpenGL extensions.
  ASSERT_EQ(mtlDeviceFeatureSet.hasRequirement(DeviceRequirement::StandardDerivativeExtReq), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasRequirement(DeviceRequirement::TextureFormatRGExtReq), false);
  ASSERT_EQ(mtlDeviceFeatureSet.hasRequirement(DeviceRequirement::ShaderTextureLodExtReq), false);
}

} // namespace igl::tests
