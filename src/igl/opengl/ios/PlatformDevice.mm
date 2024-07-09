/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/ios/PlatformDevice.h>

#include <CoreVideo/CoreVideo.h>

#import <Foundation/Foundation.h>

#include <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/glext.h>

#import <QuartzCore/QuartzCore.h>

#include <cstdio>
#include <cstring>
#include <igl/Common.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/TextureTarget.h>
#include <igl/opengl/VertexInputState.h>
#include <igl/opengl/ios/Context.h>
#include <igl/opengl/ios/Device.h>
#include <igl/opengl/ios/TextureBuffer.h>
#import <objc/runtime.h>

static void* kAssociatedRenderBufferHolderKey = &kAssociatedRenderBufferHolderKey;

/// Object used to hold onto a _renderBuffer so we can attach it as an associated object
@interface _IGLRenderBufferHolder : NSObject {
 @public
  std::weak_ptr<igl::opengl::TextureTarget> _renderBuffer;
}
@end

namespace {
/// Backed by an associated object. This is used to track the last _renderBuffer used to create this
/// texture so we can reuse it and invalidate it when necessary
/// This always returns a renderBufferHolder, but it is up to the responsibility of the caller to
/// set _renderBuffer.
// @fb-only
// @fb-only
_IGLRenderBufferHolder* GetAssociatedRenderBufferHolder(CAEAGLLayer* nativeDrawable);

} // namespace

namespace igl::opengl::ios {

PlatformDevice::PlatformDevice(Device& owner) : opengl::PlatformDevice(owner) {}

PlatformDevice::~PlatformDevice() {
  getSharedContext()->setCurrent();
}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(
    CAEAGLLayer* nativeDrawable,
    Result* outResult) {
  if (!nativeDrawable) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "Invalid native drawable");
    return nullptr;
  }

  Result::setOk(outResult);

  const CGFloat scale = nativeDrawable.contentsScale;

  const CGRect bounds = [nativeDrawable bounds];
  const CGRect resolution = CGRectMake(
      bounds.origin.x, bounds.origin.y, bounds.size.width * scale, bounds.size.height * scale);

  _IGLRenderBufferHolder* renderBufferHolder = GetAssociatedRenderBufferHolder(nativeDrawable);

  const auto renderBuffer = renderBufferHolder->_renderBuffer.lock();

  if (renderBuffer != nullptr && renderBuffer->getSize().width == resolution.size.width &&
      renderBuffer->getSize().height == resolution.size.height) {
    // The caller is expected to release the reference
    return renderBuffer;
  } else {
    // If a size change causes the texture to be recreated, the caller must ensure the previous
    // ITexture is freed before calling, or else there may be unexpected behavior.
    if (renderBuffer != nullptr) {
      renderBuffer->bind();
      [[EAGLContext currentContext] renderbufferStorage:GL_RENDERBUFFER fromDrawable:nullptr];
      renderBuffer->unbind();
    }

    NSString* colorFormat = kEAGLColorFormatRGBA8;
    /*
        if (!nativeDrawable.drawableProperties) {
          colorFormat = kEAGLColorFormatRGBA8;
          eaglLayer.drawableProperties = @{kEAGLDrawablePropertyColorFormat: colorFormat};
        } else {
          colorFormat = [eaglLayer.drawableProperties
       objectForKey:kEAGLDrawablePropertyColorFormat];
        }
    */
    TextureDesc desc;
    desc.type = TextureType::TwoD;
    if (colorFormat == kEAGLColorFormatRGBA8) {
      desc.format = TextureFormat::RGBA_UNorm8;
      //  } else if (colorFormat == kEAGLColorFormatRGB565) {
      //    desc.format = TextureFormatRGB565UNorm; // TODO: we need to bring back RGB565 first
      //  } else if (colorFormat == kEAGLColorFormatSRGBA8) {
      //    desc.format = TextureFormatSRGBA8888UNorm; // TODO: we need to add support for sRGB
      //    formats
    } else {
      Result::setResult(outResult, Result::Code::Unsupported, "Unsupported texture format");
      return nullptr;
    }

    desc.width = (size_t)resolution.size.width;
    desc.height = (size_t)resolution.size.height;
    desc.depth = 1;
    desc.numSamples = 1;
    desc.usage = TextureDesc::TextureUsageBits::Attachment;

    auto texture = std::make_shared<TextureTarget>(getContext(), desc.format);
    if (texture != nullptr) {
      const Result result = texture->create(desc, true);

      texture->bind();
      [[EAGLContext currentContext] renderbufferStorage:GL_RENDERBUFFER
                                           fromDrawable:nativeDrawable];
      texture->unbind();

      if (!result.isOk()) {
        if (outResult) {
          *outResult = result;
        }

        return nullptr;
      }
    }

    renderBufferHolder->_renderBuffer = texture;
    if (auto resourceTracker = owner_.getResourceTracker()) {
      texture->initResourceTracker(resourceTracker);
    }

    return texture;
  }
}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDepth(
    CAEAGLLayer* nativeDrawable,
    TextureFormat depthTextureFormat,
    Result* outResult) {
  if (!nativeDrawable) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "Invalid native drawable");
    return nullptr;
  }

  Result::setOk(outResult);

  const CGFloat scale = nativeDrawable.contentsScale;

  const CGRect bounds = [nativeDrawable bounds];
  const CGRect resolution = CGRectMake(
      bounds.origin.x, bounds.origin.y, bounds.size.width * scale, bounds.size.height * scale);

  TextureDesc desc = {
      static_cast<size_t>(resolution.size.width),
      static_cast<size_t>(resolution.size.height),
      1,
      1,
      1,
      TextureDesc::TextureUsageBits::Attachment,
      1,
      TextureType::TwoD,
      depthTextureFormat,
  };
  desc.storage = ResourceStorage::Private;
  return owner_.createTexture(desc, outResult);
}

