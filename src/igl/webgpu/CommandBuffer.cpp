/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/CommandBuffer.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
#include <vector>
#include <igl/ComputeCommandEncoder.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/ComputeCommandEncoder.h>
#include <igl/webgpu/Device.h>
#include <igl/webgpu/RenderCommandEncoder.h>
#include <igl/webgpu/StateSanitizer.h>
#include <igl/webgpu/Texture.h>
#include <igl/webgpu/Timer.h>
#include <igl/webgpu/TimestampQueries.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

CommandBuffer::CommandBuffer(Device& device, CommandBufferDesc desc) :
  ICommandBuffer(std::move(desc)),
  device_(device),
  ctx_(device.getContext()),
  serial_(ctx_.getResourceTracker().openCommandBuffer()),
  uniformArena_(ctx_) {
  const WGPUCommandEncoderDescriptor encoderDesc = {
      .nextInChain = nullptr,
      .label = toWGPUStringView(this->desc.debugName),
  };
  encoder_.reset(wgpuDeviceCreateCommandEncoder(ctx_.getDevice(), &encoderDesc));
  if (this->desc.timer) {
    static_cast<Timer&>(*this->desc.timer).begin();
  }
  if (this->desc.timestampQueries) {
    timestampQueries_.push_back(
        std::static_pointer_cast<TimestampQueries>(this->desc.timestampQueries));
  }
}

CommandBuffer::~CommandBuffer() {
  if (!submitted_) {
    ctx_.getResourceTracker().closeCommandBuffer(serial_);
  }
}

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& /*dependencies*/,
    Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (submitted_) {
    Result::setResult(
        outResult, Result::Code::InvalidOperation, "The command buffer was already submitted");
    return nullptr;
  }
  return RenderCommandEncoder::create(shared_from_this(), renderPass, framebuffer, outResult);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (submitted_) {
    IGL_LOG_ERROR("createComputeCommandEncoder(): the command buffer was already submitted\n");
    return nullptr;
  }
  return std::make_unique<ComputeCommandEncoder>(shared_from_this(), ComputePassDesc{});
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder(
    const ComputePassDesc& computePass) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (submitted_) {
    IGL_LOG_ERROR("createComputeCommandEncoder(): the command buffer was already submitted\n");
    return nullptr;
  }
  return std::make_unique<ComputeCommandEncoder>(shared_from_this(), computePass);
}

std::optional<WGPUPassTimestampWrites> CommandBuffer::getPassTimestampWrites(
    const std::shared_ptr<ITimestampQueries>& queries,
    uint32_t slotIndex) {
  if (queries) {
    auto webgpuQueries = std::static_pointer_cast<TimestampQueries>(queries);
    if (std::find(timestampQueries_.begin(), timestampQueries_.end(), webgpuQueries) ==
        timestampQueries_.end()) {
      timestampQueries_.push_back(webgpuQueries);
    }
    if (desc.timer) {
      IGL_LOG_INFO_ONCE("A pass with its own timestamp queries is not measured by the timer\n");
    }
    return webgpuQueries->getPassTimestampWrites(slotIndex);
  }
  if (desc.timer) {
    return static_cast<Timer&>(*desc.timer).nextPass();
  }
  return std::nullopt;
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& surface) const {
  if (surface && static_cast<const Texture&>(*surface).isDeferred()) {
    presented_.push_back(surface);
  }
}

void CommandBuffer::waitUntilScheduled() {}

void CommandBuffer::waitUntilCompleted() {
  // Browser builds without JSPI cannot wait; submitted work completes on its own.
  if (!submitted_ || (ctx_.isSuspensionAllowed() && !ctx_.canWait())) {
    return;
  }
  const Result result = ctx_.waitForSubmittedWork();
  if (!result.isOk()) {
    IGL_LOG_ERROR("waitUntilCompleted(): %s\n", result.message.c_str());
  }
}

void CommandBuffer::pushDebugGroupLabel(const char* IGL_NONNULL label,
                                        const Color& /*color*/) const {
  wgpuCommandEncoderPushDebugGroup(encoder_.get(), toWGPUStringView(label));
}

void CommandBuffer::popDebugGroupLabel() const {
  wgpuCommandEncoderPopDebugGroup(encoder_.get());
}

