/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/DeviceFeatures.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// ShaderFamily
// ---------------------------------------------------------------------------

TEST(ShaderFamilyTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::Unknown), 0u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::Glsl), 1u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::GlslEs), 2u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::Metal), 3u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::SpirV), 4u);
  EXPECT_EQ(static_cast<uint8_t>(ShaderFamily::Hlsl), 5u);
}

// ---------------------------------------------------------------------------
// ShaderVersion
// ---------------------------------------------------------------------------

TEST(ShaderVersionTest, DefaultConstruction) {
  const ShaderVersion sv;
  EXPECT_EQ(sv.family, ShaderFamily::Unknown);
  EXPECT_EQ(sv.majorVersion, 0u);
  EXPECT_EQ(sv.minorVersion, 0u);
  EXPECT_EQ(sv.extra, 0u);
}

TEST(ShaderVersionTest, DesignatedInitializer) {
  const ShaderVersion sv{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 0,
  };
  EXPECT_EQ(sv.family, ShaderFamily::SpirV);
  EXPECT_EQ(sv.majorVersion, 1u);
  EXPECT_EQ(sv.minorVersion, 5u);
  EXPECT_EQ(sv.extra, 0u);
}

TEST(ShaderVersionTest, GlslEs300) {
  const ShaderVersion sv{
      .family = ShaderFamily::GlslEs,
      .majorVersion = 3,
      .minorVersion = 0,
  };
  EXPECT_EQ(sv.family, ShaderFamily::GlslEs);
  EXPECT_EQ(sv.majorVersion, 3u);
  EXPECT_EQ(sv.minorVersion, 0u);
  EXPECT_EQ(sv.extra, 0u);
}

TEST(ShaderVersionTest, EqualityAllFieldsMatch) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(ShaderVersionTest, InequalityDifferingFamily) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::Glsl,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

TEST(ShaderVersionTest, InequalityDifferingMajorVersion) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::SpirV,
      .majorVersion = 2,
      .minorVersion = 5,
      .extra = 7,
  };
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

TEST(ShaderVersionTest, InequalityDifferingMinorVersion) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 6,
      .extra = 7,
  };
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

TEST(ShaderVersionTest, InequalityDifferingExtra) {
  const ShaderVersion a{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 7,
  };
  const ShaderVersion b{
      .family = ShaderFamily::SpirV,
      .majorVersion = 1,
      .minorVersion = 5,
      .extra = 8,
  };
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

// ---------------------------------------------------------------------------
// DeviceRequirement
// ---------------------------------------------------------------------------

TEST(DeviceRequirementTest, EnumValues) {
  EXPECT_EQ(static_cast<int>(DeviceRequirement::ExplicitBindingExtReq), 0);
  EXPECT_EQ(static_cast<int>(DeviceRequirement::ShaderTextureLodExtReq), 1);
  EXPECT_EQ(static_cast<int>(DeviceRequirement::StandardDerivativeExtReq), 2);
  EXPECT_EQ(static_cast<int>(DeviceRequirement::TextureArrayExtReq), 3);
  EXPECT_EQ(static_cast<int>(DeviceRequirement::TextureFormatRGExtReq), 4);
}

// ---------------------------------------------------------------------------
// TextureFormatCapabilityBits
// ---------------------------------------------------------------------------

TEST(TextureFormatCapabilityBitsTest, BitValues) {
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::Unsupported, 0u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::Sampled, 1u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::SampledFiltered, 2u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::Storage, 4u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::Attachment, 8u);
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::SampledAttachment, 16u);
}

TEST(TextureFormatCapabilityBitsTest, AllIsUnionOfIndividualBits) {
  const uint8_t expected = ICapabilities::TextureFormatCapabilityBits::Sampled |
                           ICapabilities::TextureFormatCapabilityBits::SampledFiltered |
                           ICapabilities::TextureFormatCapabilityBits::Storage |
                           ICapabilities::TextureFormatCapabilityBits::Attachment |
                           ICapabilities::TextureFormatCapabilityBits::SampledAttachment;
  EXPECT_EQ(ICapabilities::TextureFormatCapabilityBits::All, expected);
}

TEST(TextureFormatCapabilityBitsTest, ContainsHelper) {
  const ICapabilities::TextureFormatCapabilities caps =
      ICapabilities::TextureFormatCapabilityBits::Sampled |
      ICapabilities::TextureFormatCapabilityBits::Attachment;
  EXPECT_TRUE(contains(caps, ICapabilities::TextureFormatCapabilityBits::Sampled));
  EXPECT_TRUE(contains(caps, ICapabilities::TextureFormatCapabilityBits::Attachment));
  EXPECT_FALSE(contains(caps, ICapabilities::TextureFormatCapabilityBits::Storage));
}

// ---------------------------------------------------------------------------
// DeviceFeatures
// ---------------------------------------------------------------------------

TEST(DeviceFeaturesEnumTest, BindBytesIsZero) {
  EXPECT_EQ(static_cast<int>(DeviceFeatures::BindBytes), 0);
}

TEST(DeviceFeaturesEnumTest, EnumValues) {
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

// ---------------------------------------------------------------------------
// DeviceFeatureLimits
// ---------------------------------------------------------------------------

TEST(DeviceFeatureLimitsTest, BufferAlignmentIsZero) {
  EXPECT_EQ(static_cast<int>(DeviceFeatureLimits::BufferAlignment), 0);
}

TEST(DeviceFeatureLimitsTest, EnumValues) {
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
