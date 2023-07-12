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
    std::shared_ptr<Buffer> resource;
    size_t offset;
  };

  using TextureState = std::pair<ITexture*, std::shared_ptr<ISamplerState>>;
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
  void setStencilReferenceValue(uint32_t value, Result* outResult = nullptr);
  void setStencilReferenceValues(uint32_t frontValue,
                                 uint32_t backValue,
                                 Result* outResult = nullptr);
  void setBlendColor(Color color);
  void setDepthBias(float depthBias, float slopeScale);

  void clearVertexBuffers();
  void setVertexBuffer(std::shared_ptr<Buffer> buffer,
                       size_t offset,
                       int index,
                       Result* outResult = nullptr);

  void clearUniformBuffers();
  void setUniformBuffer(const std::shared_ptr<Buffer>& buffer,
                        size_t offset,
                        int index,
                        Result* outResult = nullptr);
  void setUniform(const UniformDesc& uniformDesc, const void* data, Result* outResult = nullptr);

  void clearVertexTexture();
  void setVertexTexture(ITexture* texture, size_t index, Result* outResult = nullptr);
  void setVertexSamplerState(const std::shared_ptr<ISamplerState>& samplerState,
                             size_t index,
                             Result* outResult = nullptr);

  void clearFragmentTexture();
  void setFragmentTexture(ITexture* texture, size_t index, Result* outResult = nullptr);
  void setFragmentSamplerState(const std::shared_ptr<ISamplerState>& samplerState,
                               size_t index,
                               Result* outResult = nullptr);

  void setPipelineState(const std::shared_ptr<IRenderPipelineState>& newValue,
                        Result* outResult = nullptr);

  void drawArrays(GLenum mode, GLint first, GLsizei count);
  void drawElements(GLenum mode,
                    GLsizei indexCount,
                    GLenum indexType,
                    Buffer& indexBuffer,
                    const GLvoid* indexOffset);
  void drawElementsIndirect(GLenum mode,
                            GLenum indexType,
                            Buffer& indexBuffer,
                            Buffer& indirectBuffer,
                            const GLvoid* indirectBufferOffset);

  void endEncoding();

  void initialize(const RenderPassDesc& renderPass,
                  const std::shared_ptr<IFramebuffer>& framebuffer,
                  Result* outResult);

 private:
  RenderCommandAdapter(IContext& context);

  void clearDependentResources(const std::shared_ptr<IRenderPipelineState>& newValue,
                               Result* outResult = nullptr);
  void willDraw();
  void didDraw();
  void unbindVertexAttributes();
  void unbindResources();

  void bindBufferWithShaderStorageBufferOverride(Buffer& buffer,
                                                 GLenum overrideTargetForShaderStorageBuffer);

  static void unbindTexture(IContext& context, size_t textureUnit, TextureState& textureState);
  static void unbindTextures(IContext& context,
                             TextureStates& states,
                             std::bitset<IGL_TEXTURE_SAMPLERS_MAX>& dirtyFlags);

  bool isDirty(StateMask mask) const {
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
  GLenum toMockWireframeMode(GLenum mode) const;

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

  UnbindPolicy cachedUnbindPolicy_;
  bool useVAO_ = false;
};
} // namespace opengl
} // namespace igl
