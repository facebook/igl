/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>
#include <cstring>
#include <igl/Common.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/Texture.h>
#include <igl/opengl/ViewTextureTarget.h>
#include <igl/opengl/macos/Context.h>
#include <igl/opengl/macos/Device.h>
#include <igl/opengl/macos/PlatformDevice.h>
#include <igl/opengl/macos/TextureBuffer.h>

namespace igl::opengl::macos {

///--------------------------------------
/// MARK: - PlatformDevice
PlatformDevice::PlatformDevice(Device& owner) : opengl::PlatformDevice(owner) {}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(Result* outResult) {
  Size requiredSize;
  NSView* view = [[NSOpenGLContext currentContext] view];
  if (IGL_VERIFY(view)) {
    const NSRect bounds = view.bounds;
    const NSSize sizeInPixels = [view convertSizeToBacking:bounds.size];
    requiredSize =
        Size(static_cast<float>(sizeInPixels.width), static_cast<float>(sizeInPixels.height));
  } else {
    Result::setResult(outResult, Result::Code::RuntimeError);
    return nullptr;
  }

  if (!drawableTexture_ || drawableTexture_->getSize() != requiredSize) {
    const TextureDesc desc = {
        static_cast<uint32_t>(requiredSize.width),
        static_cast<uint32_t>(requiredSize.height),
        1,
        1,
        1,
        TextureDesc::TextureUsageBits::Attachment,
        1,
        TextureType::TwoD,
        TextureFormat::RGBA_SRGB,
    };
    auto texture = std::make_shared<ViewTextureTarget>(getContext(), desc.format);
    texture->create(desc, true);
    drawableTexture_ = texture;
    if (auto resourceTracker = owner_.getResourceTracker()) {
      drawableTexture_->initResourceTracker(resourceTracker);
    }
  }

  Result::setOk(outResult);
  return drawableTexture_;
}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDepth(Result* outResult) {
  NSSize sizeInPixels;
  NSView* view = [[NSOpenGLContext currentContext] view];
  if (IGL_VERIFY(view)) {
    const NSRect bounds = view.bounds;
    sizeInPixels = [view convertSizeToBacking:bounds.size];
  } else {
    Result::setResult(outResult, Result::Code::RuntimeError);
    return nullptr;
  }

  GLint depthBits;
  NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLContext currentContext] pixelFormat];
  if (IGL_VERIFY(pixelFormat)) {
    [pixelFormat getValues:&depthBits forAttribute:NSOpenGLPFADepthSize forVirtualScreen:0];
  } else {
    Result::setResult(outResult, Result::Code::RuntimeError);
    return nullptr;
  }

  if (depthBits == 0) {
    Result::setOk(outResult);
    return nullptr;
  }

  TextureFormat textureFormat;
  switch (depthBits) {
  case 16:
    textureFormat = TextureFormat::Z_UNorm16;
    break;
  case 24:
    textureFormat = TextureFormat::Z_UNorm24;
    break;
  case 32:
    textureFormat = TextureFormat::Z_UNorm32;
    break;
  default:
    Result::setResult(outResult, Result::Code::RuntimeError);
    return nullptr;
  }

  const TextureDesc desc = {
      static_cast<uint32_t>(sizeInPixels.width),
      static_cast<uint32_t>(sizeInPixels.height),
      1,
      1,
      1,
      TextureDesc::TextureUsageBits::Attachment,
      1,
      TextureType::TwoD,
      textureFormat,
  };
  auto depthTexture = std::make_shared<ViewTextureTarget>(getContext(), desc.format);
  depthTexture->create(desc, true);
  if (auto resourceTracker = owner_.getResourceTracker()) {
    depthTexture->initResourceTracker(resourceTracker);
  }

  Result::setOk(outResult);
  return depthTexture;
}

Size PlatformDevice::getNativeDrawableSize(Result* outResult) {
  Result::setOk(outResult);

  if (drawableTexture_ == nullptr) {
    return {0, 0};
  }

  return drawableTexture_->getSize();
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativePixelBuffer(
    const CVImageBufferRef& sourceImage,
    const CVOpenGLTextureCacheRef& textureCache,
    TextureDesc::TextureUsage usage,
    Result* outResult) {
  auto textureBuffer =
      std::make_unique<TextureBuffer>(getContext(), sourceImage, textureCache, usage);
  const auto result = textureBuffer->create();
  if (auto resourceTracker = owner_.getResourceTracker()) {
    textureBuffer->initResourceTracker(resourceTracker);
  }
  Result::setResult(outResult, result.code, result.message);
  return textureBuffer;
}

TextureFormat PlatformDevice::getNativeDrawableTextureFormat(Result* outResult) {
  Result::setOk(outResult);

  return TextureFormat::RGBA_UNorm8;
}

bool PlatformDevice::isType(PlatformDeviceType t) const noexcept {
  return t == Type || opengl::PlatformDevice::isType(t);
}

} // namespace igl::opengl::macos
