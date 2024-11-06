/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <bitset>
#include <functional>
#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/UnbindPolicy.h>
#include <igl/opengl/UniformAdapter.h>
#include <igl/opengl/WithContext.h>

namespace igl {
class IFramebuffer;
class ITexture;
class IDepthStencilState;
class ISamplerState;
struct RenderPassDesc;
class IRenderPipelineState;

namespace opengl {
class Buffer;
class VertexArrayObject;

class RenderCommandAdapter final : public WithContext {
 public:
  using StateBits = uint32_t;
  enum class StateMask : StateBits { NONE = 0, PIPELINE = 1 << 1, DEPTH_STENCIL = 1 << 2 };

 private:
  struct BufferState {
    Buffer* resource = nullptr;
    size_t offset = 0;
  };

  using TextureState = std::pair<ITexture*, ISamplerState*>;
  using TextureStates = std::array<TextureState, IGL_TEXTURE_SAMPLERS_MAX>;

 public:
  static std::unique_ptr<RenderCommandAdapter> create(
      IContext& context,
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      Result* outResult);

  void setViewport(const Viewport& viewport);

  void setScissorRect(const ScissorRect& rect);

  void setDepthStencilState(const std::shared_ptr<IDepthStencilState>& newValue);
  void setStencilReferenceValue(uint32_t value);
  void setBlendColor(Color color);
  void setDepthBias(float depthBias, float slopeScale, float clamp);

  void clearVertexBuffers();
  void setVertexBuffer(Buffer& buffer, size_t offset, size_t index, Result* outResult = nullptr);
  void setIndexBuffer(Buffer& buffer);

  void clearUniformBuffers();
  void setUniformBuffer(Buffer* buffer,
                        size_t offset,
                        size_t size,
                        uint32_t index,
                        Result* outResult = nullptr);
  void setUniform(const UniformDesc& uniformDesc, const void* data, Result* outResult = nullptr);

  void clearVertexTexture();
  void setVertexTexture(ITexture* texture, size_t index, Result* outResult = nullptr);
  void setVertexSamplerState(ISamplerState* samplerState,
                             size_t index,
                             Result* outResult = nullptr);

  void clearFragmentTexture();
  void setFragmentTexture(ITexture* texture, size_t index, Result* outResult = nullptr);
  void setFragmentSamplerState(ISamplerState* samplerState,
                               size_t index,
                               Result* outResult = nullptr);

  void setPipelineState(const std::shared_ptr<IRenderPipelineState>& newValue,
                        Result* outResult = nullptr);

  void drawArrays(GLenum mode, GLint first, GLsizei count);
  void drawArraysIndirect(GLenum mode, Buffer& indirectBuffer, const GLvoid* indirectBufferOffset);
  void drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
  void drawElements(GLenum mode, GLsizei indexCount, GLenum indexType, const GLvoid* indexOffset);
  void drawElementsInstanced(GLenum mode,
                             GLsizei indexCount,
                             GLenum indexType,
                             const GLvoid* indexOffset,
                             GLsizei instancecount);
  void drawElementsIndirect(GLenum mode,
                            GLenum indexType,
                            Buffer& indirectBuffer,
                            const GLvoid* indirectBufferOffset);

  void endEncoding();

  void initialize(const RenderPassDesc& renderPass,
                  const std::shared_ptr<IFramebuffer>& framebuffer,
                  Result* outResult);

  [[nodiscard]] const igl::IRenderPipelineState& pipelineState() const {
    IGL_DEBUG_ASSERT(pipelineState_, "No rendering pipeline is bound");
    return *pipelineState_;
  }

 private:
  explicit RenderCommandAdapter(IContext& context);

  void clearDependentResources(const std::shared_ptr<IRenderPipelineState>& newValue,
                               Result* outResult = nullptr);
  void willDraw();
  void didDraw();
  void unbindVertexAttributes();

  void bindBufferWithShaderStorageBufferOverride(Buffer& buffer,
                                                 GLenum overrideTargetForShaderStorageBuffer);

  static void unbindTexture(IContext& context, size_t textureUnit, TextureState& textureState);
  static void unbindTextures(IContext& context,
                             TextureStates& states,
                             std::bitset<IGL_TEXTURE_SAMPLERS_MAX>& dirtyFlags);

  [[nodiscard]] bool isDirty(StateMask mask) const {
    return (dirtyStateBits_ & EnumToValue(mask)) != 0;
  }
  void setDirty(StateMask mask) {
    dirtyStateBits_ |= EnumToValue(mask);
  }
  void clearDirty(StateMask mask) {
    dirtyStateBits_ &= ~EnumToValue(mask);
  }

  // @brief OpenGL ES doesn't support glPolygonMode. To support rendering wireframe with it
  // we change all triangle drawing modes to GL_LINE_STRIP
  [[nodiscard]] GLenum toMockWireframeMode(GLenum mode) const;

 private:
  std::array<BufferState, IGL_VERTEX_BUFFER_MAX> vertexBuffers_;
  std::bitset<IGL_VERTEX_BUFFER_MAX> vertexBuffersDirty_;
  std::bitset<IGL_TEXTURE_SAMPLERS_MAX> vertexTextureStatesDirty_;
  std::bitset<IGL_TEXTURE_SAMPLERS_MAX> fragmentTextureStatesDirty_;
  TextureStates vertexTextureStates_;
  TextureStates fragmentTextureStates_;
  UniformAdapter uniformAdapter_;
  StateBits dirtyStateBits_ = EnumToValue(StateMask::NONE);
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IDepthStencilState> depthStencilState_;
  std::shared_ptr<VertexArrayObject> activeVAO_ = nullptr;
  uint32_t frontStencilReferenceValue_ = 0xFF;
  uint32_t backStencilReferenceValue_ = 0xFF;

  UnbindPolicy cachedUnbindPolicy_;
  bool useVAO_ = false;
};
} // namespace opengl
} // namespace igl
