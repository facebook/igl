/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <igl/ComputeCommandEncoder.h>
#include <igl/ComputePass.h>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/ResourcesBinder.h>

namespace igl::webgpu {

class CommandBuffer;
class ComputePipelineState;

/// @brief Implements the igl::IComputeCommandEncoder interface with one WebGPU compute pass.
///
/// Bindings are recorded when set and turned into bind groups at the next dispatch. Dispatches
/// WebGPU would reject (no pipeline, bindings the pipeline cannot use) are skipped with an error
/// log. The workgroup size is fixed by the shader's `@workgroup_size`; `threadgroupSize` is
/// ignored, as on Vulkan.
class ComputeCommandEncoder final : public IComputeCommandEncoder {
 public:
  ComputeCommandEncoder(std::shared_ptr<CommandBuffer> commandBuffer,
                        const ComputePassDesc& computePass);
  ~ComputeCommandEncoder() override;

  ComputeCommandEncoder(const ComputeCommandEncoder&) = delete;
  ComputeCommandEncoder& operator=(const ComputeCommandEncoder&) = delete;
  ComputeCommandEncoder(ComputeCommandEncoder&&) = delete;
  ComputeCommandEncoder& operator=(ComputeCommandEncoder&&) = delete;

  void endEncoding() override;

  void pushDebugGroupLabel(const char* IGL_NONNULL label, const Color& color) const override;
  void insertDebugEventLabel(const char* IGL_NONNULL label, const Color& color) const override;
  void popDebugGroupLabel() const override;

  void bindComputePipelineState(
      const std::shared_ptr<IComputePipelineState>& pipelineState) override;
  void bindUniform(const UniformDesc& uniformDesc, const void* IGL_NULLABLE data) override;
  void bindTexture(uint32_t index, ITexture* IGL_NULLABLE texture) override;
  void bindImageTexture(uint32_t index,
                        ITexture* IGL_NULLABLE texture,
                        TextureFormat format) override;
  void bindSamplerState(uint32_t index, ISamplerState* IGL_NULLABLE samplerState) override;
  void bindBuffer(uint32_t index,
                  IBuffer* IGL_NULLABLE buffer,
                  size_t offset,
                  size_t bufferSize) override;
  void bindBytes(uint32_t index, const void* IGL_NULLABLE data, size_t length) override;
  void bindPushConstants(const void* IGL_NULLABLE data, size_t length, size_t offset) override;

  void dispatchThreadGroups(const Dimensions& threadgroupCount,
                            const Dimensions& threadgroupSize,
                            const Dependencies& dependencies) override;
  void dispatchThreadGroupsIndirect(IBuffer& indirectBuffer,
                                    size_t indirectBufferOffset,
                                    const Dimensions& threadgroupSize,
                                    const Dependencies& dependencies) override;

  [[nodiscard]] WGPUComputePassEncoder IGL_NULLABLE getWGPUComputePassEncoder() const noexcept {
    return pass_.get();
  }

 private:
  // Applies the recorded state before a dispatch; false if the dispatch must be skipped.
  [[nodiscard]] bool prepareDispatch();

  std::shared_ptr<CommandBuffer> commandBuffer_;
  Handle<WGPUComputePassEncoder> pass_;
  ResourcesBinder binder_;
  std::shared_ptr<ComputePipelineState> pipeline_;
  WGPUComputePipeline IGL_NULLABLE boundPipeline_ = nullptr;
};

} // namespace igl::webgpu
