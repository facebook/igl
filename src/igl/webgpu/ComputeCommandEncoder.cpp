/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/ComputeCommandEncoder.h>

#include <optional>
#include <utility>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/CommandBuffer.h>
#include <igl/webgpu/ComputePipelineState.h>
#include <igl/webgpu/Device.h>
#include <igl/webgpu/SamplerState.h>
#include <igl/webgpu/Texture.h>

namespace igl::webgpu {

ComputeCommandEncoder::ComputeCommandEncoder(std::shared_ptr<CommandBuffer> commandBuffer,
                                             const ComputePassDesc& computePass) :
  commandBuffer_(std::move(commandBuffer)),
  binder_(commandBuffer_->getDevice().getContext(),
          commandBuffer_->getDevice().getDeviceFeatureSet()) {
  const std::optional<WGPUPassTimestampWrites> timestampWrites =
      commandBuffer_->getPassTimestampWrites(computePass.timestampQuery.queries,
                                             computePass.timestampQuery.slotIndex);
  WGPUComputePassDescriptor passDesc = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
  passDesc.timestampWrites = timestampWrites ? &*timestampWrites : nullptr;
  pass_.reset(
      wgpuCommandEncoderBeginComputePass(commandBuffer_->getWGPUCommandEncoder(), &passDesc));
}

ComputeCommandEncoder::~ComputeCommandEncoder() {
  // WebGPU cannot finish a command encoder with an open pass.
  endEncoding();
}

void ComputeCommandEncoder::endEncoding() {
  if (pass_) {
    wgpuComputePassEncoderEnd(pass_.get());
    pass_ = nullptr;
  }
}

void ComputeCommandEncoder::pushDebugGroupLabel(const char* IGL_NONNULL label,
                                                const Color& /*color*/) const {
  if (pass_) {
    wgpuComputePassEncoderPushDebugGroup(pass_.get(), toWGPUStringView(label));
  }
}

void ComputeCommandEncoder::insertDebugEventLabel(const char* IGL_NONNULL label,
                                                  const Color& /*color*/) const {
  if (pass_) {
    wgpuComputePassEncoderInsertDebugMarker(pass_.get(), toWGPUStringView(label));
  }
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  if (pass_) {
    wgpuComputePassEncoderPopDebugGroup(pass_.get());
  }
}

void ComputeCommandEncoder::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  pipeline_ = std::static_pointer_cast<ComputePipelineState>(pipelineState);
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/,
                                        const void* IGL_NULLABLE /*data*/) {
  IGL_LOG_ERROR_ONCE("bindUniform() is OpenGL-only; use uniform buffers on WebGPU\n");
}

void ComputeCommandEncoder::bindTexture(uint32_t index, ITexture* IGL_NULLABLE texture) {
  binder_.bindTexture(index, static_cast<Texture*>(texture));
}

void ComputeCommandEncoder::bindImageTexture(uint32_t index,
                                             ITexture* IGL_NULLABLE texture,
                                             TextureFormat /*format*/) {
  binder_.bindStorageTexture(index, static_cast<Texture*>(texture));
}

void ComputeCommandEncoder::bindSamplerState(uint32_t index,
                                             ISamplerState* IGL_NULLABLE samplerState) {
  binder_.bindSampler(index, static_cast<SamplerState*>(samplerState));
}

void ComputeCommandEncoder::bindBuffer(uint32_t index,
                                       IBuffer* IGL_NULLABLE buffer,
                                       size_t offset,
                                       size_t bufferSize) {
  binder_.bindBuffer(index, static_cast<Buffer*>(buffer), offset, bufferSize);
}