Size PlatformDevice::getNativeDrawableSize(CAEAGLLayer* nativeDrawable, Result* outResult) {
  if (nativeDrawable == nullptr) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "nativeDrawable cannot be null");
    return {0, 0};
  }
  Result::setOk(outResult);
  const auto screenScale = nativeDrawable.contentsScale;
  const auto bounds = nativeDrawable.bounds;
  const auto resolution = CGRectMake(bounds.origin.x,
                                     bounds.origin.y,
                                     bounds.size.width * screenScale,
                                     bounds.size.height * screenScale);
  return {(float)resolution.size.width, (float)resolution.size.height};
}

TextureFormat PlatformDevice::getNativeDrawableTextureFormat(CAEAGLLayer* /*nativeDrawable*/,
                                                             Result* outResult) {
  Result::setOk(outResult);
  //[nativeDrawable.drawableProperties objectForKey:kEAGLDrawablePropertyColorFormat];
  return TextureFormat::RGBA_UNorm8; // TODO convert value retrieved with above method to IGL!
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativePixelBufferWithSize(
    const CVImageBufferRef& sourceImage,
    const CVOpenGLESTextureCacheRef& textureCache,
    size_t width,
    size_t height,
    size_t planeIndex,
    TextureDesc::TextureUsage usage,
    Result* outResult) {
  auto textureBuffer =
      std::make_unique<TextureBuffer>(getContext(), sourceImage, textureCache, planeIndex, usage);
  const Result result = textureBuffer->createWithSize(width, height);

  Result::setResult(outResult, result.code, result.message);
  if (!result.isOk()) {
    return nullptr;
  }
  if (auto resourceTracker = owner_.getResourceTracker()) {
    textureBuffer->initResourceTracker(resourceTracker);
  }
  return textureBuffer;
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativePixelBuffer(
    const CVImageBufferRef& sourceImage,
    const CVOpenGLESTextureCacheRef& textureCache,
    size_t planeIndex,
    TextureDesc::TextureUsage usage,
    Result* outResult) {
  auto textureBuffer =
      std::make_unique<TextureBuffer>(getContext(), sourceImage, textureCache, planeIndex, usage);
  const Result result = textureBuffer->create();

  Result::setResult(outResult, result.code, result.message);
  if (!result.isOk()) {
    return nullptr;
  }
  if (auto resourceTracker = owner_.getResourceTracker()) {
    textureBuffer->initResourceTracker(resourceTracker);
  }
  return textureBuffer;
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativePixelBufferWithSize(
    const CVImageBufferRef& sourceImage,
    size_t width,
    size_t height,
    size_t planeIndex,
    TextureDesc::TextureUsage usage,
    Result* outResult) {
  auto textureBuffer = std::make_unique<TextureBuffer>(
      getContext(), sourceImage, getTextureCache(), planeIndex, usage);
  const Result result = textureBuffer->createWithSize(width, height);

  Result::setResult(outResult, result.code, result.message);
  if (!result.isOk()) {
    return nullptr;
  }
  if (auto resourceTracker = owner_.getResourceTracker()) {
    textureBuffer->initResourceTracker(resourceTracker);
  }
  return textureBuffer;
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativePixelBuffer(
    const CVImageBufferRef& sourceImage,
    size_t planeIndex,
    TextureDesc::TextureUsage usage,
    Result* outResult) {
  auto textureBuffer = std::make_unique<TextureBuffer>(
      getContext(), sourceImage, getTextureCache(), planeIndex, usage);
  const Result result = textureBuffer->create();

  Result::setResult(outResult, result.code, result.message);
  if (!result.isOk()) {
    return nullptr;
  }
  if (auto resourceTracker = owner_.getResourceTracker()) {
    textureBuffer->initResourceTracker(resourceTracker);
  }
  return textureBuffer;
}

bool PlatformDevice::isType(PlatformDeviceType t) const noexcept {
  return t == Type || opengl::PlatformDevice::isType(t);
}

CVOpenGLESTextureCacheRef PlatformDevice::getTextureCache() {
  auto* context = static_cast<Context*>(getSharedContext().get());
  return context->getTextureCache();
}

} // namespace igl::opengl::ios

namespace {
_IGLRenderBufferHolder* GetAssociatedRenderBufferHolder(CAEAGLLayer* nativeDrawable) {
  _IGLRenderBufferHolder* renderBufferHolder =
      objc_getAssociatedObject(nativeDrawable, kAssociatedRenderBufferHolderKey);
  if (renderBufferHolder) {
    return renderBufferHolder;
  }
  renderBufferHolder = [_IGLRenderBufferHolder new];
  objc_setAssociatedObject(nativeDrawable,
                           kAssociatedRenderBufferHolderKey,
                           renderBufferHolder,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  return renderBufferHolder;
}
} // namespace

@implementation _IGLRenderBufferHolder
@end
