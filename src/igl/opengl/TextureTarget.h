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
  ~TextureTarget() override;

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
  void attach(GLenum attachment, const AttachmentParams& params, GLuint renderBufferId);

  /// @returns false if an unknown format is specified
  bool toRenderBufferFormatGL(TextureDesc::TextureUsage usage, GLenum& formatGL) const;

  Result createRenderBuffer(const TextureDesc& desc, bool hasStorageAlready);

  GLuint renderBufferID_ = 0;
};

} // namespace igl::opengl
