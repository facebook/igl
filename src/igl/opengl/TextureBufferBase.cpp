/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/TextureBufferBase.h>

#include <igl/opengl/Errors.h>

namespace igl::opengl {

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
  IGL_DEBUG_ABORT("Unsupported OGL Texture Target: 0x%x", target_);
  return TextureType::Invalid;
}

TextureDesc::TextureUsage TextureBufferBase::getUsage() const {
  return usage_;
}

// bind this as a source texture for rendering from
void TextureBufferBase::bind() {
  IGL_DEBUG_ASSERT(getUsage() & TextureDesc::TextureUsageBits::Sampled);
  getContext().bindTexture(target_, textureID_);
}

void TextureBufferBase::unbind() {
  IGL_DEBUG_ASSERT(getUsage() & TextureDesc::TextureUsageBits::Sampled);
  getContext().bindTexture(target_, 0);
}

void TextureBufferBase::attachAsColor(uint32_t index, const AttachmentParams& params) {
  IGL_DEBUG_ASSERT(getUsage() & TextureDesc::TextureUsageBits::Attachment);
  if (IGL_DEBUG_VERIFY(textureID_)) {
    attach(GL_COLOR_ATTACHMENT0 + index, params, textureID_);
  }
}

void TextureBufferBase::attach(GLenum attachment,
                               const AttachmentParams& params,
                               GLuint textureID) {
  const GLenum target =
      target_ == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + params.face : target_;
  GLenum framebufferTarget = GL_FRAMEBUFFER;
  const auto& deviceFeatures = getContext().deviceFeatures();
  if (deviceFeatures.hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    framebufferTarget = params.read ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER;
  }
  const auto numSamples = getSamples();
  const auto numLayers = getNumLayers();

  if (numSamples > 1) {
    IGL_DEBUG_ASSERT(
        attachment == GL_COLOR_ATTACHMENT0 || attachment == GL_DEPTH_ATTACHMENT ||
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
      // `IMG_multisampled_render_to_texture` unlike `EXT_multisampled_render_to_texture`,
      // only supports  GL_FRAMEBUFFER, not GL_DRAW/READ_FRAMEBUFFER
      if ((framebufferTarget == GL_DRAW_FRAMEBUFFER || framebufferTarget == GL_READ_FRAMEBUFFER) &&
          !deviceFeatures.hasExtension(Extensions::MultiSampleExt) &&
          deviceFeatures.hasExtension(Extensions::MultiSampleImg)) {
        framebufferTarget = GL_FRAMEBUFFER;
      }
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
  if (IGL_DEBUG_VERIFY(textureID_)) {
    attach(GL_DEPTH_ATTACHMENT, params, textureID_);
  }
}

void TextureBufferBase::detachAsDepth(bool read) {
  AttachmentParams params{};
  params.read = read;
  attach(GL_DEPTH_ATTACHMENT, params, 0);
}

void TextureBufferBase::attachAsStencil(const AttachmentParams& params) {
  if (IGL_DEBUG_VERIFY(textureID_)) {
    attach(GL_STENCIL_ATTACHMENT, params, textureID_);
  }
}

void TextureBufferBase::detachAsStencil(bool read) {
  AttachmentParams params{};
  params.read = read;
  attach(GL_STENCIL_ATTACHMENT, params, 0);
}

void TextureBufferBase::setMaxMipLevel() const {
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::TexturePartialMipChain)) {
    getContext().texParameteri(getTarget(), GL_TEXTURE_MAX_LEVEL, (GLint)(numMipLevels_ - 1));
  }
}

void TextureBufferBase::generateMipmap(ICommandQueue& /* unused */,
                                       const TextureRangeDesc* IGL_NULLABLE /* unused */) const {
  generateMipmap();
}

void TextureBufferBase::generateMipmap(ICommandBuffer& /* unused */,
                                       const TextureRangeDesc* IGL_NULLABLE /* unused */) const {
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

bool TextureBufferBase::isValidForTexImage(const TextureRangeDesc& range) const {
  const auto dimensions = getDimensions();
  const auto levelWidth = std::max(dimensions.width >> range.mipLevel, 1u);
  const auto levelHeight = std::max(dimensions.height >> range.mipLevel, 1u);
  const auto levelDepth = std::max(dimensions.depth >> range.mipLevel, 1u);

  return (range.x == 0 && range.y == 0 && range.z == 0 && range.layer == 0 &&
          range.width == levelWidth && range.height == levelHeight && range.depth == levelDepth &&
          range.numLayers == getNumLayers());
}

} // namespace igl::opengl
