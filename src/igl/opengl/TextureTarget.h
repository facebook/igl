/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/Texture.h>

namespace igl {
namespace opengl {

// TextureTarget encapsulates OpenGL renderbuffers
class TextureTarget final : public Texture {
  using Super = Texture;

 public:
  TextureTarget(IContext& context, TextureFormat format) : Super(context, format) {}
  ~TextureTarget() override;

  // ITexture overrides
  TextureType getType() const override;
  ulong_t getUsage() const override;

  // Texture overrides
  Result create(const TextureDesc& desc, bool hasStorageAlready) override;

  void bind() override;
  void unbind() override;
  void bindImage(size_t unit) override;
  void attachAsColor(uint32_t index, uint32_t face = 0, uint32_t mipLevel = 0) override;
  void detachAsColor(uint32_t index, uint32_t face = 0, uint32_t mipLevel = 0) override;
  void attachAsDepth() override;
  void attachAsStencil() override;

  // @fb-only
  GLuint getId() const override {
    IGL_ASSERT_NOT_REACHED();
    return 0;
  }

 private:
  /// @returns false if an unknown format is specified
  bool toRenderBufferFormatGL(TextureDesc::TextureUsage usage, GLenum& formatGL) const;

  Result createRenderBuffer(const TextureDesc& desc, bool hasStorageAlready);

  GLuint renderBufferID_ = 0;
};

} // namespace opengl
} // namespace igl
