/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <igl/DepthStencilState.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/ResourcesBinder.h>

namespace igl::webgpu {

class Buffer;
class CommandBuffer;
class Device;
class RenderPipelineState;
class Texture;

/// @brief Implements the igl::IRenderCommandEncoder interface with one WebGPU render pass.
///
/// Bindings, depth/stencil state, culling, winding and depth bias are recorded when set and
/// applied at the next draw, which picks the pipeline variant and bind groups they need. Draws
/// WebGPU would reject are skipped with an error log (missing pipeline or vertex buffers, a
/// texture sampled and attached in the same pass, bindings the pipeline cannot use).
class RenderCommandEncoder final : public IRenderCommandEncoder {
 public:
  [[nodiscard]] static std::unique_ptr<RenderCommandEncoder> create(
      const std::shared_ptr<CommandBuffer>& commandBuffer,
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      Result* IGL_NULLABLE outResult);
  ~RenderCommandEncoder() override;

  RenderCommandEncoder(const RenderCommandEncoder&) = delete;
  RenderCommandEncoder& operator=(const RenderCommandEncoder&) = delete;
  RenderCommandEncoder(RenderCommandEncoder&&) = delete;
  RenderCommandEncoder& operator=(RenderCommandEncoder&&) = delete;

  void endEncoding() override;

  void pushDebugGroupLabel(const char* IGL_NONNULL label, const Color& color) const override;
  void insertDebugEventLabel(const char* IGL_NONNULL label, const Color& color) const override;
  void popDebugGroupLabel() const override;

  void bindViewport(const Viewport& viewport) override;
  void bindScissorRect(const ScissorRect& rect) override;

  void bindRenderPipelineState(const std::shared_ptr<IRenderPipelineState>& pipelineState) override;
  void bindDepthStencilState(const std::shared_ptr<IDepthStencilState>& depthStencilState) override;

  void bindBuffer(uint32_t index,
                  uint8_t bindTarget,
                  IBuffer* IGL_NULLABLE buffer,
                  size_t bufferOffset,
                  size_t bufferSize) override;
  void bindBuffer(uint32_t index,
                  IBuffer* IGL_NULLABLE buffer,
                  size_t bufferOffset,
                  size_t bufferSize) override;
  void bindVertexBuffer(uint32_t index,
                        IBuffer& buffer,
                        size_t bufferOffset,
                        size_t attributeStride) override;
  void bindIndexBuffer(IBuffer& buffer, IndexFormat format, size_t bufferOffset) override;
  void bindBytes(size_t index, uint8_t bindTarget, const void* data, size_t length) override;
  void bindPushConstants(const void* data, size_t length, size_t offset) override;
  void bindSamplerState(size_t index,
                        uint8_t target,
                        ISamplerState* IGL_NULLABLE samplerState) override;
  void bindTexture(size_t index, uint8_t target, ITexture* IGL_NULLABLE texture) override;
  void bindTexture(size_t index, ITexture* IGL_NULLABLE texture) override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;

  void bindBindGroup(BindGroupTextureHandle handle) override;
  void bindBindGroup(BindGroupBufferHandle handle,
                     uint32_t numDynamicOffsets,
                     const uint32_t* IGL_NULLABLE dynamicOffsets) override;

  void draw(size_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t baseInstance) override;
  void drawIndexed(size_t indexCount,
                   uint32_t instanceCount,
                   uint32_t firstIndex,
                   int32_t vertexOffset,
                   uint32_t baseInstance) override;
  void drawMeshTasks(const Dimensions& threadgroupsPerGrid,
                     const Dimensions& threadsPerTaskThreadgroup,
                     const Dimensions& threadsPerMeshThreadgroup) override;
  void multiDrawIndirect(IBuffer& indirectBuffer,
                         size_t indirectBufferOffset,
                         uint32_t drawCount,
                         uint32_t stride) override;
  void multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                                size_t indirectBufferOffset,
                                uint32_t drawCount,
                                uint32_t stride) override;

  void setStencilReferenceValue(uint32_t value) override;
  void setBlendColor(const Color& color) override;
  void setCullMode(CullMode cullMode) override;
  void setDepthBias(float depthBias, float slopeScale, float clamp) override;
  void setFrontFacingWinding(WindingMode frontFaceWinding) override;

  [[nodiscard]] WGPURenderPassEncoder IGL_NULLABLE getWGPURenderPassEncoder() const noexcept {
    return pass_.get();
  }

 private:
  // A subresource attached to the pass.
  struct Attachment {
    WGPUTexture IGL_NULLABLE texture = nullptr;
    uint32_t mipLevel = 0;
    uint32_t layer = 0;
  };

  RenderCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer, Device& device);

  [[nodiscard]] Result begin(const RenderPassDesc& renderPass, const IFramebuffer& framebuffer);
  // Applies the recorded state before a draw; false if the draw must be skipped.
  [[nodiscard]] bool prepareDraw(bool indexed);
  void drawIndirect(IBuffer& indirectBuffer,
                    size_t indirectBufferOffset,
                    uint32_t drawCount,
                    uint32_t stride,
                    bool indexed);
  [[nodiscard]] Result checkSampledAttachments() const;

  CommandBuffer& commandBuffer_;
  Device& device_;
  Handle<WGPURenderPassEncoder> pass_;
  ResourcesBinder binder_;
  std::vector<Attachment> attachments_;
  uint32_t targetWidth_ = 0;
  uint32_t targetHeight_ = 0;

  std::shared_ptr<RenderPipelineState> pipeline_;
  WGPURenderPipeline IGL_NULLABLE boundPipeline_ = nullptr;
  DepthStencilStateDesc depthStencilState_;
  std::optional<CullMode> cullMode_;
  std::optional<WindingMode> frontFaceWinding_;
  float depthBias_ = 0.0f;
  float depthBiasSlopeScale_ = 0.0f;
  float depthBiasClamp_ = 0.0f;
  std::optional<IndexFormat> indexFormat_;
  uint32_t boundVertexBuffers_ = 0;
  std::array<Texture*, kMaxTextureUnits> textures_ = {};
};

} // namespace igl::webgpu
