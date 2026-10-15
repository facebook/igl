/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/Texture.h>

namespace igl::opengl {

// TextureTarget encapsulates OpenGL renderbuffers
class TextureTarget final : public Texture {
  using Super = Texture;

 public:
  TextureTarget(IContext& context, TextureFormat format) : Super(context, format) {}
  TextureTarget(IContext& context, TextureFormat format, bool canPresent) :
    Super(context, format), canPresent_(canPresent) {}
  ~TextureTarget() override;
  TextureTarget(const TextureTarget&) = delete;
  TextureTarget& operator=(const TextureTarget&) = delete;
  TextureTarget(TextureTarget&&) = delete;
  TextureTarget& operator=(TextureTarget&&) = delete;

  // ITexture overrides
  [[nodiscard]] TextureType getType() const override;
  [[nodiscard]] TextureDesc::TextureUsage getUsage() const override;

  // Texture overrides
  Result create(const TextureDesc& desc, bool hasStorageAlready) override;

  void bind() override;
  void unbind() override;
  void bindImage(size_t unit) override;
  void attachAsColor(uint32_t index, const AttachmentParams& params) override;
  void detachAsColor(uint32_t index, bool read) override;
  void attachAsDepth(const AttachmentParams& params) override;
  void detachAsDepth(bool read) override;
  void attachAsStencil(const AttachmentParams& params) override;
  void detachAsStencil(bool read) override;

  // @fb-only
  [[nodiscard]] GLuint getId() const override {
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return 0;
  }

 private:
  [[nodiscard]] bool canPresent() const noexcept override;

  void attach(GLenum attachment, const AttachmentParams& params, GLuint renderBufferId);

  /// @returns false if an unknown format is specified
  bool toRenderBufferFormatGL(TextureDesc::TextureUsage usage, GLenum& formatGL) const;

  Result createRenderBuffer(const TextureDesc& desc, bool hasStorageAlready);

  /// Specifies the storage of the bound renderbuffer
  void specifyStorage() const;

  /// @returns the renderbuffer to attach with `params`, after it gets the storage that it needs
  [[nodiscard]] GLuint prepareRenderBuffer(const AttachmentParams& params);

  /// Gives `renderBufferId` the debug name of the texture
  void labelRenderBuffer(GLuint renderBufferId) const;

  void createImplicitRenderBuffer(uint32_t samples);
  void releaseImplicitRenderBuffer();

  GLuint renderBufferID_ = 0;
  // The renderbuffer that AttachmentParams::implicitSamples attaches. Its storage needs
  // renderbufferStorageMultisampleImplicit(), so it is created when a framebuffer needs it, and
  // released when a framebuffer detaches it or attaches renderBufferID_.
  GLuint implicitRenderBufferID_ = 0;
  // Whether renderBufferID_ still needs its storage. A Memoryless renderbuffer gets it when a
  // framebuffer attaches it, because a framebuffer that resolves implicitly never attaches the
  // multisampled one.
  bool needsStorage_ = false;
  std::string debugName_;
  TextureDesc::TextureUsage usage_ = TextureDesc::TextureUsageBits::Attachment;
#if IGL_PLATFORM_IOS
  bool canPresent_ = false;
#else
  bool canPresent_ = true;
#endif
};

} // namespace igl::opengl
