/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/DeviceFeatureSet.h>

#include <vector>

// get the GPU family from the device
// a return value of 0 indicates an error or lack of a supported GPU
static size_t getGPUFamily(id<MTLDevice> device) {
  // the new supportsFamily API is applicable to both iOS and macOS
  if (@available(macOS 10.15, iOS 13.0, *)) {
    typedef std::pair<MTLGPUFamily, size_t> GPUFamilyPair;
    std::vector<GPUFamilyPair> const gpuFamilies = {
        // @fb-only
        // @fb-only
        {MTLGPUFamilyApple8, 8},
        {MTLGPUFamilyApple7, 7},
        {MTLGPUFamilyApple6, 6},
        {MTLGPUFamilyApple5, 5},
        {MTLGPUFamilyApple4, 4},
        {MTLGPUFamilyApple3, 3},
        {MTLGPUFamilyApple2, 2},
        {MTLGPUFamilyApple1, 1}};

    // return the first (highest) supported GPU family
    for (const GPUFamilyPair& gpuFam : gpuFamilies) {
      if ([device supportsFamily:gpuFam.first]) {
        return gpuFam.second;
      }
    }
  } else {
    // resort to the old deprecated API supportsFeatureSet for older OS versions
    typedef std::pair<MTLFeatureSet, size_t> FeatureSetPair;
    std::vector<FeatureSetPair> featureSets;

#if IGL_PLATFORM_IOS
    if (@available(iOS 12, *)) {
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily5_v1, 5);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily4_v2, 4);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily3_v4, 3);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily2_v5, 2);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily1_v5, 1);
    } else if (@available(iOS 11, *)) {
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily4_v1, 4);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily3_v3, 3);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily2_v4, 2);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily1_v4, 1);
    } else if (@available(iOS 10, *)) {
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily3_v2, 3);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily2_v3, 2);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily1_v3, 1);
    } else if (@available(iOS 9, *)) {
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily3_v1, 3);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily2_v2, 2);
      featureSets.emplace_back(MTLFeatureSet_iOS_GPUFamily1_v2, 1);
    } else {
      IGL_ASSERT_MSG(0, "IGL iOS deployment target is 9.0+");
      return 0;
    }
#elif IGL_PLATFORM_MACOS
    if (@available(macOS 10.14, *)) {
      featureSets.emplace_back(MTLFeatureSet_macOS_GPUFamily2_v1, 2);
      featureSets.emplace_back(MTLFeatureSet_macOS_GPUFamily1_v4, 1);
    } else if (@available(macOS 10.13, *)) {
      featureSets.emplace_back(MTLFeatureSet_macOS_GPUFamily1_v3, 1);
    } else if (@available(macOS 10.12, *)) {
      featureSets.emplace_back(MTLFeatureSet_macOS_GPUFamily1_v2, 1);
    } else if (@available(macOS 10.11, *)) {
      featureSets.emplace_back(MTLFeatureSet_macOS_GPUFamily1_v1, 1);
    } else {
      IGL_ASSERT_MSG(0, "IGL macOS deployment target is 10.11+");
      return 0;
    }
#endif

    // return the first (highest) supported GPU family
    for (const FeatureSetPair& featureSet : featureSets) {
      if ([device supportsFeatureSet:featureSet.first]) {
        return featureSet.second;
      }
    }
  }

  IGL_LOG_INFO("No supported GPU family available");
  return 0;
}

