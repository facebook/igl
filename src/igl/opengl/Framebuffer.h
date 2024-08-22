/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Framebuffer.h>
#include <igl/RenderPass.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/Texture.h>

namespace igl {
class ICommandBuffer;
namespace opengl {

///--------------------------------------
/// MARK: - FramebufferBindingGuard

class FramebufferBindingGuard {
 public:
  explicit FramebufferBindingGuard(IContext& context);
  ~FramebufferBindingGuard();

 private:
  IContext& context_;
  GLuint currentRenderbuffer_ = 0;
  GLuint currentFramebuffer_ = 0;
  GLuint currentReadFramebuffer_ = 0;
  GLuint currentDrawFramebuffer_ = 0;
};

// Framebuffer encapsulates an immutable render target (attachments) and per-render pass state.
class Framebuffer : public WithContext, public IFramebuffer {
 public:
  explicit Framebuffer(IContext& context);
  virtual Viewport getViewport() const = 0;
  virtual void bind(const RenderPassDesc& renderPass) const = 0;
  virtual void unbind() const = 0;

  void bindBuffer() const;
  void bindBufferForRead() const;
  void copyBytesColorAttachment(ICommandQueue& /* unused */,
                                size_t index,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const override;

  void copyBytesDepthAttachment(ICommandQueue& /* unused */,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const override;

  void copyBytesStencilAttachment(ICommandQueue& /* unused */,
                                  void* pixelBytes,
                                  const TextureRangeDesc& range,
                                  size_t bytesPerRow = 0) const override;

  void copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                  size_t index,
                                  std::shared_ptr<ITexture> destTexture,
                                  const TextureRangeDesc& range) const override;
  inline GLuint getId() const {
    return frameBufferID_;
  }

  inline std::shared_ptr<Framebuffer> getResolveFramebuffer() const {
    return resolveFramebuffer;
  }

  [[nodiscard]] bool isSwapchainBound() const override;

 protected:
  void attachAsColor(igl::ITexture& texture,
                     uint32_t index,
                     const Texture::AttachmentParams& params) const;
  void attachAsDepth(igl::ITexture& texture, const Texture::AttachmentParams& params) const;
  void attachAsStencil(igl::ITexture& texture, const Texture::AttachmentParams& params) const;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  GLuint frameBufferID_ = 0;

  struct CachedState {
    FramebufferMode mode_ = igl::FramebufferMode::Mono;
    uint8_t layer_ = 0;
    uint8_t face_ = 0;
    uint8_t mipLevel_ = 0;

    bool needsUpdate(FramebufferMode mode, uint8_t layer, uint8_t face, uint8_t mipLevel);
    void updateCache(FramebufferMode mode, uint8_t layer, uint8_t face, uint8_t mipLevel);
  };

  constexpr static auto kNumCachedStates = 8; // We allow up to 8 color attachments
  mutable std::array<CachedState, kNumCachedStates> colorCachedState_;
  mutable CachedState depthCachedState_;
  mutable CachedState stencilCachedState_;

  std::shared_ptr<Framebuffer> resolveFramebuffer = nullptr;
};

// CustomFramebuffer enables caller-defined attachments
//
// There are several kinds of framebuffer color attachments that are encapsulated
// by the opengl::Texture interface. For the OpenGL backend, we have different
// implementations of that interface whose usage supports TextureUsageBits::Attachment:
//
// 1. `opengl::TextureBuffer`. These are regular old textures, e.g. loaded from an
// image file. They can be read from a shader (or written to by a compute shader)
// 2. `opengl::TextureTarget`. These are renderbuffers. For storage, normally IGL
// allocates storage via glRenderbufferStorage. On iOS, the view's storage is used
// instead — see ios::PlatformDevice::createTextureFromNativeDrawable() where
// glRenderBufferStorage is replaced by [EAGLContext renderbufferStorage:fromDrawable:].
// 3. `opengl::macos::ViewTextureTarget`. This represents the color attachment of
// the implicit framebuffer supplied by NSOpenGLView. Here, IGL skips prepareResource()
// since GPU resources are owned by the client, i.e. `hasImplicitColorAttachment()` == true.
class CustomFramebuffer final : public Framebuffer {
 public:
  using Framebuffer::Framebuffer;
  ~CustomFramebuffer() override;

  // Accessors
  std::vector<size_t> getColorAttachmentIndices() const override;
  std::shared_ptr<ITexture> getColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getDepthAttachment() const override;
  std::shared_ptr<ITexture> getResolveDepthAttachment() const override;
  std::shared_ptr<ITexture> getStencilAttachment() const override;
  [[nodiscard]] FramebufferMode getMode() const override;

  // Methods
  void updateDrawable(std::shared_ptr<ITexture> texture) override;
  void updateDrawable(SurfaceTextures surfaceTextures) override;
  void updateResolveAttachment(std::shared_ptr<ITexture> texture) override;

  bool isInitialized() const;
  bool hasImplicitColorAttachment() const;
  void initialize(const FramebufferDesc& desc, Result* outResult);

  // CommandBuffer
  Viewport getViewport() const override;
  void bind(const RenderPassDesc& renderPass) const override;
  void unbind() const override;

 private:
  void prepareResource(const std::string& debugName, Result* outResult);
  void updateDrawableInternal(SurfaceTextures surfaceTextures, bool updateDepthStencil);

  bool initialized_ = false;

  friend class Framebuffer; // Needed to enable copyBytesColorAttachment
  FramebufferDesc renderTarget_; // attachments
  mutable RenderPassDesc renderPass_;
};

class CurrentFramebuffer final : public Framebuffer {
 public:
  using Super = Framebuffer;
  explicit CurrentFramebuffer(IContext& context);

  // IFramebuffer
  std::vector<size_t> getColorAttachmentIndices() const override;
  std::shared_ptr<ITexture> getColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getDepthAttachment() const override;
  std::shared_ptr<ITexture> getResolveDepthAttachment() const override;
  std::shared_ptr<ITexture> getStencilAttachment() const override;
  void updateDrawable(std::shared_ptr<ITexture> texture) override;
  void updateDrawable(SurfaceTextures surfaceTextures) override;
  void updateResolveAttachment(std::shared_ptr<ITexture> texture) override;
  [[nodiscard]] FramebufferMode getMode() const override;

  // opengl::Framebuffer
  Viewport getViewport() const override;
  void bind(const RenderPassDesc& renderPass) const override;
  void unbind() const override;

 private:
  Viewport viewport_;
  std::shared_ptr<ITexture> colorAttachment_;
};

} // namespace opengl
} // namespace igl
