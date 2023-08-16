/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Texture.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>

namespace igl {
class ICommandBuffer;
namespace opengl {

// Texture is the base class for the OpenGL backend. It represents:
// 1. traditional textures (sampled/output by shaders)
// 2. render targets (attachments to framebuffers)
class Texture : public WithContext, public ITexture {
 public:
  Texture(IContext& context, TextureFormat format) : WithContext(context), ITexture(format) {}
  ~Texture() override = default;

 public:
  Result upload(const TextureRangeDesc& range, const void* data, size_t bytesPerRow) const override;
  Result uploadCube(const TextureRangeDesc& range,
                    TextureCubeFace face,
                    const void* data,
                    size_t bytesPerRow) const override;

  // Accessors
  Dimensions getDimensions() const override;
  size_t getNumLayers() const override;
  size_t getSamples() const override;
  void generateMipmap(ICommandQueue& cmdQueue) const override;
  size_t getNumMipLevels() const override;
  bool isRequiredGenerateMipmap() const override;
  uint64_t getTextureId() const override;

  virtual Result create(const TextureDesc& desc, bool hasStorageAlready);

  // bind this as a source texture for rendering from
  virtual void bind() = 0;
  virtual void bindImage(size_t unit) = 0;
  virtual void unbind() = 0;

  // frame buffer attachments
  virtual void attachAsColor(uint32_t index,
                             uint32_t face = 0,
                             uint32_t mipLevel = 0,
                             bool read = false) = 0;
  virtual void detachAsColor(uint32_t index,
                             uint32_t face = 0,
                             uint32_t mipLevel = 0,
                             bool read = false) = 0;
  virtual void attachAsDepth() = 0;
  virtual void attachAsStencil() = 0;

  virtual bool isImplicitStorage() const;

  [[nodiscard]] GLenum toGLTarget(TextureType type, size_t samples = 1) const;
  static TextureFormat glInternalFormatToTextureFormat(GLuint glTexInternalFormat,
                                                       GLuint glTexFormat,
                                                       GLuint glTexType);

  // @fb-only
  // @fb-only
  virtual GLuint getId() const = 0;

  GLint getAlignment(size_t stride, size_t mipLevel = 0) const;

  void setSamplerHash(size_t newValue) {
    samplerHash_ = newValue;
  }

  size_t getSamplerHash() const {
    return samplerHash_;
  }

  GLenum getGLInternalTextureFormat() const {
    IGL_ASSERT(glInternalFormat_ != 0);
    return glInternalFormat_;
  }

  // glTexImageXXX functions require 3 different parameters to specify a texture format
  struct FormatDescGL final {
    GLint internalFormat = 0;
    GLenum format = GL_NONE;
    GLenum type = GL_NONE;
  };

  /// Converts the GL format descriptor from the given texture format
  /// @returns false if an unknown format is specified
  bool toFormatDescGL(TextureFormat textureFormat,
                      TextureDesc::TextureUsage usage,
                      FormatDescGL& outFormatGL) const;

  static bool toFormatDescGL(IContext& ctx,
                             TextureFormat textureFormat,
                             TextureDesc::TextureUsage usage,
                             FormatDescGL& outFormatGL);

 protected:
  IGL_INLINE GLsizei getWidth() const {
    return width_;
  }
  IGL_INLINE GLsizei getHeight() const {
    return height_;
  }
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  IGL_INLINE void setTextureProperties(GLsizei width, GLsizei height, GLsizei numLayers = 1) {
    width_ = width;
    height_ = height;
    numLayers_ = numLayers;
  }

  /// @returns true if the format is usable as a TextureTarget format.
  /// @remark Does not take into account whether a particular context supports it or not.
  bool isTextureTargetFormat(TextureFormat textureFormat) const;

  GLenum glInternalFormat_;
  size_t numMipLevels_ = 1;
  TextureType type_ = TextureType::Invalid;

 private:
  size_t samplerHash_ = std::numeric_limits<size_t>::max();
  GLsizei width_ = 0;
  GLsizei height_ = 0;
  GLsizei depth_ = 1;
  GLsizei numLayers_ = 1;
  size_t numSamples_ = 1;
  bool isCreated_ = false;
};

} // namespace opengl
} // namespace igl
