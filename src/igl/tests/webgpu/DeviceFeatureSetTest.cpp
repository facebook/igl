/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/DeviceFeatureSet.h>

#include <memory>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/webgpu/PlatformDevice.h>

namespace igl::tests {

namespace {

using CapabilityBits = ICapabilities::TextureFormatCapabilityBits;

constexpr ICapabilities::TextureFormatCapabilities kRenderable =
    CapabilityBits::Sampled | CapabilityBits::Attachment | CapabilityBits::SampledAttachment;

} // namespace

class WebGPUDeviceFeatureSetTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
    platformDevice_ = &device_->getPlatformDevice();
  }

 protected:
  [[nodiscard]] bool adapterHas(WGPUFeatureName feature) const {
    return wgpuAdapterHasFeature(platformDevice_->getWGPUAdapter(), feature) != 0;
  }

  std::unique_ptr<webgpu::Device> device_;
  const webgpu::PlatformDevice* platformDevice_ = nullptr;
};

TEST_F(WebGPUDeviceFeatureSetTest, PlatformDeviceExposesHandles) {
  EXPECT_NE(platformDevice_->getWGPUInstance(), nullptr);
  EXPECT_NE(platformDevice_->getWGPUAdapter(), nullptr);
  EXPECT_EQ(platformDevice_->getWGPUDevice(), device_->getNativeDevice());
  EXPECT_NE(platformDevice_->getWGPUQueue(), nullptr);
  EXPECT_EQ(&platformDevice_->getContext(), &device_->getContext());
}

TEST_F(WebGPUDeviceFeatureSetTest, OptionalFeaturesFollowAdapter) {
  for (const WGPUFeatureName feature : webgpu::WebGPUContext::getOptionalFeatures()) {
    EXPECT_EQ(platformDevice_->hasWGPUFeature(feature), adapterHas(feature))
        << "feature " << static_cast<int>(feature);
  }
}

TEST_F(WebGPUDeviceFeatureSetTest, OptionalFeaturesCanBeDisabled) {
  auto device = util::device::webgpu::createTestDevice({.requestOptionalFeatures = false});
  ASSERT_NE(device, nullptr);
  for (const WGPUFeatureName feature : webgpu::WebGPUContext::getOptionalFeatures()) {
    EXPECT_FALSE(device->getPlatformDevice().hasWGPUFeature(feature))
        << "feature " << static_cast<int>(feature);
  }
}

TEST_F(WebGPUDeviceFeatureSetTest, Features) {
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::DrawInstanced));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::MultipleRenderTargets));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::SRGB));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::UniformBlocks));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::ValidationLayersEnabled));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::Compute));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::StorageBuffers));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::DrawIndexedIndirect));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::DynamicCullMode));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::DynamicFrontFacingWinding));

  EXPECT_FALSE(device_->hasFeature(DeviceFeatures::BufferRing));
  EXPECT_FALSE(device_->hasFeature(DeviceFeatures::Indices8Bit));
  EXPECT_FALSE(device_->hasFeature(DeviceFeatures::Multiview));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::PushConstants));
  EXPECT_FALSE(device_->hasFeature(DeviceFeatures::ReadWriteFramebuffer));
  EXPECT_FALSE(device_->hasFeature(DeviceFeatures::TextureFormatRGB));

  EXPECT_FALSE(device_->hasRequirement(DeviceRequirement::ExplicitBindingExtReq));
  EXPECT_FALSE(device_->hasRequirement(DeviceRequirement::TextureFormatRGExtReq));
}

TEST_F(WebGPUDeviceFeatureSetTest, LimitsDefaultToSpec) {
  size_t value = 0;
  EXPECT_TRUE(device_->getFeatureLimits(DeviceFeatureLimits::BufferAlignment, value));
  EXPECT_EQ(value, 256u);
  EXPECT_TRUE(device_->getFeatureLimits(DeviceFeatureLimits::MaxTextureDimension1D2D, value));
  EXPECT_EQ(value, 8192u);
  EXPECT_TRUE(device_->getFeatureLimits(DeviceFeatureLimits::MaxTextureDimension3D, value));
  EXPECT_EQ(value, 2048u);
  EXPECT_TRUE(device_->getFeatureLimits(DeviceFeatureLimits::MaxUniformBufferBytes, value));
  EXPECT_EQ(value, 65536u);
  EXPECT_TRUE(device_->getFeatureLimits(DeviceFeatureLimits::MaxMultisampleCount, value));
  EXPECT_EQ(value, 4u);
  EXPECT_TRUE(device_->getFeatureLimits(DeviceFeatureLimits::MaxColorAttachments, value));
  EXPECT_EQ(value, 8u);
  EXPECT_TRUE(device_->getFeatureLimits(DeviceFeatureLimits::MaxPushConstantBytes, value));
  EXPECT_EQ(value, 128u);
  EXPECT_FALSE(device_->getFeatureLimits(DeviceFeatureLimits::MaxDescriptorHeapSamplers, value));
}

