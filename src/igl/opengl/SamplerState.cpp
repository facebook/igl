/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/SamplerState.h>

#include <igl/CommandBuffer.h>
#include <igl/Common.h>
#include <igl/Texture.h>
#include <igl/opengl/DepthStencilState.h>
#include <igl/opengl/Texture.h>

namespace igl::opengl {
namespace {
bool isPowerOfTwo(size_t number) {
  return (number & (number - 1)) == 0;
}
} // namespace

SamplerState::SamplerState(IContext& context, const SamplerStateDesc& desc) :
  WithContext(context),
  minMipFilter_(convertMinMipFilter(desc.minFilter, desc.mipFilter)),
  magFilter_(convertMagFilter(desc.magFilter)),
  mipLodMin_(desc.mipLodMin),
  mipLodMax_(desc.mipLodMax),
  maxAnisotropy_(desc.maxAnisotropic),
  addressU_(convertAddressMode(desc.addressModeU)),
  addressV_(convertAddressMode(desc.addressModeV)),
  addressW_(convertAddressMode(desc.addressModeW)),
  depthCompareFunction_(DepthStencilState::convertCompareFunction(desc.depthCompareFunction)),
  depthCompareEnabled_(desc.depthCompareEnabled),
  isYUV_(desc.yuvFormat != igl::TextureFormat::Invalid) {
  const std::hash<SamplerStateDesc> h;
  hash_ = h(desc);
}

void SamplerState::bind(ITexture* t) {
  if (IGL_UNEXPECTED(t == nullptr)) {
    return;
  }

  auto* texture = static_cast<igl::opengl::Texture*>(t);
  if (texture->getSamplerHash() == hash_) {
    return;
  }
  texture->setSamplerHash(hash_);

  auto type = texture->getType();
  auto target = texture->toGLTarget(type);
  if (target == 0) {
    return;
  }

  const auto& deviceFeatures = getContext().deviceFeatures();
  const bool isDepthOrDepthStencil = texture->getProperties().isDepthOrDepthStencil();
  // From OpenGL ES 3.1 spec
  // The effective internal format specified for the texture arrays is a sized internal depth or
  // depth and stencil format (see table 8.14), the value of TEXTURE_COMPARE_MODE is NONE , and
  // either the magnification filter is not NEAREST or the minification filter is neither
  // NEAREST nor NEAREST_MIPMAP_NEAREST.
  if (!depthCompareEnabled_ && isDepthOrDepthStencil && minMipFilter_ != GL_NEAREST &&
      minMipFilter_ != GL_NEAREST_MIPMAP_NEAREST) {
    IGL_LOG_INFO_ONCE(
        "OpenGL requires a GL_NEAREST or NEAREST_MIPMAP_NEAREST min filter for depth/stencil "
        "samplers when DepthCompareEnabled is false, falling back to supported mode instead of "
        "requested format.");
    auto supportedMode = GL_NEAREST;
    if (minMipFilter_ == GL_LINEAR_MIPMAP_NEAREST || minMipFilter_ == GL_NEAREST_MIPMAP_LINEAR ||
        minMipFilter_ == GL_LINEAR_MIPMAP_LINEAR) {
      supportedMode = GL_NEAREST_MIPMAP_NEAREST;
    }
    getContext().texParameteri(target, GL_TEXTURE_MIN_FILTER, supportedMode);
  } else {
    getContext().texParameteri(target, GL_TEXTURE_MIN_FILTER, minMipFilter_);
  }
  if (!depthCompareEnabled_ && isDepthOrDepthStencil && magFilter_ != GL_NEAREST) {
    IGL_LOG_INFO_ONCE(
        "OpenGL requires a GL_NEAREST mag filter for depth/stencil samplers when "
        "DepthCompareEnabled is false, falling back to GL_NEAREST instead of requested format.");
    getContext().texParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  } else {
    getContext().texParameteri(target, GL_TEXTURE_MAG_FILTER, magFilter_);
  }

  // See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glTexParameter.xml
  // for OpenGL version information.
  // Ensure we have mipmaps before setting this state. This should also catch special
  // texture types that may not support mipmaps, like ExternalOES textures on Android
  if (texture->getNumMipLevels() > 1 &&
      deviceFeatures.hasFeature(DeviceFeatures::SamplerMinMaxLod)) {
    getContext().texParameteri(target, GL_TEXTURE_MIN_LOD, mipLodMin_);
    getContext().texParameteri(target, GL_TEXTURE_MAX_LOD, mipLodMax_);
  }
  if (deviceFeatures.hasFeature(DeviceFeatures::TextureFilterAnisotropic)) {
    // @fb-only
    // Disable the anisotropic filter for now, it's causing a crash on some devices
#if 0
    getContext().texParameteri(target, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy_);
#else
    (void)maxAnisotropy_;
#endif
  }

  if (isDepthOrDepthStencil &&
      deviceFeatures.hasInternalFeature(InternalFeatures::TextureCompare)) {
    getContext().texParameteri(target,
                               GL_TEXTURE_COMPARE_MODE,
                               depthCompareEnabled_ ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
    getContext().texParameteri(target, GL_TEXTURE_COMPARE_FUNC, depthCompareFunction_);
  }

  if (!deviceFeatures.hasFeature(DeviceFeatures::TextureNotPot)) {
    const auto dimensions = texture->getDimensions();
    if (!isPowerOfTwo(dimensions.width) || !isPowerOfTwo(dimensions.height)) {
      getContext().texParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      getContext().texParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
      getContext().texParameteri(target, GL_TEXTURE_WRAP_S, addressU_);
      getContext().texParameteri(target, GL_TEXTURE_WRAP_T, addressV_);
    }
  } else {
    getContext().texParameteri(target, GL_TEXTURE_WRAP_S, addressU_);
    getContext().texParameteri(target, GL_TEXTURE_WRAP_T, addressV_);
  }

  if (type == TextureType::TwoDArray || type == TextureType::ThreeD) {
    getContext().texParameteri(target, GL_TEXTURE_WRAP_R, addressW_);
  }
}

// utility functions for converting from IGL sampler state enums to GL enums
GLint SamplerState::convertMinMipFilter(SamplerMinMagFilter minFilter, SamplerMipFilter mipFilter) {
  GLint glMinFilter;

  switch (mipFilter) {
  case SamplerMipFilter::Disabled:
    glMinFilter = (minFilter == SamplerMinMagFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
    break;

  case SamplerMipFilter::Nearest:
    glMinFilter = (minFilter == SamplerMinMagFilter::Nearest) ? GL_NEAREST_MIPMAP_NEAREST
                                                              : GL_LINEAR_MIPMAP_NEAREST;
    break;

  case SamplerMipFilter::Linear:
    glMinFilter = (minFilter == SamplerMinMagFilter::Nearest) ? GL_NEAREST_MIPMAP_LINEAR
                                                              : GL_LINEAR_MIPMAP_LINEAR;
    break;
  }

  return glMinFilter;
}

GLint SamplerState::convertMagFilter(SamplerMinMagFilter magFilter) {
  return (magFilter == SamplerMinMagFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
}

SamplerMinMagFilter SamplerState::convertGLMagFilter(GLint glMagFilter) {
  return (glMagFilter == GL_NEAREST) ? SamplerMinMagFilter::Nearest : SamplerMinMagFilter::Linear;
}

SamplerMinMagFilter SamplerState::convertGLMinFilter(GLint glMinFilter) {
  SamplerMinMagFilter minFilter;

  switch (glMinFilter) {
  case GL_NEAREST:
  case GL_NEAREST_MIPMAP_NEAREST:
  case GL_NEAREST_MIPMAP_LINEAR:
    minFilter = SamplerMinMagFilter::Nearest;
    break;

  case GL_LINEAR:
  case GL_LINEAR_MIPMAP_NEAREST:
  case GL_LINEAR_MIPMAP_LINEAR:
    minFilter = SamplerMinMagFilter::Linear;
    break;

  default:
    IGL_ASSERT_NOT_REACHED();
    minFilter = SamplerMinMagFilter::Nearest;
  }

  return minFilter;
}

SamplerMipFilter SamplerState::convertGLMipFilter(GLint glMinFilter) {
  SamplerMipFilter mipFilter;

  switch (glMinFilter) {
  case GL_NEAREST:
  case GL_LINEAR:
    mipFilter = SamplerMipFilter::Disabled;
    break;

  case GL_NEAREST_MIPMAP_NEAREST:
  case GL_LINEAR_MIPMAP_NEAREST:
    mipFilter = SamplerMipFilter::Nearest;
    break;

  case GL_NEAREST_MIPMAP_LINEAR:
  case GL_LINEAR_MIPMAP_LINEAR:
    mipFilter = SamplerMipFilter::Linear;
    break;

  default:
    IGL_ASSERT_NOT_REACHED();
    mipFilter = SamplerMipFilter::Disabled;
  }

  return mipFilter;
}

GLint SamplerState::convertAddressMode(SamplerAddressMode addressMode) {
  GLint glAddressMode;

  switch (addressMode) {
  case SamplerAddressMode::Repeat:
    glAddressMode = GL_REPEAT;
    break;

  case SamplerAddressMode::Clamp:
    glAddressMode = GL_CLAMP_TO_EDGE;
    break;

  case SamplerAddressMode::MirrorRepeat:
    glAddressMode = GL_MIRRORED_REPEAT;
    break;
  }

  return glAddressMode;
}

SamplerAddressMode SamplerState::convertGLAddressMode(GLint glAddressMode) {
  SamplerAddressMode addressMode;

  switch (glAddressMode) {
  case GL_REPEAT:
    addressMode = SamplerAddressMode::Repeat;
    break;

  case GL_CLAMP_TO_EDGE:
    addressMode = SamplerAddressMode::Clamp;
    break;

  case GL_MIRRORED_REPEAT:
    addressMode = SamplerAddressMode::MirrorRepeat;
    break;

  default:
    addressMode = SamplerAddressMode::Repeat;
    break;
  }

  return addressMode;
}

bool SamplerState::isYUV() const noexcept {
  return isYUV_;
}

} // namespace igl::opengl
