/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/DeviceFeatureSet.h>

#include <algorithm>
#include <optional>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/UniformArena.h>

namespace igl::webgpu {

namespace {

bool isFloat32Format(WGPUTextureFormat format) {
  return format == WGPUTextureFormat_R32Float || format == WGPUTextureFormat_RG32Float ||
         format == WGPUTextureFormat_RGBA32Float;
}

// Color-renderable and depth/stencil formats from the WebGPU texture format capability tables.
// Formats that need a device feature are only reached once that feature has been checked.
bool isRenderableFormat(WGPUTextureFormat format) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (format) {
  case WGPUTextureFormat_R8Unorm:
  case WGPUTextureFormat_RG8Unorm:
  case WGPUTextureFormat_RGBA8Unorm:
  case WGPUTextureFormat_RGBA8UnormSrgb:
  case WGPUTextureFormat_BGRA8Unorm:
  case WGPUTextureFormat_BGRA8UnormSrgb:
  case WGPUTextureFormat_R16Float:
  case WGPUTextureFormat_R16Uint:
  case WGPUTextureFormat_R16Unorm:
  case WGPUTextureFormat_RG16Float:
  case WGPUTextureFormat_RG16Uint:
  case WGPUTextureFormat_RG16Unorm:
  case WGPUTextureFormat_RGBA16Float:
  case WGPUTextureFormat_RGBA16Unorm:
  case WGPUTextureFormat_R32Float:
  case WGPUTextureFormat_R32Uint:
  case WGPUTextureFormat_RG32Float:
  case WGPUTextureFormat_RGBA32Float:
  case WGPUTextureFormat_RGBA32Uint:
  case WGPUTextureFormat_RGB10A2Unorm:
  case WGPUTextureFormat_RGB10A2Uint:
  case WGPUTextureFormat_Stencil8:
  case WGPUTextureFormat_Depth16Unorm:
  case WGPUTextureFormat_Depth24Plus:
  case WGPUTextureFormat_Depth24PlusStencil8:
  case WGPUTextureFormat_Depth32Float:
  case WGPUTextureFormat_Depth32FloatStencil8:
    return true;
  default:
    return false;
  }
}

// Formats with the STORAGE_BINDING capability in core WebGPU.
bool isStorageFormat(WGPUTextureFormat format) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (format) {
  case WGPUTextureFormat_RGBA8Unorm:
  case WGPUTextureFormat_RGBA8Snorm:
  case WGPUTextureFormat_RGBA8Uint:
  case WGPUTextureFormat_RGBA8Sint:
  case WGPUTextureFormat_RGBA16Uint:
  case WGPUTextureFormat_RGBA16Sint:
  case WGPUTextureFormat_RGBA16Float:
  case WGPUTextureFormat_R32Uint:
  case WGPUTextureFormat_R32Sint:
  case WGPUTextureFormat_R32Float:
  case WGPUTextureFormat_RG32Uint:
  case WGPUTextureFormat_RG32Sint:
  case WGPUTextureFormat_RG32Float:
  case WGPUTextureFormat_RGBA32Uint:
  case WGPUTextureFormat_RGBA32Sint:
  case WGPUTextureFormat_RGBA32Float:
    return true;
  default:
    return false;
  }
}

} // namespace

DeviceFeatureSet::DeviceFeatureSet(WGPUDevice IGL_NULLABLE device) : device_(device) {
  limitsValid_ = device_ != nullptr && wgpuDeviceGetLimits(device_, &limits_) == WGPUStatus_Success;
  if (!limitsValid_) {
    IGL_LOG_ERROR("wgpuDeviceGetLimits() failed\n");
  }
}

bool DeviceFeatureSet::hasWGPUFeature(WGPUFeatureName feature) const {
  return device_ != nullptr && wgpuDeviceHasFeature(device_, feature) != 0;
}