void CommandBuffer::copyBuffer(IBuffer& src,
                               IBuffer& dst,
                               uint64_t srcOffset,
                               uint64_t dstOffset,
                               uint64_t size) {
  auto& srcBuffer = static_cast<Buffer&>(src);
  auto& dstBuffer = static_cast<Buffer&>(dst);
  // Buffers are allocated in whole words, so a copy that ends mid-word at the end of the
  // destination can also copy the padding.
  const uint64_t paddedSize = (size + 3) & ~uint64_t{3};
  if (dstOffset + size == dstBuffer.getSizeInBytes() &&
      srcOffset + paddedSize <= srcBuffer.getAllocatedSize() &&
      dstOffset + paddedSize <= dstBuffer.getAllocatedSize()) {
    size = paddedSize;
  }
  if (const Result result = validateBufferCopy(srcOffset, dstOffset, size); !result.isOk()) {
    IGL_LOG_ERROR("copyBuffer(): %s\n", result.message.c_str());
    return;
  }
  if (submitted_ || size == 0) {
    return;
  }
  wgpuCommandEncoderCopyBufferToBuffer(encoder_.get(),
                                       srcBuffer.getWGPUBuffer(),
                                       srcOffset,
                                       dstBuffer.getWGPUBuffer(),
                                       dstOffset,
                                       size);
  srcBuffer.recordUse(serial_, /*gpuWrite=*/false);
  dstBuffer.recordUse(serial_, /*gpuWrite=*/true);
}

void CommandBuffer::fillBuffer(IBuffer& buffer, const BufferRange& range, uint8_t value) {
  auto& dst = static_cast<Buffer&>(buffer);
  if (range.offset % 4 != 0 || range.size % 4 != 0 || range.offset > dst.getAllocatedSize() ||
      range.size > dst.getAllocatedSize() - range.offset) {
    IGL_LOG_ERROR("fillBuffer(): the range must be in bounds and 4-byte aligned\n");
    return;
  }
  if (submitted_ || range.size == 0) {
    return;
  }
  if (value == 0) {
    wgpuCommandEncoderClearBuffer(encoder_.get(), dst.getWGPUBuffer(), range.offset, range.size);
  } else {
    // WebGPU only clears to zero; other values come from a staging buffer copied in order with
    // the rest of the command buffer.
    const WGPUBufferDescriptor stagingDesc = {
        .nextInChain = nullptr,
        .label = toWGPUStringView("igl.webgpu.fillBuffer"),
        .usage = WGPUBufferUsage_CopySrc,
        .size = range.size,
        .mappedAtCreation = 1,
    };
    const Handle<WGPUBuffer> staging(wgpuDeviceCreateBuffer(ctx_.getDevice(), &stagingDesc));
    void* mapped = wgpuBufferGetMappedRange(staging.get(), 0, range.size);
    if (mapped == nullptr) {
      IGL_LOG_ERROR("fillBuffer(): cannot map the staging buffer\n");
      return;
    }
    std::memset(mapped, value, range.size);
    wgpuBufferUnmap(staging.get());
    wgpuCommandEncoderCopyBufferToBuffer(
        encoder_.get(), staging.get(), 0, dst.getWGPUBuffer(), range.offset, range.size);
  }
  dst.recordUse(serial_, /*gpuWrite=*/true);
}

void CommandBuffer::copyTextureToBuffer(ITexture& src,
                                        IBuffer& dst,
                                        uint64_t dstOffset,
                                        uint32_t level,
                                        uint32_t layer) {
  if (submitted_) {
    return;
  }
  const auto& texture = static_cast<const Texture&>(src);
  auto& buffer = static_cast<Buffer&>(dst);
  if (texture.getWGPUTexture() == nullptr) {
    IGL_LOG_ERROR("copyTextureToBuffer(): the surface texture could not be acquired\n");
    return;
  }
  const TextureFormatProperties& props = src.getProperties();
  const WGPUTextureAspect aspect =
      props.hasDepth() && props.hasStencil() ? WGPUTextureAspect_DepthOnly : WGPUTextureAspect_All;
  const uint32_t bytesPerTexel = getCopyBytesPerTexel(texture.getWGPUFormat(), aspect);
  if (bytesPerTexel == 0 || src.getSamples() != 1) {
    IGL_LOG_ERROR("copyTextureToBuffer(): this texture cannot be copied on WebGPU\n");
    return;
  }
  if (dstOffset % 4 != 0 || dstOffset % bytesPerTexel != 0) {
    IGL_LOG_ERROR(
        "copyTextureToBuffer(): dstOffset must be a multiple of 4 and of the texel size\n");
    return;
  }
  const TextureRangeDesc range = src.getFullRange(level);
  const uint32_t width = range.width;
  const uint32_t height = range.height;
  const uint32_t depthOrFaces = src.getType() == TextureType::Cube ? 6u : range.depth;
  const uint64_t tightBytesPerRow = static_cast<uint64_t>(width) * bytesPerTexel;
  const uint64_t imageBytes = tightBytesPerRow * height;
  // Against the caller-visible size: the allocation is padded to whole words.
  if (dstOffset + imageBytes * depthOrFaces > buffer.getSizeInBytes()) {
    IGL_LOG_ERROR("copyTextureToBuffer(): the buffer is too small\n");
    return;
  }

  const bool isCube = src.getType() == TextureType::Cube;
  const bool is3D = src.getType() == TextureType::ThreeD;
  std::vector<WGPUTexelCopyTextureInfo> sources(depthOrFaces);
  for (uint32_t slice = 0; slice < depthOrFaces; ++slice) {
    sources[slice] = {
        .texture = texture.getWGPUTexture(),
        .mipLevel = texture.getBaseMipLevel() + level,
        .origin = {.x = 0,
                   .y = 0,
                   .z = is3D ? slice : texture.getWGPULayer(layer, isCube ? slice : 0)},
        .aspect = aspect,
    };
  }
  const WGPUExtent3D extent = {.width = width, .height = height, .depthOrArrayLayers = 1};
  if (tightBytesPerRow % 256 == 0) {
    for (uint32_t slice = 0; slice < depthOrFaces; ++slice) {
      const WGPUTexelCopyBufferInfo destination = {
          .layout = {.offset = dstOffset + imageBytes * slice,
                     .bytesPerRow = static_cast<uint32_t>(tightBytesPerRow),
                     .rowsPerImage = height},
          .buffer = buffer.getWGPUBuffer(),
      };
      wgpuCommandEncoderCopyTextureToBuffer(encoder_.get(), &sources[slice], &destination, &extent);
    }
  } else if (!copyUnalignedRowsToBuffer(sources, extent, tightBytesPerRow, buffer, dstOffset)) {
    return;
  }
  texture.recordUse(serial_);
  buffer.recordUse(serial_, /*gpuWrite=*/true);
}

