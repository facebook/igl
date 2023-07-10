/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/PlatformDevice.h>

#import <QuartzCore/CAMetalLayer.h>

#import <Foundation/Foundation.h>

#import <QuartzCore/QuartzCore.h>

#include <igl/metal/DepthStencilState.h>
#include <igl/metal/Device.h>
#include <igl/metal/Framebuffer.h>
#include <igl/metal/SamplerState.h>
#include <igl/metal/Texture.h>

namespace igl {
namespace metal {

PlatformDevice::PlatformDevice(Device& device) : device_(device), textureCache_(nullptr) {}

PlatformDevice::~PlatformDevice() {
  if (textureCache_) {
    CFRelease(textureCache_);
    textureCache_ = nullptr;
  }
}

std::shared_ptr<SamplerState> PlatformDevice::createSamplerState(const SamplerStateDesc& desc,
                                                                 Result* outResult) const {
  MTLSamplerDescriptor* metalDesc = [MTLSamplerDescriptor new];
  metalDesc.minFilter = SamplerState::convertMinMagFilter(desc.minFilter);
  metalDesc.magFilter = SamplerState::convertMinMagFilter(desc.magFilter);
  metalDesc.mipFilter = SamplerState::convertMipFilter(desc.mipFilter);
  metalDesc.lodMinClamp = desc.mipLodMin;
  metalDesc.lodMaxClamp = desc.mipLodMax;
  metalDesc.sAddressMode = SamplerState::convertAddressMode(desc.addressModeU);
  metalDesc.tAddressMode = SamplerState::convertAddressMode(desc.addressModeV);
  metalDesc.rAddressMode = SamplerState::convertAddressMode(desc.addressModeW);
  metalDesc.maxAnisotropy = desc.maxAnisotropic;
  if (desc.depthCompareEnabled && device_.hasFeature(DeviceFeatures::DepthCompare)) {
    metalDesc.compareFunction =
        DepthStencilState::convertCompareFunction(desc.depthCompareFunction);
  }

  id<MTLSamplerState> metalObject = [device_.get() newSamplerStateWithDescriptor:metalDesc];
  auto resource = std::make_shared<SamplerState>(metalObject);
  if (device_.getResourceTracker()) {
    resource->initResourceTracker(device_.getResourceTracker());
  }
  Result::setOk(outResult);
  return resource;
}

std::shared_ptr<Framebuffer> PlatformDevice::createFramebuffer(const FramebufferDesc& desc,
                                                               Result* outResult) const {
  return std::static_pointer_cast<Framebuffer>(device_.createFramebuffer(desc, outResult));
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(
    id<CAMetalDrawable> nativeDrawable,
    Result* outResult) {
  auto iglObject = std::make_unique<Texture>(nativeDrawable);
  if (auto resourceTracker = device_.getResourceTracker()) {
    iglObject->initResourceTracker(resourceTracker);
  }
  Result::setOk(outResult);
  return iglObject;
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(CALayer* nativeDrawable,
                                                                          Result* outResult) {
  if (!nativeDrawable) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "Invalid native drawable");
    return nullptr;
  }
#if (!TARGET_OS_SIMULATOR || __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
  if ([nativeDrawable isKindOfClass:[CAMetalLayer class]]) {
    id<CAMetalDrawable> drawableObject = [(CAMetalLayer*)nativeDrawable nextDrawable];
    if (!drawableObject) {
      Result::setResult(outResult, Result::Code::RuntimeError, "Could not retrieve a drawable.");
      return nullptr;
    }

    return createTextureFromNativeDrawable(drawableObject, outResult);
  } else {
    // Layer is not CAMetalLayer
    // This should never hit, unless there's a new layer type that supports Metal
    IGL_ASSERT_NOT_IMPLEMENTED();
    Result::setResult(outResult, Result::Code::Unsupported);
    return nullptr;
  }
#else
  Result::setResult(outResult, Result::Code::Unsupported);
  return nullptr;
#endif
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativeDepth(
    id<MTLTexture> depthStencilTexture,
    Result* outResult) {
  auto iglObject = std::make_unique<Texture>(depthStencilTexture);
  if (auto resourceTracker = device_.getResourceTracker()) {
    iglObject->initResourceTracker(resourceTracker);
  }
  Result::setOk(outResult);
  return iglObject;
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativePixelBuffer(
    CVImageBufferRef sourceImage,
    TextureFormat format,
    size_t planeIndex,
    Result* outResult) {
  const bool isPlanar = CVPixelBufferIsPlanar(sourceImage);
  const size_t width = (isPlanar ? CVPixelBufferGetWidthOfPlane(sourceImage, planeIndex)
                                 : CVPixelBufferGetWidth(sourceImage));
  const size_t height = (isPlanar ? CVPixelBufferGetHeightOfPlane(sourceImage, planeIndex)
                                  : CVPixelBufferGetHeight(sourceImage));
  return PlatformDevice::createTextureFromNativePixelBufferWithSize(
      sourceImage, format, width, height, planeIndex, outResult);
}

std::unique_ptr<ITexture> PlatformDevice::createTextureFromNativePixelBufferWithSize(
    CVImageBufferRef sourceImage,
    TextureFormat format,
    size_t width,
    size_t height,
    size_t planeIndex,
    Result* outResult) {
  std::unique_ptr<Texture> resultTexture = nullptr;

#if (!TARGET_OS_SIMULATOR || __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
  CVMetalTextureCacheRef textureCache = getTextureCache();
  if (textureCache) {
    // Use the user provided texture instead
    MTLPixelFormat const metalFormat = Texture::textureFormatToMTLPixelFormat(format);
    if (metalFormat == MTLPixelFormatInvalid) {
      Result::setResult(outResult,
                        Result::Code::Unsupported,
                        "Invalid Texture Format : " +
                            std::string(TextureFormatProperties::fromTextureFormat(format).name));
      IGL_ASSERT_MSG(0, outResult->message.c_str());
      return nullptr;
    }

    CVMetalTextureRef cvMetalTexture = nullptr;
    CVReturn result = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                textureCache,
                                                                sourceImage,
                                                                nil,
                                                                metalFormat,
                                                                width,
                                                                height,
                                                                planeIndex,
                                                                &cvMetalTexture);
    IGL_ASSERT_MSG(result == kCVReturnSuccess, "Failed to created Metal texture from PixelBuffer");

    if (result != kCVReturnSuccess) {
      NSLog(@"Failed to created Metal texture from PixelBuffer");
      return nullptr;
    }

    id<MTLTexture> metalTexture = CVMetalTextureGetTexture(cvMetalTexture);
    CVBufferRelease(cvMetalTexture);
    cvMetalTexture = nullptr;

    resultTexture = std::make_unique<Texture>(metalTexture);
    if (auto resourceTracker = device_.getResourceTracker()) {
      resultTexture->initResourceTracker(resourceTracker);
    }
  }
#endif

  Result::setOk(outResult);
  return resultTexture;
}

Size PlatformDevice::getNativeDrawableSize(CALayer* nativeDrawable, Result* outResult) {
#if (!TARGET_OS_SIMULATOR || __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
  Result::setOk(outResult);
  return Size((float)CGRectGetWidth(nativeDrawable.bounds),
              (float)CGRectGetHeight(nativeDrawable.bounds));
#else
  Result::setResult(outResult, Result::Code::Unsupported, "Metal not supported on iOS simulator.");
  return {};
#endif
}

TextureFormat PlatformDevice::getNativeDrawableTextureFormat(CALayer* nativeDrawable,
                                                             Result* outResult) {
  TextureFormat formatResult = TextureFormat::Invalid;

#if (!TARGET_OS_SIMULATOR || __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
  if ([nativeDrawable isKindOfClass:[CAMetalLayer class]]) {
    auto metalLayer = (CAMetalLayer*)nativeDrawable;
    formatResult = Texture::mtlPixelFormatToTextureFormat(metalLayer.pixelFormat);
    Result::setOk(outResult);
  } else {
    // Layer is not CAMetalLayer
    // This should never hit, unless there's a new layer type that supports Metal
    IGL_ASSERT_NOT_IMPLEMENTED();
    Result::setResult(outResult, Result::Code::Unsupported);
  }
#else
  Result::setResult(outResult, Result::Code::Unsupported, "Metal not supported on iOS simulator.");
#endif

  return formatResult;
}

CVMetalTextureCacheRef PlatformDevice::getTextureCache() {
#if (!TARGET_OS_SIMULATOR || __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
  if (textureCache_ == nullptr && device_.get() != nullptr) {
    CVReturn result =
        CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, device_.get(), nil, &textureCache_);
    IGL_ASSERT_MSG(result == kCVReturnSuccess, "Failed to created texture cache");

    if (result != kCVReturnSuccess) {
      NSLog(@"Failed to created texture cache");
      CFRelease(textureCache_);
      textureCache_ = nullptr;
    }
  }
#endif
  return textureCache_;
}

void PlatformDevice::flushNativeTextureCache() {
#if (!TARGET_OS_SIMULATOR || __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
  if (textureCache_) {
    CVMetalTextureCacheFlush(textureCache_, 0);
  }
#endif
}

} // namespace metal
} // namespace igl