namespace igl {
namespace metal {

DeviceFeatureSet::DeviceFeatureSet(id<MTLDevice> device) {
  gpuFamily_ = getGPUFamily(device);

  // Get the supported MSAA
  maxMultisampleCount_ = 0;
  for (auto i = 1; i <= IGL_TEXTURE_SAMPLERS_MAX; i *= 2) {
    if ([device supportsTextureSampleCount:i]) {
      maxMultisampleCount_ = i;
    } else {
      break;
    }
  }

  // get max buffer length
  maxBufferLength_ = [device maxBufferLength];

  if (@available(macOS 11.0, iOS 14.0, *)) {
    // this API became available as of iOS 14 and macOS 11
    supports32BitFloatFiltering_ = device.supports32BitFloatFiltering;
  }
}

bool DeviceFeatureSet::hasFeature(DeviceFeatures feature) const {
  // Metal supports all the basic features on iOS9.0+
  // see reference https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
  // For more advanced features, use gpu family and version to determine availability
  switch (feature) {
  case DeviceFeatures::MultiSample:
  case DeviceFeatures::MultiSampleResolve:
  case DeviceFeatures::TextureFilterAnisotropic:
  case DeviceFeatures::MapBufferRange:
  case DeviceFeatures::MultipleRenderTargets:
  case DeviceFeatures::StandardDerivative:
  case DeviceFeatures::TextureFormatRG:
  case DeviceFeatures::ReadWriteFramebuffer:
  case DeviceFeatures::TextureNotPot:
  case DeviceFeatures::TextureHalfFloat:
  case DeviceFeatures::TextureFloat:
  case DeviceFeatures::ShaderTextureLod:
  case DeviceFeatures::DepthShaderRead:
  case DeviceFeatures::MinMaxBlend:
  case DeviceFeatures::UniformBlocks:
  case DeviceFeatures::ExplicitBinding:
  case DeviceFeatures::Texture2DArray:
  case DeviceFeatures::Texture3D:
  case DeviceFeatures::SRGB:
  case DeviceFeatures::DrawIndexedIndirect:
    return true;
  // on Metal and Vulkan, the framebuffer pixel format dictates sRGB control.
  case DeviceFeatures::SRGBWriteControl:
    return false;
  // No current Metal devices support packed, uncompressed RGB textures.
  case DeviceFeatures::TextureFormatRGB:
    return false;
  case DeviceFeatures::ExplicitBindingExt:
  case DeviceFeatures::StandardDerivativeExt:
  case DeviceFeatures::ShaderTextureLodExt:
    return false;
  case DeviceFeatures::DepthCompare:
    /// docs say:
    ///  The MTLFeatureSet_iOS_GPUFamily2_v1 and MTLFeatureSet_OSX_GPUFamily1_v1 feature sets allow
    ///  you to define a framework-side sampler comparison function for a MTLSamplerState object.
    ///  All feature sets support shader-side sampler comparison functions, as described in the
    ///  Metal Shading Language Guide.
#if IGL_PLATFORM_IOS
    return gpuFamily_ >= 2;
#else
    return gpuFamily_ >= 1;
#endif
  case DeviceFeatures::TextureExternalImage:
    return false;
  case DeviceFeatures::Compute:
    return true;
  case DeviceFeatures::TextureBindless:
    return false;
  case DeviceFeatures::BufferDeviceAddress:
    return false;
  case DeviceFeatures::Multiview:
    return false;
  case DeviceFeatures::BindUniform:
    return false;
  case DeviceFeatures::TexturePartialMipChain:
    return true;
  case DeviceFeatures::BufferRing:
    return true;
  case DeviceFeatures::BufferNoCopy:
    return true;
  case DeviceFeatures::StorageBuffers:
    return true;
  case DeviceFeatures::ShaderLibrary:
    return true;
  case DeviceFeatures::BindBytes:
    return true;
  case DeviceFeatures::TextureArrayExt:
    return false;
  case DeviceFeatures::SamplerMinMaxLod:
    return true;
  case DeviceFeatures::ValidationLayersEnabled:
    return false;
  case DeviceFeatures::ExternalMemoryObjects:
    return false;
  case DeviceFeatures::PushConstants:
    return false;
  }
  return false;
}

bool DeviceFeatureSet::hasRequirement(DeviceRequirement requirement) const {
  switch (requirement) {
  case DeviceRequirement::ExplicitBindingExtReq:
  case DeviceRequirement::StandardDerivativeExtReq:
  case DeviceRequirement::TextureArrayExtReq:
  case DeviceRequirement::TextureFormatRGExtReq:
  case DeviceRequirement::ShaderTextureLodExtReq:
    return false;
  }
  return true;
}

// iOS 9 plus implementation
bool DeviceFeatureSet::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  switch (featureLimits) {
  case DeviceFeatureLimits::MaxTextureDimension1D2D:
  case DeviceFeatureLimits::MaxCubeMapDimension:
#if IGL_PLATFORM_IOS
    result = (gpuFamily_ <= 2) ? 8192 : 16384;
#else // macos
    result = 16384;
#endif
    return true;
  case DeviceFeatureLimits::MaxFragmentUniformVectors:
    // According to Metal gurus, this should be identical to MaxVertexUniformVectors
  case DeviceFeatureLimits::MaxVertexUniformVectors:
    // MaxVertexUniformVectors = number of vec4
    {
      const size_t maxBuffers = 31;
      result = (maxBuffers * maxBufferLength_) / (sizeof(float) * 4);
      return true;
    }
  case DeviceFeatureLimits::MaxMultisampleCount:
    result = maxMultisampleCount_;
    return true;
  case DeviceFeatureLimits::MaxPushConstantBytes:
    result = 4096;
    return true;
  case DeviceFeatureLimits::MaxStorageBufferBytes:
  case DeviceFeatureLimits::MaxUniformBufferBytes:
    result = maxBufferLength_;
    return true;
  case DeviceFeatureLimits::PushConstantsAlignment:
    result = 16;
    return true;
  case DeviceFeatureLimits::ShaderStorageBufferOffsetAlignment:
  case DeviceFeatureLimits::BufferAlignment: {
    // Since IGL currently doesn't distinguish how buffers are being used, for consistency reasons,
    // we currently assume BufferAlignment means Constant Buffer offset alignment
#if IGL_PLATFORM_MACOS
    result = 32;
#elif IGL_PLATFORM_IOS_SIMULATOR
    result = 256;
#else
    result = 16;
#endif
    return true;
  }
  case DeviceFeatureLimits::BufferNoCopyAlignment: {
    IGL_ASSERT(getpagesize() > 0);
    result = static_cast<size_t>(getpagesize());
    return true;
  }
  case DeviceFeatureLimits::MaxBindBytesBytes:
    result = 4096;
    return true;
  default:
    IGL_ASSERT_MSG(
        0,
        "invalid feature limit query: feature limit query is not implemented or does not exist\n");
    return false;
  }
}

ICapabilities::TextureFormatCapabilities DeviceFeatureSet::getTextureFormatCapabilities(
    TextureFormat format) const {
  const auto all = ICapabilities::TextureFormatCapabilityBits::All;
  const auto sampled = ICapabilities::TextureFormatCapabilityBits::Sampled;
  const auto attachment = ICapabilities::TextureFormatCapabilityBits::Attachment;
  const auto storage = ICapabilities::TextureFormatCapabilityBits::Storage;
  const auto sampledFiltered = ICapabilities::TextureFormatCapabilityBits::SampledFiltered;
  const auto sampledAttachment = ICapabilities::TextureFormatCapabilityBits::SampledAttachment;
  const auto unsupported = ICapabilities::TextureFormatCapabilityBits::Unsupported;

  switch (format) {
  case TextureFormat::Invalid:
    return unsupported;

    // 8 bpp
  case TextureFormat::A_UNorm8:
    return sampled;
  case TextureFormat::R_UNorm8:
    return all;

    // 16 bpp
  case TextureFormat::R_F16:
    return all;
  case TextureFormat::R_UInt16:
    return sampled | storage | attachment | sampledAttachment;
  case TextureFormat::R_UNorm16:
    return all;
  case TextureFormat::B5G5R5A1_UNorm:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST || IGL_PLATFORM_IOS_SIMULATOR
    return unsupported;
#else
    return sampled | attachment | sampledAttachment;
#endif
  case TextureFormat::B5G6R5_UNorm:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST || IGL_PLATFORM_IOS_SIMULATOR
    return unsupported;
#else
    return sampled | attachment | sampledAttachment;
#endif
  case TextureFormat::ABGR_UNorm4:
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST || IGL_PLATFORM_IOS_SIMULATOR
    return unsupported;
#else
    return sampled | attachment | sampledAttachment;
#endif

  case TextureFormat::RG_UNorm8:
    return all;
  case TextureFormat::R4G2B2_UNorm_Apple:
    return sampled;
  case TextureFormat::R4G2B2_UNorm_Rev_Apple:
    return sampled;
  case TextureFormat::RGBA_UNorm8:
  case TextureFormat::BGRA_UNorm8:
  case TextureFormat::RGB10_A2_UNorm_Rev:
  case TextureFormat::BGR10_A2_Unorm:
  case TextureFormat::RGBA_SRGB:
  case TextureFormat::BGRA_SRGB:
    return all;
  case TextureFormat::RGB10_A2_Uint_Rev:
    return (sampled | storage | attachment | sampledAttachment);

    // 32 bpp
  case TextureFormat::RG_F16:
    return all;
  case TextureFormat::RG_UInt16:
    return sampled | storage | attachment | sampledAttachment;
  case TextureFormat::RG_UNorm16:
    return all;
  case TextureFormat::R_F32:
    return sampled | storage | attachment | sampledAttachment |
           (supports32BitFloatFiltering_ ? sampledFiltered : 0);

    // 64 bpp
  case TextureFormat::RGBA_F16:
    return all;
  case TextureFormat::RG_F32:
    return sampled | storage | attachment | sampledAttachment |
           (supports32BitFloatFiltering_ ? sampledFiltered : 0);

    // 96 bpp
  case TextureFormat::RGB_F32:
    return unsupported;
  // 128 bps
  case TextureFormat::RGBA_UInt32:
    return sampled | storage | attachment | sampledAttachment;
  case TextureFormat::RGBA_F32:
    return sampled | storage | attachment | sampledAttachment |
           (supports32BitFloatFiltering_ ? sampledFiltered : 0);
    // Compressed formats

  case TextureFormat::RGBA_ASTC_4x4:
  case TextureFormat::SRGB8_A8_ASTC_4x4:
  case TextureFormat::RGBA_ASTC_5x4:
  case TextureFormat::SRGB8_A8_ASTC_5x4:
  case TextureFormat::RGBA_ASTC_5x5:
  case TextureFormat::SRGB8_A8_ASTC_5x5:
  case TextureFormat::RGBA_ASTC_6x5:
  case TextureFormat::SRGB8_A8_ASTC_6x5:
  case TextureFormat::RGBA_ASTC_6x6:
  case TextureFormat::SRGB8_A8_ASTC_6x6:
  case TextureFormat::RGBA_ASTC_8x5:
  case TextureFormat::SRGB8_A8_ASTC_8x5:
  case TextureFormat::RGBA_ASTC_8x6:
  case TextureFormat::SRGB8_A8_ASTC_8x6:
  case TextureFormat::RGBA_ASTC_8x8:
  case TextureFormat::SRGB8_A8_ASTC_8x8:
  case TextureFormat::RGBA_ASTC_10x5:
  case TextureFormat::SRGB8_A8_ASTC_10x5:
  case TextureFormat::RGBA_ASTC_10x6:
  case TextureFormat::SRGB8_A8_ASTC_10x6:
  case TextureFormat::RGBA_ASTC_10x8:
  case TextureFormat::SRGB8_A8_ASTC_10x8:
  case TextureFormat::RGBA_ASTC_10x10:
  case TextureFormat::SRGB8_A8_ASTC_10x10:
  case TextureFormat::RGBA_ASTC_12x10:
  case TextureFormat::SRGB8_A8_ASTC_12x10:
  case TextureFormat::RGBA_ASTC_12x12:
  case TextureFormat::SRGB8_A8_ASTC_12x12:
#if TARGET_OS_OSX
    return unsupported;
#else
    if (gpuFamily_ >= 2) {
      return sampled;
    } else {
      return unsupported;
    }
#endif

  case TextureFormat::RGBA_PVRTC_2BPPV1:
  case TextureFormat::RGB_PVRTC_2BPPV1:
  case TextureFormat::RGBA_PVRTC_4BPPV1:
  case TextureFormat::RGB_PVRTC_4BPPV1:
#if TARGET_OS_OSX
    return unsupported;
#else
    return sampled;
#endif

  case TextureFormat::RGB8_ETC2:
  case TextureFormat::RGBA8_EAC_ETC2:
#if TARGET_OS_OSX
    return unsupported;
#else
    return sampled;
#endif

  case TextureFormat::RGBA_BC7_UNORM_4x4:
  case TextureFormat::RGBA_BC7_SRGB_4x4:

#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
    return sampled;
#else
    return unsupported;
#endif

    // Depth and stencil formats

  case TextureFormat::Z_UNorm16:
    // This exists in Metal, but we unconditionally force it to Depth32Float
    return sampled | sampledFiltered | attachment | sampledAttachment;

  case TextureFormat::Z_UNorm24:
    // This doesn't exist in Metal, but we map it to Depth32Float
    return sampled | attachment | sampledAttachment;

  case TextureFormat::Z_UNorm32:
    // This doesn't exist in Metal, but we map it to Depth32Float
    return sampled | attachment | sampledAttachment;

  case TextureFormat::S8_UInt_Z24_UNorm:
    // This exists in Metal for macOS, but we unconditionally force it to Depth32Float_Stencil8
#if TARGET_OS_OSX
    return sampled | attachment | sampledAttachment;
#else
    return sampled;
#endif

  case TextureFormat::S8_UInt_Z32_UNorm:
#if TARGET_OS_OSX
    return sampled | attachment | sampledAttachment;
#else
    return sampled;
#endif

  case TextureFormat::S_UInt8:
    return sampled | attachment | sampledAttachment;

  // Formats with no support in IGL Metal
  case TextureFormat::L_UNorm8:
  case TextureFormat::LA_UNorm8:
  case TextureFormat::R5G5B5A1_UNorm:
  case TextureFormat::RGBX_UNorm8:
  case TextureFormat::BGRA_UNorm8_Rev:
  case TextureFormat::RGB_F16:
  case TextureFormat::RGB8_ETC1:
  case TextureFormat::RGB8_Punchthrough_A1_ETC2:
  case TextureFormat::SRGB8_ETC2:
  case TextureFormat::SRGB8_Punchthrough_A1_ETC2:
  case TextureFormat::SRGB8_A8_EAC_ETC2:
  case TextureFormat::RG_EAC_UNorm:
  case TextureFormat::RG_EAC_SNorm:
  case TextureFormat::R_EAC_UNorm:
  case TextureFormat::R_EAC_SNorm:
  case TextureFormat::YUV_NV12:
    return unsupported;
  }
}

} // namespace metal
} // namespace igl