bool CommandBuffer::copyUnalignedRowsToBuffer(const std::vector<WGPUTexelCopyTextureInfo>& sources,
                                              const WGPUExtent3D& extent,
                                              uint64_t tightBytesPerRow,
                                              Buffer& buffer,
                                              uint64_t dstOffset) {
  // Buffer rows of texture copies must be 256-byte aligned, and depth and stencil copies must cover
  // whole subresources, so slices are copied with padded rows into a staging buffer and packed from
  // there. Buffer-to-buffer copies need 4-byte aligned offsets and sizes; other rows are packed by
  // a compute pass.
  const uint64_t numSlices = sources.size();
  const uint64_t paddedBytesPerRow = (tightBytesPerRow + 255) / 256 * 256;
  const uint64_t paddedBytesPerImage = paddedBytesPerRow * extent.height;
  const uint64_t tightBytesPerImage = tightBytesPerRow * extent.height;
  const uint64_t totalBytes = tightBytesPerImage * numSlices;
  const uint64_t packedBytes = (totalBytes + 3) / 4 * 4;
  const bool packOnGpu = tightBytesPerRow % 4 != 0;
  WGPULimits limits = WGPU_LIMITS_INIT;
  if (wgpuDeviceGetLimits(ctx_.getDevice(), &limits) != WGPUStatus_Success ||
      paddedBytesPerImage * numSlices > limits.maxBufferSize ||
      (packOnGpu && (paddedBytesPerImage * numSlices > limits.maxStorageBufferBindingSize ||
                     packedBytes > limits.maxStorageBufferBindingSize))) {
    IGL_LOG_ERROR("copyTextureToBuffer(): the texture is too large to repack\n");
    return false;
  }

  WGPUBufferDescriptor stagingDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
  stagingDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc | WGPUBufferUsage_Storage;
  stagingDesc.size = paddedBytesPerImage * numSlices;
  const Handle<WGPUBuffer> staging(wgpuDeviceCreateBuffer(ctx_.getDevice(), &stagingDesc));
  for (size_t slice = 0; slice < sources.size(); ++slice) {
    const WGPUTexelCopyBufferInfo destination = {
        .layout = {.offset = paddedBytesPerImage * slice,
                   .bytesPerRow = static_cast<uint32_t>(paddedBytesPerRow),
                   .rowsPerImage = extent.height},
        .buffer = staging.get(),
    };
    wgpuCommandEncoderCopyTextureToBuffer(encoder_.get(), &sources[slice], &destination, &extent);
  }

  if (!packOnGpu) {
    for (uint64_t slice = 0; slice < numSlices; ++slice) {
      for (uint32_t row = 0; row < extent.height; ++row) {
        wgpuCommandEncoderCopyBufferToBuffer(
            encoder_.get(),
            staging.get(),
            paddedBytesPerImage * slice + paddedBytesPerRow * row,
            buffer.getWGPUBuffer(),
            dstOffset + tightBytesPerImage * slice + tightBytesPerRow * row,
            tightBytesPerRow);
      }
    }
    return true;
  }

  const WGPUComputePipeline pipeline = ctx_.getRowPackPipeline();
  if (pipeline == nullptr) {
    IGL_LOG_ERROR("copyTextureToBuffer(): the row packing pipeline is unavailable\n");
    return false;
  }
  WGPUBufferDescriptor packedDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
  packedDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc | WGPUBufferUsage_Storage;
  packedDesc.size = packedBytes;
  const Handle<WGPUBuffer> packed(wgpuDeviceCreateBuffer(ctx_.getDevice(), &packedDesc));
  if (packedBytes != totalBytes) {
    // The bytes of the destination that share the last word with the copy are preserved.
    wgpuCommandEncoderCopyBufferToBuffer(encoder_.get(),
                                         buffer.getWGPUBuffer(),
                                         dstOffset + packedBytes - 4,
                                         packed.get(),
                                         packedBytes - 4,
                                         4);
  }
  const std::array<uint32_t, 8> params = {static_cast<uint32_t>(tightBytesPerRow),
                                          static_cast<uint32_t>(paddedBytesPerRow),
                                          static_cast<uint32_t>(tightBytesPerImage),
                                          static_cast<uint32_t>(paddedBytesPerImage),
                                          static_cast<uint32_t>(totalBytes)};
  WGPUBufferDescriptor paramsDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
  paramsDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  paramsDesc.size = sizeof(params);
  const Handle<WGPUBuffer> paramsBuffer(wgpuDeviceCreateBuffer(ctx_.getDevice(), &paramsDesc));
  wgpuQueueWriteBuffer(ctx_.getQueue(), paramsBuffer.get(), 0, params.data(), sizeof(params));

  const std::array<WGPUBindGroupEntry, 3> entries = {
      WGPUBindGroupEntry{.binding = 0, .buffer = paramsBuffer.get(), .size = sizeof(params)},
      WGPUBindGroupEntry{.binding = 1, .buffer = staging.get(), .size = stagingDesc.size},
      WGPUBindGroupEntry{.binding = 2, .buffer = packed.get(), .size = packedBytes},
  };
  const Handle<WGPUBindGroupLayout> layout(wgpuComputePipelineGetBindGroupLayout(pipeline, 0));
  WGPUBindGroupDescriptor bindGroupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
  bindGroupDesc.layout = layout.get();
  bindGroupDesc.entryCount = entries.size();
  bindGroupDesc.entries = entries.data();
  const Handle<WGPUBindGroup> bindGroup(
      wgpuDeviceCreateBindGroup(ctx_.getDevice(), &bindGroupDesc));

  constexpr uint64_t kWorkgroupSize = 64;
  constexpr uint64_t kMaxWorkgroups = 65535;
  const uint64_t workgroups = (packedBytes / 4 + kWorkgroupSize - 1) / kWorkgroupSize;
  const auto workgroupsX = static_cast<uint32_t>(std::min(workgroups, kMaxWorkgroups));
  const auto workgroupsY = static_cast<uint32_t>((workgroups + workgroupsX - 1) / workgroupsX);
  const Handle<WGPUComputePassEncoder> pass(
      wgpuCommandEncoderBeginComputePass(encoder_.get(), nullptr));
  wgpuComputePassEncoderSetPipeline(pass.get(), pipeline);
  wgpuComputePassEncoderSetBindGroup(pass.get(), 0, bindGroup.get(), 0, nullptr);
  wgpuComputePassEncoderDispatchWorkgroups(pass.get(), workgroupsX, workgroupsY, 1);
  wgpuComputePassEncoderEnd(pass.get());

  wgpuCommandEncoderCopyBufferToBuffer(
      encoder_.get(), packed.get(), 0, buffer.getWGPUBuffer(), dstOffset, packedBytes);
  return true;
}

Result CommandBuffer::submit() {
  if (submitted_) {
    return Result(Result::Code::InvalidOperation, "The command buffer was already submitted");
  }
  submitted_ = true;
  for (const auto& queries : timestampQueries_) {
    queries->encodeResolve(encoder_.get());
  }
  if (desc.timer) {
    static_cast<Timer&>(*desc.timer).encodeResolve(encoder_.get());
  }
  const Handle<WGPUCommandBuffer> commands(wgpuCommandEncoderFinish(encoder_.get(), nullptr));
  encoder_ = nullptr;
  const WGPUCommandBuffer rawCommands = commands.get();
  uniformArena_.flush();
  wgpuQueueSubmit(ctx_.getQueue(), 1, &rawCommands);
  uniformArena_.releaseChunks();
  ctx_.getResourceTracker().closeCommandBuffer(serial_);
  for (const auto& queries : timestampQueries_) {
    queries->onSubmitted();
  }
  if (desc.timer) {
    static_cast<Timer&>(*desc.timer).onSubmitted();
  }
  for (const auto& texture : presented_) {
    static_cast<const Texture&>(*texture).present();
  }
  presented_.clear();
  return Result();
}

} // namespace igl::webgpu
