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

// TextureBufferBase encapsulates OpenGL textures
class TextureBufferBase : public Texture {
  using Super = Texture;

 public:
  TextureBufferBase(IContext& context, TextureFormat format) : Super(context, format) {}

  // ITexture overrides
  TextureType getType() const override;
  ulong_t getUsage() const override;

  void bind() override;
  void bindImage(size_t unit) override {}
  void unbind() override;
  void attachAsColor(uint32_t index, uint32_t face = 0, uint32_t mipmapLevel = 0) override;
  void detachAsColor(uint32_t index, uint32_t face = 0, uint32_t mipmapLevel = 0) override;
  void attachAsDepth() override;
  void attachAsStencil() override;
  size_t getNumMipLevels() const override;
  void generateMipmap(ICommandQueue& cmdQueue) const override;
  bool isRequiredGenerateMipmap() const override;

  GLuint getId() const override {
    return textureID_;
  }

  GLuint getTarget() const {
    return target_;
  }

 protected:
  IGL_INLINE void setTextureBufferProperties(GLuint textureID, GLenum target) {
    textureID_ = textureID;
    target_ = target;
  }

  IGL_INLINE void setUsage(TextureDesc::TextureUsage usage) {
    usage_ = usage;
  }

  void attachAsColor(uint32_t index, uint32_t face, uint32_t mipmapLevel, GLuint textureID);
  void setMaxMipLevel() const;

 private:
  // the GL ID for this texture
  GLuint textureID_ = 0;
  // target depends on usage and texture type
  GLenum target_ = 0;
  TextureDesc::TextureUsage usage_ = 0;
};

} // namespace opengl
} // namespace igl