TEST_F(WebGPUDeviceFeatureSetTest, AdapterLimitsCanBeRequested) {
  auto device = util::device::webgpu::createTestDevice({.requestAdapterLimits = true});
  ASSERT_NE(device, nullptr);
  WGPULimits adapterLimits = WGPU_LIMITS_INIT;
  ASSERT_EQ(wgpuAdapterGetLimits(device->getPlatformDevice().getWGPUAdapter(), &adapterLimits),
            WGPUStatus_Success);
  size_t value = 0;
  EXPECT_TRUE(device->getFeatureLimits(DeviceFeatureLimits::MaxTextureDimension3D, value));
  EXPECT_EQ(value, adapterLimits.maxTextureDimension3D);
}

TEST_F(WebGPUDeviceFeatureSetTest, ColorFormatCapabilities) {
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGBA_UNorm8),
            kRenderable | CapabilityBits::SampledFiltered | CapabilityBits::Storage);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::BGRA_SRGB),
            kRenderable | CapabilityBits::SampledFiltered);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGBA_SRGB),
            kRenderable | CapabilityBits::SampledFiltered);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGBA_F16),
            kRenderable | CapabilityBits::SampledFiltered | CapabilityBits::Storage);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGBA_UInt32),
            kRenderable | CapabilityBits::Storage);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::R_UInt16), kRenderable);
  const ICapabilities::TextureFormatCapabilities bgraStorage =
      adapterHas(WGPUFeatureName_BGRA8UnormStorage) ? CapabilityBits::Storage : 0;
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::BGRA_UNorm8),
            kRenderable | CapabilityBits::SampledFiltered | bgraStorage);
}

TEST_F(WebGPUDeviceFeatureSetTest, Float32Filterability) {
  const ICapabilities::TextureFormatCapabilities filtered =
      adapterHas(WGPUFeatureName_Float32Filterable) ? CapabilityBits::SampledFiltered : 0;
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::R_F32),
            kRenderable | filtered | CapabilityBits::Storage);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGBA_F32),
            kRenderable | filtered | CapabilityBits::Storage);
}

TEST_F(WebGPUDeviceFeatureSetTest, DepthStencilFormatCapabilities) {
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::Z_UNorm16), kRenderable);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::Z_UNorm32), kRenderable);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::S8_UInt_Z24_UNorm), kRenderable);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::S8_UInt_Z32_UNorm),
            adapterHas(WGPUFeatureName_Depth32FloatStencil8) ? kRenderable : 0);
}

TEST_F(WebGPUDeviceFeatureSetTest, FeatureGatedFormats) {
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::B10G11R11_UFloat),
            CapabilityBits::Sampled | CapabilityBits::SampledFiltered |
                (adapterHas(WGPUFeatureName_RG11B10UfloatRenderable)
                     ? CapabilityBits::Attachment | CapabilityBits::SampledAttachment
                     : 0));

  const ICapabilities::TextureFormatCapabilities compressed =
      CapabilityBits::Sampled | CapabilityBits::SampledFiltered;
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGBA_ASTC_4x4),
            adapterHas(WGPUFeatureName_TextureCompressionASTC) ? compressed : 0);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGB8_ETC2),
            adapterHas(WGPUFeatureName_TextureCompressionETC2) ? compressed : 0);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGBA_BC7_UNORM_4x4),
            adapterHas(WGPUFeatureName_TextureCompressionBC) ? compressed : 0);
}

TEST_F(WebGPUDeviceFeatureSetTest, UnmappedFormatsAreUnsupported) {
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::Invalid), 0);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGB_F32), 0);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::R5G6B5_UNorm), 0);
  EXPECT_EQ(device_->getTextureFormatCapabilities(TextureFormat::RGBA_PVRTC_4BPPV1), 0);
}

TEST_F(WebGPUDeviceFeatureSetTest, LimitsAreUnavailableWithoutDevice) {
  const webgpu::DeviceFeatureSet featureSet(nullptr);
  size_t result = 1;
  EXPECT_FALSE(featureSet.getFeatureLimits(DeviceFeatureLimits::BufferAlignment, result));
  EXPECT_EQ(result, 0u);
  EXPECT_FALSE(featureSet.getFeatureLimits(DeviceFeatureLimits::MaxMultisampleCount, result));
}

} // namespace igl::tests
