/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/StateSanitizer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/DeviceFeatureSet.h>

namespace igl::webgpu {

namespace {

constexpr uint64_t kDynamicOffsetAlignment = 256;

bool isDualSource(BlendFactor factor) {
  return factor == BlendFactor::Src1Color || factor == BlendFactor::OneMinusSrc1Color ||
         factor == BlendFactor::Src1Alpha || factor == BlendFactor::OneMinusSrc1Alpha;
}

bool isFloat32(WGPUTextureFormat format) {
  return format == WGPUTextureFormat_R32Float || format == WGPUTextureFormat_RG32Float ||
         format == WGPUTextureFormat_RGBA32Float;
}

bool hasDepthAspect(WGPUTextureFormat format) {
  return format == WGPUTextureFormat_Depth16Unorm || format == WGPUTextureFormat_Depth24Plus ||
         format == WGPUTextureFormat_Depth24PlusStencil8 ||
         format == WGPUTextureFormat_Depth32Float ||
         format == WGPUTextureFormat_Depth32FloatStencil8;
}

bool hasStencilAspect(WGPUTextureFormat format) {
  return format == WGPUTextureFormat_Stencil8 || format == WGPUTextureFormat_Depth24PlusStencil8 ||
         format == WGPUTextureFormat_Depth32FloatStencil8;
}

WGPUStencilFaceState makeStencilFace(const StencilStateDesc& desc) {
  return {
      .compare = compareFunctionToWGPUCompareFunction(desc.stencilCompareFunction),
      .failOp = stencilOperationToWGPUStencilOperation(desc.stencilFailureOperation),
      .depthFailOp = stencilOperationToWGPUStencilOperation(desc.depthFailureOperation),
      .passOp = stencilOperationToWGPUStencilOperation(desc.depthStencilPassOperation),
  };
}

constexpr WGPUStencilFaceState kDefaultStencilFace = {
    .compare = WGPUCompareFunction_Always,
    .failOp = WGPUStencilOperation_Keep,
    .depthFailOp = WGPUStencilOperation_Keep,
    .passOp = WGPUStencilOperation_Keep,
};

} // namespace

Result makeBlendComponent(BlendOp op,
                          BlendFactor srcFactor,
                          BlendFactor dstFactor,
                          WGPUBlendComponent& outComponent) {
  outComponent.operation = blendOpToWGPUBlendOperation(op);
  if (op == BlendOp::Min || op == BlendOp::Max) {
    outComponent.srcFactor = WGPUBlendFactor_One;
    outComponent.dstFactor = WGPUBlendFactor_One;
    return Result();
  }
  const std::optional<WGPUBlendFactor> src = blendFactorToWGPUBlendFactor(srcFactor);
  const std::optional<WGPUBlendFactor> dst = blendFactorToWGPUBlendFactor(dstFactor);
  if (!src || !dst) {
    return Result(Result::Code::Unsupported, "WebGPU has no blend factor for the blend alpha");
  }
  outComponent.srcFactor = *src;
  outComponent.dstFactor = *dst;
  return Result();
}

Result makeColorTargetState(const RenderPipelineDesc::TargetDesc::ColorAttachment& attachment,
                            const DeviceFeatureSet& features,
                            WGPUColorTargetState& outTarget,
                            WGPUBlendState& outBlend) {
  const std::optional<WGPUTextureFormat> format =
      textureFormatToWGPUTextureFormat(attachment.textureFormat);
  if (!format || (features.getTextureFormatCapabilities(attachment.textureFormat) &
                  ICapabilities::TextureFormatCapabilityBits::Attachment) == 0) {
    return Result(Result::Code::Unsupported, "The color attachment format is not renderable");
  }
  outTarget = WGPU_COLOR_TARGET_STATE_INIT;
  outTarget.format = *format;
  outTarget.writeMask = colorWriteMaskToWGPUColorWriteMask(attachment.colorWriteMask);
  outTarget.blend = nullptr;
  if (!attachment.blendEnabled) {
    return Result();
  }
  const auto props = TextureFormatProperties::fromTextureFormat(attachment.textureFormat);
  if (props.isInteger() ||
      (isFloat32(*format) && !features.hasWGPUFeature(WGPUFeatureName_Float32Blendable))) {
    return Result(Result::Code::Unsupported, "Blending is not supported for this format");
  }
  const bool dualSource =
      isDualSource(attachment.srcRGBBlendFactor) || isDualSource(attachment.dstRGBBlendFactor) ||
      isDualSource(attachment.srcAlphaBlendFactor) || isDualSource(attachment.dstAlphaBlendFactor);
  if (dualSource && !features.hasWGPUFeature(WGPUFeatureName_DualSourceBlending)) {
    return Result(Result::Code::Unsupported, "Dual-source blending is not supported");
  }
  Result result = makeBlendComponent(attachment.rgbBlendOp,
                                     attachment.srcRGBBlendFactor,
                                     attachment.dstRGBBlendFactor,
                                     outBlend.color);
  if (result.isOk()) {
    result = makeBlendComponent(attachment.alphaBlendOp,
                                attachment.srcAlphaBlendFactor,
                                attachment.dstAlphaBlendFactor,
                                outBlend.alpha);
  }
  if (result.isOk()) {
    outTarget.blend = &outBlend;
  }
  return result;
}

Result makeDepthStencilState(const DepthStencilStateDesc& desc,
                             WGPUTextureFormat format,
                             WGPUDepthStencilState& outState) {
  if (!hasDepthAspect(format) && !hasStencilAspect(format)) {
    return Result(Result::Code::ArgumentInvalid, "Not a depth/stencil format");
  }
  outState = WGPU_DEPTH_STENCIL_STATE_INIT;
  outState.format = format;
  if (hasDepthAspect(format)) {
    outState.depthCompare = compareFunctionToWGPUCompareFunction(desc.compareFunction);
    outState.depthWriteEnabled = desc.isDepthWriteEnabled ? WGPUOptionalBool_True
                                                          : WGPUOptionalBool_False;
  } else {
    outState.depthCompare = WGPUCompareFunction_Always;
    outState.depthWriteEnabled = WGPUOptionalBool_False;
  }
  if (hasStencilAspect(format)) {
    outState.stencilFront = makeStencilFace(desc.frontFaceStencil);
    outState.stencilBack = makeStencilFace(desc.backFaceStencil);
    if (desc.frontFaceStencil.readMask != desc.backFaceStencil.readMask ||
        desc.frontFaceStencil.writeMask != desc.backFaceStencil.writeMask) {
      IGL_LOG_INFO_ONCE("WebGPU has one pair of stencil masks; using the front face's\n");
    }
    outState.stencilReadMask = desc.frontFaceStencil.readMask;
    outState.stencilWriteMask = desc.frontFaceStencil.writeMask;
  } else {
    outState.stencilFront = kDefaultStencilFace;
    outState.stencilBack = kDefaultStencilFace;
  }
  return Result();
}

void applyDepthBias(PrimitiveType topology,
                    float depthBias,
                    float slopeScale,
                    float clamp,
                    WGPUDepthStencilState& inOutState) {
  const bool isTriangle = topology == PrimitiveType::Triangle ||
                          topology == PrimitiveType::TriangleStrip;
  // NaN and biases outside int32 would make the conversion undefined. 2147483520 is the largest
  // float below 2^31.
  constexpr float kMaxDepthBias = 2147483520.0f;
  const float bias = std::isnan(depthBias) ? 0.0f
                                           : std::clamp(depthBias, -kMaxDepthBias, kMaxDepthBias);
  inOutState.depthBias = isTriangle ? static_cast<int32_t>(std::lround(bias)) : 0;
  inOutState.depthBiasSlopeScale = isTriangle ? slopeScale : 0.0f;
  inOutState.depthBiasClamp = isTriangle ? clamp : 0.0f;
}

Result makePrimitiveState(const RenderPipelineDesc& desc,
                          CullMode cullMode,
                          WindingMode frontFaceWinding,
                          WGPUIndexFormat stripIndexFormat,
                          WGPUPrimitiveState& outState) {
  if (desc.polygonFillMode != PolygonFillMode::Fill) {
    return Result(Result::Code::Unsupported, "WebGPU only fills polygons");
  }
  const bool isStrip = desc.topology == PrimitiveType::LineStrip ||
                       desc.topology == PrimitiveType::TriangleStrip;
  outState = WGPU_PRIMITIVE_STATE_INIT;
  outState.topology = primitiveTypeToWGPUPrimitiveTopology(desc.topology);
  outState.stripIndexFormat = isStrip ? stripIndexFormat : WGPUIndexFormat_Undefined;
  outState.frontFace = windingModeToWGPUFrontFace(frontFaceWinding);
  outState.cullMode = cullModeToWGPUCullMode(cullMode);
  return Result();
}

Result makeMultisampleState(uint32_t sampleCount,
                            bool alphaToCoverage,
                            WGPUMultisampleState& outState) {
  if (sampleCount != 1 && sampleCount != 4) {
    return Result(Result::Code::Unsupported, "WebGPU pipelines have 1 or 4 samples");
  }
  outState = WGPU_MULTISAMPLE_STATE_INIT;
  outState.count = sampleCount;
  outState.mask = ~0u;
  outState.alphaToCoverageEnabled = alphaToCoverage && sampleCount > 1 ? 1u : 0u;
  return Result();
}

// Like the other backends, attributes select their input binding by buffer index;
// numInputBindings does not bound the indices.
Result validateVertexInputState(const VertexInputStateDesc& desc, const WGPULimits& limits) {
  if (desc.numAttributes > IGL_VERTEX_ATTRIBUTES_MAX ||
      desc.numAttributes > limits.maxVertexAttributes ||
      desc.numInputBindings > IGL_BUFFER_BINDINGS_MAX ||
      desc.numInputBindings > limits.maxVertexBuffers) {
    return Result(Result::Code::ArgumentOutOfRange, "Too many vertex attributes or buffers");
  }
  std::array<bool, IGL_VERTEX_ATTRIBUTES_MAX> usedLocations = {};
  for (size_t i = 0; i < desc.numAttributes; ++i) {
    const VertexAttribute& attribute = desc.attributes[i];
    if (!vertexAttributeFormatToWGPUVertexFormat(attribute.format)) {
      return Result(Result::Code::Unsupported, "Vertex format is not supported by WebGPU");
    }
    // Same fallback as makeVertexBufferLayouts(): a negative location means the attribute index.
    const size_t location = attribute.location >= 0 ? static_cast<size_t>(attribute.location) : i;
    if (location >= IGL_VERTEX_ATTRIBUTES_MAX || location >= limits.maxVertexAttributes) {
      return Result(Result::Code::ArgumentOutOfRange, "Vertex attribute location is out of range");
    }
    if (usedLocations[location]) {
      return Result(Result::Code::ArgumentInvalid, "Vertex attributes share a shader location");
    }
    usedLocations[location] = true;
    if (attribute.bufferIndex >= IGL_BUFFER_BINDINGS_MAX ||
        attribute.bufferIndex >= limits.maxVertexBuffers) {
      return Result(Result::Code::ArgumentOutOfRange, "Vertex buffer index is out of range");
    }
    const VertexInputBinding& binding = desc.inputBindings[attribute.bufferIndex];
    if (binding.stride % 4 != 0 || binding.stride > limits.maxVertexBufferArrayStride) {
      return Result(Result::Code::Unsupported,
                    "WebGPU vertex strides are multiples of 4 bytes up to the stride limit");
    }
    if (binding.sampleFunction == VertexSampleFunction::Constant || binding.sampleRate != 1) {
      return Result(Result::Code::Unsupported,
                    "WebGPU vertex buffers step once per vertex or per instance");
    }
    const size_t size = VertexInputStateDesc::sizeForVertexAttributeFormat(attribute.format);
    if (attribute.offset % std::min<size_t>(4, size) != 0) {
      return Result(Result::Code::Unsupported, "Vertex attribute offset is not aligned");
    }
    if (binding.stride != 0 && attribute.offset + size > binding.stride) {
      return Result(Result::Code::ArgumentOutOfRange, "Vertex attribute exceeds its stride");
    }
  }
  return Result();
}

void makeVertexBufferLayouts(const VertexInputStateDesc& desc,
                             std::vector<WGPUVertexBufferLayout>& outLayouts,
                             std::vector<WGPUVertexAttribute>& outAttributes) {
  // Attributes with a buffer index of IGL_BUFFER_BINDINGS_MAX or more are skipped, so inputBindings
  // is never read past its end.
  const size_t numAttributes = std::min<size_t>(desc.numAttributes, IGL_VERTEX_ATTRIBUTES_MAX);
  std::array<uint32_t, IGL_BUFFER_BINDINGS_MAX> counts = {};
  size_t numBuffers = 0;
  for (size_t i = 0; i < numAttributes; ++i) {
    const size_t buffer = desc.attributes[i].bufferIndex;
    if (buffer < IGL_BUFFER_BINDINGS_MAX) {
      ++counts[buffer];
      numBuffers = std::max(numBuffers, buffer + 1);
    }
  }
  outAttributes.clear();
  outAttributes.reserve(numAttributes);
  outLayouts.assign(numBuffers, WGPU_VERTEX_BUFFER_LAYOUT_INIT);
  // Attributes are grouped by buffer so each layout points at a contiguous run.
  for (size_t buffer = 0; buffer < numBuffers; ++buffer) {
    if (counts[buffer] == 0) {
      // A slot without attributes is unused: Undefined step mode, no attributes, zero stride.
      continue;
    }
    const size_t first = outAttributes.size();
    for (size_t i = 0; i < numAttributes; ++i) {
      const VertexAttribute& attribute = desc.attributes[i];
      if (attribute.bufferIndex != buffer) {
        continue;
      }
      WGPUVertexAttribute wgpuAttribute = WGPU_VERTEX_ATTRIBUTE_INIT;
      wgpuAttribute.format =
          vertexAttributeFormatToWGPUVertexFormat(attribute.format).value_or(WGPUVertexFormat{});
      wgpuAttribute.offset = attribute.offset;
      wgpuAttribute.shaderLocation = attribute.location >= 0
                                         ? static_cast<uint32_t>(attribute.location)
                                         : static_cast<uint32_t>(i);
      outAttributes.push_back(wgpuAttribute);
    }
    const VertexInputBinding& binding = desc.inputBindings[buffer];
    WGPUVertexBufferLayout& layout = outLayouts[buffer];
    layout.stepMode = binding.sampleFunction == VertexSampleFunction::Instance
                          ? WGPUVertexStepMode_Instance
                          : WGPUVertexStepMode_Vertex;
    layout.arrayStride = binding.stride;
    layout.attributeCount = counts[buffer];
    layout.attributes = outAttributes.data() + first;
  }
}

uint16_t getMaxAnisotropy(const SamplerStateDesc& desc) {
  const auto maxAnisotropy = static_cast<uint16_t>(std::clamp<int>(desc.maxAnisotropic, 1, 16));
  const bool allLinear = desc.minFilter == SamplerMinMagFilter::Linear &&
                         desc.magFilter == SamplerMinMagFilter::Linear &&
                         desc.mipFilter == SamplerMipFilter::Linear;
  if (maxAnisotropy > 1 && !allLinear) {
    IGL_LOG_INFO_ONCE("WebGPU ignores maxAnisotropic unless all sampler filters are linear\n");
    return 1;
  }
  return maxAnisotropy;
}

bool isDynamicOffsetAligned(uint64_t offset) {
  return offset % kDynamicOffsetAlignment == 0;
}

Result validateBufferCopy(uint64_t srcOffset, uint64_t dstOffset, uint64_t size) {
  if (srcOffset % 4 != 0 || dstOffset % 4 != 0 || size % 4 != 0) {
    return Result(Result::Code::ArgumentInvalid,
                  "WebGPU buffer copies need 4-byte aligned offsets and size");
  }
  return Result();
}

} // namespace igl::webgpu