bool DeviceFeatureSet::hasFeature(DeviceFeatures feature) const {
  switch (feature) {
  case DeviceFeatures::BindBytes:
  case DeviceFeatures::Compute:
  case DeviceFeatures::CopyBuffer:
  case DeviceFeatures::DepthCompare:
  case DeviceFeatures::DepthShaderRead:
  case DeviceFeatures::DrawFirstIndexFirstVertex:
  case DeviceFeatures::DrawIndexedIndirect:
  case DeviceFeatures::DrawInstanced:
  case DeviceFeatures::ExplicitBinding:
  case DeviceFeatures::FillBuffer:
  case DeviceFeatures::MapBufferRange:
  case DeviceFeatures::MinMaxBlend:
  case DeviceFeatures::MultiSample:
  case DeviceFeatures::MultiSampleResolve:
  case DeviceFeatures::MultipleRenderTargets:
  case DeviceFeatures::PushConstants:
  case DeviceFeatures::SamplerMinMaxLod:
  case DeviceFeatures::ShaderLibrary:
  case DeviceFeatures::ShaderTextureLod:
  case DeviceFeatures::SRGB:
  case DeviceFeatures::StandardDerivative:
  case DeviceFeatures::StorageBuffers:
  case DeviceFeatures::TextureFilterAnisotropic:
  case DeviceFeatures::TextureFloat:
  case DeviceFeatures::TextureFormatRG:
  case DeviceFeatures::TextureHalfFloat:
  case DeviceFeatures::Texture2DArray:
  case DeviceFeatures::Texture3D:
  case DeviceFeatures::TextureNotPot:
  case DeviceFeatures::TexturePartialMipChain:
  case DeviceFeatures::TextureViews:
  case DeviceFeatures::UniformBlocks:
  case DeviceFeatures::ValidationLayersEnabled:
  // Encoder overrides select a pipeline variant (RenderCommandEncoder::setCullMode()).
  case DeviceFeatures::DynamicCullMode:
  case DeviceFeatures::DynamicFrontFacingWinding:
    return true;
  case DeviceFeatures::BindUniform:
  case DeviceFeatures::BufferDeviceAddress:
  case DeviceFeatures::BufferNoCopy:
  case DeviceFeatures::BufferRing:
  case DeviceFeatures::DynamicVertexBufferStride:
  case DeviceFeatures::ExplicitBindingExt:
  case DeviceFeatures::ExternalMemoryObjects:
  case DeviceFeatures::Indices8Bit:
  case DeviceFeatures::MeshShaders:
  case DeviceFeatures::Multiview:
  case DeviceFeatures::MultiViewMultisample:
  case DeviceFeatures::ReadWriteFramebuffer:
  case DeviceFeatures::ShaderTextureLodExt:
  case DeviceFeatures::SRGBSwapchain:
  case DeviceFeatures::SRGBWriteControl:
  case DeviceFeatures::StandardDerivativeExt:
  case DeviceFeatures::TextureArrayExt:
  case DeviceFeatures::TextureBindless:
  case DeviceFeatures::TextureExternalImage:
  case DeviceFeatures::TextureFormatRGB:
    return false;
  case DeviceFeatures::TimestampQueries:
  case DeviceFeatures::Timers:
    return hasWGPUFeature(WGPUFeatureName_TimestampQuery);
  }
  IGL_UNREACHABLE_RETURN(false)
}

bool DeviceFeatureSet::hasRequirement(DeviceRequirement /*requirement*/) const {
  return false;
}

