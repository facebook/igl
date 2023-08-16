/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/TextureBufferBase.h>

#include <igl/opengl/Errors.h>

namespace igl {
namespace opengl {

TextureType TextureBufferBase::getType() const {
  // TODO: Handle compressed texture type
  switch (target_) {
  case GL_TEXTURE_CUBE_MAP:
    return TextureType::Cube;
  case GL_TEXTURE_2D:
  case GL_TEXTURE_2D_MULTISAMPLE:
    return TextureType::TwoD;
  case GL_TEXTURE_2D_ARRAY:
  case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::Texture2DArray)) {
      return TextureType::TwoDArray;
    }
    break;
  case GL_TEXTURE_3D:
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::Texture3D)) {
      return TextureType::ThreeD;
    }
    break;
  case GL_TEXTURE_EXTERNAL_OES:
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureExternalImage)) {
      return TextureType::ExternalImage;
    }
    break;
  }
  IGL_ASSERT_MSG(0, "Unsupported OGL Texture Target: 0x%x", target_);
  return TextureType::Invalid;
}

ulong_t TextureBufferBase::getUsage() const {
  return usage_;
}

// bind this as a source texture for rendering from
void TextureBufferBase::bind() {
  IGL_ASSERT(getUsage() & TextureDesc::TextureUsageBits::Sampled);
  getContext().bindTexture(target_, textureID_);
}

void TextureBufferBase::unbind() {
  IGL_ASSERT(getUsage() & TextureDesc::TextureUsageBits::Sampled);
  getContext().bindTexture(target_, 0);
}

void TextureBufferBase::attachAsColor(uint32_t index, uint32_t face, uint32_t mipLevel, bool read) {
  IGL_ASSERT(getUsage() & TextureDesc::TextureUsageBits::Attachment);
  if (IGL_VERIFY(textureID_)) {
    attachAsColor(index, face, mipLevel, read, textureID_);
  }
}

void TextureBufferBase::attachAsColor(uint32_t index,
                                      uint32_t face,
                                      uint32_t mipLevel,
                                      bool read,
                                      GLuint textureID) {
  GLenum target = target_ == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : target_;
  GLenum framebufferTarget = GL_FRAMEBUFFER;
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    framebufferTarget = read ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER;
  }
  if (getSamples() > 1) {
    IGL_ASSERT_MSG(index == 0, "Multisample framebuffer can only use GL_COLOR_ATTACHMENT0");
    getContext().framebufferTexture2DMultisample(
        framebufferTarget, GL_COLOR_ATTACHMENT0 + index, target, textureID, mipLevel, getSamples());
  } else {
    getContext().framebufferTexture2D(
        framebufferTarget, GL_COLOR_ATTACHMENT0 + index, target, textureID, mipLevel);
  }
}

void TextureBufferBase::detachAsColor(uint32_t index, uint32_t face, uint32_t mipLevel, bool read) {
  attachAsColor(index, face, mipLevel, read, 0);
}

void TextureBufferBase::attachAsDepth() {
  if (IGL_VERIFY(textureID_)) {
    getContext().framebufferTexture2D(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textureID_, 0);
  }
}

void TextureBufferBase::attachAsStencil() {
  if (IGL_VERIFY(textureID_)) {
    getContext().framebufferTexture2D(
        GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, textureID_, 0);
  }
}

size_t TextureBufferBase::getNumMipLevels() const {
  return numMipLevels_;
}

void TextureBufferBase::setMaxMipLevel() const {
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::TexturePartialMipChain)) {
    getContext().texParameteri(getTarget(), GL_TEXTURE_MAX_LEVEL, (GLint)(numMipLevels_ - 1));
  }
}

void TextureBufferBase::generateMipmap(ICommandQueue& /* unused */) const {
  getContext().bindTexture(getTarget(), getId());
  setMaxMipLevel();
  getContext().generateMipmap(getTarget());
}

bool TextureBufferBase::isRequiredGenerateMipmap() const {
  return numMipLevels_ > 1;
}

} // namespace opengl
} // namespace igl