void ComputeCommandEncoder::bindBytes(uint32_t index,
                                      const void* IGL_NULLABLE data,
                                      size_t length) {
  if (data == nullptr || length == 0 || index >= IGL_BUFFER_BINDINGS_MAX) {
    return;
  }
  const UniformArena::Slice slice = commandBuffer_->getUniformArena().allocate(
      static_cast<const void* IGL_NONNULL>(data), length);
  if (slice.buffer == nullptr) {
    if (length > UniformArena::kMaxAllocationSize) {
      IGL_LOG_ERROR("bindBytes(): %zu bytes is more than WebGPU's %zu\n",
                    length,
                    UniformArena::kMaxAllocationSize);
    } else {
      IGL_LOG_ERROR("bindBytes(): cannot allocate uniform memory for %zu bytes\n", length);
    }
    return;
  }
  binder_.bindBuffer(index, slice.buffer, slice.offset, slice.size);
}

void ComputeCommandEncoder::bindPushConstants(const void* IGL_NULLABLE data,
                                              size_t length,
                                              size_t offset) {
  const Result result = binder_.updatePushConstants(data, length, offset);
  if (!result.isOk()) {
    IGL_LOG_ERROR("bindPushConstants(): %s\n", result.message.c_str());
  }
}

bool ComputeCommandEncoder::prepareDispatch() {
  if (!pass_) {
    IGL_LOG_ERROR_ONCE("Dispatch after endEncoding()\n");
    return false;
  }
  if (!pipeline_) {
    IGL_LOG_ERROR_ONCE("Dispatch without a compute pipeline\n");
    return false;
  }
  SampleClasses classes = 0;
  Result result = binder_.getSampleClasses(*pipeline_, classes);
  WGPUComputePipeline pipeline = nullptr;
  if (result.isOk()) {
    pipeline = pipeline_->getPipeline(classes, &result);
  }
  if (pipeline == nullptr) {
    IGL_LOG_ERROR("Dispatch skipped: %s\n", result.message.c_str());
    return false;
  }
  if (pipeline != boundPipeline_) {
    wgpuComputePassEncoderSetPipeline(pass_.get(), pipeline);
    boundPipeline_ = pipeline;
  }
  binder_.stagePushConstants(commandBuffer_->getUniformArena());
  result = binder_.flush(pass_.get(), *pipeline_, classes, commandBuffer_->getSerial());
  if (!result.isOk()) {
    IGL_LOG_ERROR("Dispatch skipped: %s\n", result.message.c_str());
    return false;
  }
  return true;
}

void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& /*threadgroupSize*/,
                                                 const Dependencies& /*dependencies*/) {
  IGL_PROFILER_FUNCTION();
  if (threadgroupCount.width == 0 || threadgroupCount.height == 0 || threadgroupCount.depth == 0 ||
      !prepareDispatch()) {
    return;
  }
  wgpuComputePassEncoderDispatchWorkgroups(
      pass_.get(), threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);
}

void ComputeCommandEncoder::dispatchThreadGroupsIndirect(IBuffer& indirectBuffer,
                                                         size_t indirectBufferOffset,
                                                         const Dimensions& /*threadgroupSize*/,
                                                         const Dependencies& /*dependencies*/) {
  IGL_PROFILER_FUNCTION();
  auto& buffer = static_cast<Buffer&>(indirectBuffer);
  if ((buffer.getBufferType() & BufferDesc::BufferTypeBits::Indirect) == 0 ||
      indirectBufferOffset % 4 != 0) {
    IGL_LOG_ERROR("dispatchThreadGroupsIndirect() needs an Indirect buffer and a 4-byte offset\n");
    return;
  }
  constexpr size_t kDispatchIndirectSize = 3 * sizeof(uint32_t);
  if (indirectBufferOffset > buffer.getSizeInBytes() ||
      buffer.getSizeInBytes() - indirectBufferOffset < kDispatchIndirectSize) {
    IGL_LOG_ERROR("dispatchThreadGroupsIndirect() arguments extend past the end of the buffer\n");
    return;
  }
  if (!prepareDispatch()) {
    return;
  }
  wgpuComputePassEncoderDispatchWorkgroupsIndirect(
      pass_.get(), buffer.getWGPUBuffer(), indirectBufferOffset);
  buffer.recordUse(commandBuffer_->getSerial(), /*gpuWrite=*/false);
}

} // namespace igl::webgpu
