/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/renderSession/ScreenshotTestRenderSessionHelper.h>

#include <shell/shared/imageWriter/ImageWriter.h>
#include <igl/Config.h>

namespace igl::shell {

void saveFrameBufferToPng(const char* absoluteFilename,
                          const std::shared_ptr<IFramebuffer>& framebuffer,
                          Platform& platform) {
  auto drawableSurface = framebuffer->getColorAttachment(0);
  auto frameBufferSize = drawableSurface->getDimensions();
  const int bytesPerPixel = 4;
  const auto rangeDesc =
      TextureRangeDesc::new2D(0, 0, frameBufferSize.width, frameBufferSize.height);
  ImageData imageData;
  imageData.desc.format = drawableSurface->getFormat();
  imageData.desc.width = frameBufferSize.width;
  imageData.desc.height = frameBufferSize.height;
  auto buffer =
      std::make_unique<uint8_t[]>(frameBufferSize.width * frameBufferSize.height * bytesPerPixel);

  const CommandQueueDesc desc{};
  auto commandQueue = platform.getDevice().createCommandQueue(desc, nullptr);
  framebuffer->copyBytesColorAttachment(*commandQueue, 0, buffer.get(), rangeDesc);

  const size_t numPixels = frameBufferSize.width * frameBufferSize.height * bytesPerPixel;

#if IGL_PLATFORM_WINDOWS
  if (imageData.desc.format == TextureFormat::BGRA_UNorm8) {
    // Swap B and R channels, as image writer expects RGBA.
    // Note that this is only defined for the Windows platform, as in practice
    // BGRA might only be used there for render targets.
    for (size_t i = 0; i < numPixels; i += bytesPerPixel) {
      std::swap(buffer.get()[i], buffer.get()[i + 2]);
    }
  }
#endif

  imageData.data = iglu::textureloader::IData::tryCreate(std::move(buffer), numPixels, nullptr);

  IGLLog(IGLLogInfo, "Writing screenshot to: '%s'\n", absoluteFilename);
  platform.getImageWriter().writeImage(absoluteFilename, imageData);
}

} // namespace igl::shell
