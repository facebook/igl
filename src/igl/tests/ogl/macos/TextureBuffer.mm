/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../util/Common.h"
#include "../../util/TextureFormatTestBase.h"
#include <CoreVideo/CVPixelBuffer.h>
#include <igl/opengl/macos/Context.h>
#include <igl/opengl/macos/PlatformDevice.h>

namespace igl::tests {

class TextureBufferMacTest : public util::TextureFormatTestBase {
 public:
  TextureBufferMacTest() = default;
  ~TextureBufferMacTest() override = default;

  std::shared_ptr<igl::ITexture> createCVPixelBufferTextureWithSize(OSType pixelFormat,
                                                                    size_t width,
                                                                    size_t height,
                                                                    TextureDesc::TextureUsage usage,
                                                                    Result& outResult);
};

std::shared_ptr<igl::ITexture> TextureBufferMacTest::createCVPixelBufferTextureWithSize(
    OSType pixelFormat,
    const size_t width,
    const size_t height,
    TextureDesc::TextureUsage usage,
    Result& outResult) {
  const igl::BackendType backend = iglDev_->getBackendType();
  CVPixelBufferRef pixelBuffer = nullptr;
  NSDictionary* bufferAttributes = @{
    (NSString*)kCVPixelBufferIOSurfacePropertiesKey : @{},
    (NSString*)kCVPixelBufferOpenGLCompatibilityKey : @(backend == igl::BackendType::OpenGL),
  };

  const CVReturn result = CVPixelBufferCreate(kCFAllocatorDefault,
                                              width,
                                              height,
                                              pixelFormat,
                                              (__bridge CFDictionaryRef)(bufferAttributes),
                                              &pixelBuffer);
  if (result != kCVReturnSuccess) {
    Result::setResult(&outResult,
                      Result::Code::RuntimeError,
                      "CVPixelBufferCreate failed to create pixel buffer");
    return nullptr;
  }

  auto* platformDevice = iglDev_->getPlatformDevice<igl::opengl::macos::PlatformDevice>();
  auto& context = static_cast<igl::opengl::macos::Context&>(platformDevice->getContext());
  auto* textureCache = context.createTextureCache();
  std::shared_ptr<igl::ITexture> texture = platformDevice->createTextureFromNativePixelBuffer(
      pixelBuffer, textureCache, usage, &outResult);
  if (!outResult.isOk()) {
    return nullptr;
  }
  if (texture == nullptr) {
    Result::setResult(
        &outResult, Result::Code::RuntimeError, "failed to create igl texture from CVPixelBuffer");
    return nullptr;
  }
  CVPixelBufferRelease(pixelBuffer);
  CVOpenGLTextureCacheRelease(textureCache);
  Result::setOk(&outResult);
  return texture;
}

#define PIXEL_FORMAT(pf) \
  { pf, #pf }

TEST_F(TextureBufferMacTest, createTextureFromNativePixelBuffer) {
  const std::vector<std::pair<OSType, const char*>> pixelFormats = {
      PIXEL_FORMAT(kCVPixelFormatType_32BGRA), PIXEL_FORMAT(kCVPixelFormatType_OneComponent8),
      // TODO: Figure out how to test YUV textures
      // PIXEL_FORMAT(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange),
      // PIXEL_FORMAT(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
  };
  for (auto [pixelFormat, name] : pixelFormats) {
    Result result;
    auto usage = TextureDesc::TextureUsageBits::Sampled | TextureDesc::TextureUsageBits::Attachment;
    auto texture = createCVPixelBufferTextureWithSize(pixelFormat, 100, 100, usage, result);
    ASSERT_EQ(result.isOk(), true) << name << ": " << result.message.c_str();
    testUsage(texture, usage, "SampledAttachment");
  }
}

} // namespace igl::tests
