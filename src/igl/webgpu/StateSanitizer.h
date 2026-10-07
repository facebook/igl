/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <webgpu/webgpu.h>
#include <igl/Common.h>
#include <igl/DepthStencilState.h>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/VertexInputState.h>

namespace igl::webgpu {

class DeviceFeatureSet;

// Converts IGL state to WebGPU descriptors. Where IGL accepts state that WebGPU rejects but that
// cannot change the result, the state is adjusted; where the result would change, the functions
// return Unsupported (or ArgumentInvalid for invalid IGL usage) instead of letting WebGPU raise a
// validation error. Nothing here touches the GPU.

/// Blend factors are ignored by min/max blend operations, but WebGPU requires them to be One.
[[nodiscard]] Result makeBlendComponent(BlendOp op,
                                        BlendFactor srcFactor,
                                        BlendFactor dstFactor,
                                        WGPUBlendComponent& outComponent);

/// Color target of a pipeline. Blending is Unsupported on integer formats, and on 32-bit float
/// formats without `float32-blendable`; dual-source factors need `dual-source-blending`.
[[nodiscard]] Result makeColorTargetState(
    const RenderPipelineDesc::TargetDesc::ColorAttachment& attachment,
    const DeviceFeatureSet& features,
    WGPUColorTargetState& outTarget,
    WGPUBlendState& outBlend);

/// Depth/stencil state for an attachment of `format`. Aspects the format lacks get WebGPU's
/// defaults (their IGL state has no effect). WebGPU has one pair of stencil masks, taken from the
/// front face.
[[nodiscard]] Result makeDepthStencilState(const DepthStencilStateDesc& desc,
                                           WGPUTextureFormat format,
                                           WGPUDepthStencilState& outState);

/// WebGPU rejects depth bias for point and line topologies, where it has no effect in IGL either.
void applyDepthBias(PrimitiveType topology,
                    float depthBias,
                    float slopeScale,
                    float clamp,
                    WGPUDepthStencilState& inOutState);

/// Primitive state. Strip topologies take `stripIndexFormat` (Undefined for non-indexed draws).
[[nodiscard]] Result makePrimitiveState(const RenderPipelineDesc& desc,
                                        CullMode cullMode,
                                        WindingMode frontFaceWinding,
                                        WGPUIndexFormat stripIndexFormat,
                                        WGPUPrimitiveState& outState);

/// WebGPU has 1 or 4 samples. Alpha-to-coverage has no effect at 1 sample and is dropped there.
[[nodiscard]] Result makeMultisampleState(uint32_t sampleCount,
                                          bool alphaToCoverage,
                                          WGPUMultisampleState& outState);

/// Formats, strides (multiples of 4), attribute offsets (multiples of min(4, attribute size)) and
/// step modes WebGPU supports.
[[nodiscard]] Result validateVertexInputState(const VertexInputStateDesc& desc,
                                              const WGPULimits& limits);

/// Vertex buffer layouts indexed by IGL buffer index, up to the highest index an attribute uses.
/// `outAttributes` owns the attributes the layouts point to; attributes without an explicit
/// location use their index.
void makeVertexBufferLayouts(const VertexInputStateDesc& desc,
                             std::vector<WGPUVertexBufferLayout>& outLayouts,
                             std::vector<WGPUVertexAttribute>& outAttributes);

/// Anisotropic sampling requires linear min, mag and mip filters in WebGPU; otherwise it is
/// dropped (as on backends that ignore it with nearest filtering).
[[nodiscard]] uint16_t getMaxAnisotropy(const SamplerStateDesc& desc);

/// Dynamic buffer offsets must be multiples of 256 bytes.
[[nodiscard]] bool isDynamicOffsetAligned(uint64_t offset);

/// Buffer-to-buffer copies need 4-byte aligned offsets and size.
[[nodiscard]] Result validateBufferCopy(uint64_t srcOffset, uint64_t dstOffset, uint64_t size);

} // namespace igl::webgpu