bool DeviceFeatureSet::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  if (!limitsValid_) {
    result = 0;
    return false;
  }
  switch (featureLimits) {
  case DeviceFeatureLimits::BufferAlignment:
    result = limits_.minUniformBufferOffsetAlignment;
    return true;
  case DeviceFeatureLimits::ShaderStorageBufferOffsetAlignment:
    result = limits_.minStorageBufferOffsetAlignment;
    return true;
  case DeviceFeatureLimits::MaxTextureDimension1D2D:
    result = std::min(limits_.maxTextureDimension1D, limits_.maxTextureDimension2D);
    return true;
  case DeviceFeatureLimits::MaxCubeMapDimension:
    result = limits_.maxTextureDimension2D;
    return true;
  case DeviceFeatureLimits::MaxTextureDimension3D:
    result = limits_.maxTextureDimension3D;
    return true;
  case DeviceFeatureLimits::MaxVertexUniformVectors:
  case DeviceFeatureLimits::MaxFragmentUniformVectors:
  case DeviceFeatureLimits::MaxUniformBufferBytes:
    result = static_cast<size_t>(limits_.maxUniformBufferBindingSize);
    return true;
  case DeviceFeatureLimits::MaxStorageBufferBytes:
    result = static_cast<size_t>(limits_.maxStorageBufferBindingSize);
    return true;
  case DeviceFeatureLimits::MaxMultisampleCount:
    // WebGPU only has 1 and 4 samples.
    result = 4;
    return true;
  case DeviceFeatureLimits::MaxComputeWorkGroupSizeX:
    result = limits_.maxComputeWorkgroupSizeX;
    return true;
  case DeviceFeatureLimits::MaxComputeWorkGroupSizeY:
    result = limits_.maxComputeWorkgroupSizeY;
    return true;
  case DeviceFeatureLimits::MaxComputeWorkGroupSizeZ:
    result = limits_.maxComputeWorkgroupSizeZ;
    return true;
  case DeviceFeatureLimits::MaxComputeWorkGroupInvocations:
    result = limits_.maxComputeInvocationsPerWorkgroup;
    return true;
  case DeviceFeatureLimits::MaxVertexInputAttributes:
    result = limits_.maxVertexAttributes;
    return true;
  case DeviceFeatureLimits::MaxColorAttachments:
    result = limits_.maxColorAttachments;
    return true;
  case DeviceFeatureLimits::MaxBindBytesBytes:
    result = UniformArena::kMaxAllocationSize;
    return true;
  case DeviceFeatureLimits::MaxPushConstantBytes:
    result = kMaxPushConstantBytes;
    return true;
  case DeviceFeatureLimits::PushConstantsAlignment:
    result = 4;
    return true;
  case DeviceFeatureLimits::BufferNoCopyAlignment:
    result = 0;
    return true;
  case DeviceFeatureLimits::MaxDescriptorHeapCbvSrvUav:
  case DeviceFeatureLimits::MaxDescriptorHeapSamplers:
  case DeviceFeatureLimits::MaxDescriptorHeapRtvs:
  case DeviceFeatureLimits::MaxDescriptorHeapDsvs:
    result = 0;
    return false;
  }
  IGL_UNREACHABLE_RETURN(false)
}

ICapabilities::TextureFormatCapabilities DeviceFeatureSet::getTextureFormatCapabilities(
    TextureFormat format) const {
  using CapabilityBits = ICapabilities::TextureFormatCapabilityBits;

  const std::optional<WGPUTextureFormat> wgpuFormat = textureFormatToWGPUTextureFormat(format);
  if (!wgpuFormat) {
    return CapabilityBits::Unsupported;
  }
  if (const std::optional<WGPUFeatureName> feature = getRequiredWGPUFeature(format);
      feature && !hasWGPUFeature(*feature)) {
    return CapabilityBits::Unsupported;
  }

  const TextureFormatProperties props = TextureFormatProperties::fromTextureFormat(format);
  ICapabilities::TextureFormatCapabilities caps = CapabilityBits::Sampled;
  if (!props.isInteger() && !props.isDepthOrStencil() &&
      (!isFloat32Format(*wgpuFormat) || hasWGPUFeature(WGPUFeatureName_Float32Filterable))) {
    caps |= CapabilityBits::SampledFiltered;
  }
  const bool renderable = *wgpuFormat == WGPUTextureFormat_RG11B10Ufloat
                              ? hasWGPUFeature(WGPUFeatureName_RG11B10UfloatRenderable)
                              : isRenderableFormat(*wgpuFormat);
  if (renderable) {
    caps |= CapabilityBits::Attachment | CapabilityBits::SampledAttachment;
  }
  if (isStorageFormat(*wgpuFormat) || (*wgpuFormat == WGPUTextureFormat_BGRA8Unorm &&
                                       hasWGPUFeature(WGPUFeatureName_BGRA8UnormStorage))) {
    caps |= CapabilityBits::Storage;
  }
  return caps;
}

} // namespace igl::webgpu
