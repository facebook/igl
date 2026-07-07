/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/DeviceFeatures.h>

namespace igl::tests {

TEST(DeviceFeaturesEnumTest, FirstValueIsZero) {
  EXPECT_EQ(static_cast<int>(DeviceFeatures::BindBytes), 0);
}

TEST(DeviceFeaturesEnumTest, EnumOrdering) {
  EXPECT_EQ(static_cast<int>(DeviceFeatures::BindUniform), 1);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::BufferDeviceAddress), 2);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::BufferNoCopy), 3);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::BufferRing), 4);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::Compute), 5);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::CopyBuffer), 6);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::DepthCompare), 7);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::DepthShaderRead), 8);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::DrawFirstIndexFirstVertex), 9);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::DrawIndexedIndirect), 10);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::DrawInstanced), 11);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::ExplicitBinding), 12);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::ExplicitBindingExt), 13);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::ExternalMemoryObjects), 14);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::Indices8Bit), 15);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::MapBufferRange), 16);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::MeshShaders), 17);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::MinMaxBlend), 18);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::MultipleRenderTargets), 19);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::MultiSample), 20);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::MultiSampleResolve), 21);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::Multiview), 22);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::MultiViewMultisample), 23);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::PushConstants), 24);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::ReadWriteFramebuffer), 25);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::SamplerMinMaxLod), 26);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::ShaderLibrary), 27);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::ShaderTextureLod), 28);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::ShaderTextureLodExt), 29);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::SRGB), 30);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::SRGBSwapchain), 31);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::SRGBWriteControl), 32);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::StandardDerivative), 33);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::StandardDerivativeExt), 34);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::StorageBuffers), 35);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::Texture2DArray), 36);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureArrayExt), 37);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::Texture3D), 38);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureBindless), 39);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureExternalImage), 40);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureFilterAnisotropic), 41);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureFloat), 42);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureFormatRG), 43);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureFormatRGB), 44);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureHalfFloat), 45);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureNotPot), 46);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TexturePartialMipChain), 47);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TextureViews), 48);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::TimestampQueries), 49);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::Timers), 50);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::UniformBlocks), 51);
  EXPECT_EQ(static_cast<int>(DeviceFeatures::ValidationLayersEnabled), 52);
}

TEST(DeviceFeatureLimitsTest, FirstValueIsZero) {
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::BufferAlignment), 0);
}

TEST(DeviceFeatureLimitsTest, EnumOrdering) {
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::BufferNoCopyAlignment), 1);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxBindBytesBytes), 2);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxCubeMapDimension), 3);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxFragmentUniformVectors), 4);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxMultisampleCount), 5);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxPushConstantBytes), 6);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxTextureDimension1D2D), 7);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxTextureDimension3D), 8);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxStorageBufferBytes), 9);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxUniformBufferBytes), 10);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxVertexUniformVectors), 11);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::PushConstantsAlignment), 12);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::ShaderStorageBufferOffsetAlignment), 13);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxComputeWorkGroupSizeX), 14);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxComputeWorkGroupSizeY), 15);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxComputeWorkGroupSizeZ), 16);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxComputeWorkGroupInvocations), 17);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxVertexInputAttributes), 18);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxColorAttachments), 19);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxDescriptorHeapCbvSrvUav), 20);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxDescriptorHeapSamplers), 21);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxDescriptorHeapRtvs), 22);
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::MaxDescriptorHeapDsvs), 23);
}

} // namespace igl::tests
