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

void TextureBufferBase::attachAsColor(uint32_t index, const AttachmentParams& params) {
  IGL_ASSERT(getUsage() & TextureDesc::TextureUsageBits::Attachment);
  if (IGL_VERIFY(textureID_)) {
    attach(GL_COLOR_ATTACHMENT0 + index, params, textureID_);
  }
}

void TextureBufferBase::attach(GLenum attachment,
                               const AttachmentParams& params,
                               GLuint textureID) {
  GLenum target = target_ == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + params.face
                                                 : target_;
  GLenum framebufferTarget = GL_FRAMEBUFFER;
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    framebufferTarget = params.read ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER;
  }
  const auto numSamples = getSamples();
  const auto numLayers = getNumLayers();

  if (numSamples > 1) {
    IGL_ASSERT_MSG(attachment == GL_COLOR_ATTACHMENT0 || attachment == GL_DEPTH_ATTACHMENT ||
                       attachment == GL_STENCIL_ATTACHMENT,
                   "Multisample framebuffer can only use GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT "
                   "or GL_STENCIL_ATTACHMENT");
    if (params.stereo) {
      getContext().framebufferTextureMultisampleMultiview(framebufferTarget,
                                                          attachment,
                                                          textureID,
                                                          params.mipLevel,
                                                          static_cast<GLsizei>(numSamples),
                                                          0,
                                                          2);
    } else {
      getContext().framebufferTexture2DMultisample(
          framebufferTarget, attachment, target, textureID, params.mipLevel, getSamples());
    }
  } else {
    if (params.stereo) {
      getContext().framebufferTextureMultiview(
          framebufferTarget, attachment, textureID, params.mipLevel, 0, 2);
    } else if (numLayers > 1) {
      getContext().framebufferTextureLayer(
          framebufferTarget, attachment, textureID, params.mipLevel, params.layer);
    } else {
      getContext().framebufferTexture2D(
          framebufferTarget, attachment, target, textureID, params.mipLevel);
    }
  }
}

void TextureBufferBase::detachAsColor(uint32_t index, bool read) {
  AttachmentParams params{};
  params.read = read;
  attach(GL_COLOR_ATTACHMENT0 + index, params, 0);
}

void TextureBufferBase::attachAsDepth(const AttachmentParams& params) {
  if (IGL_VERIFY(textureID_)) {
    attach(GL_DEPTH_ATTACHMENT, params, textureID_);
  }
}

void TextureBufferBase::attachAsStencil(const AttachmentParams& params) {
  if (IGL_VERIFY(textureID_)) {
    attach(GL_STENCIL_ATTACHMENT, params, textureID_);
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
  generateMipmap();
}

void TextureBufferBase::generateMipmap(ICommandBuffer& /* unused */) const {
  generateMipmap();
}

void TextureBufferBase::generateMipmap() const {
  getContext().bindTexture(getTarget(), getId());
  setMaxMipLevel();
  getContext().generateMipmap(getTarget());
}

bool TextureBufferBase::isRequiredGenerateMipmap() const {
  return numMipLevels_ > 1;
}

} // namespace opengl
} // namespace igl
