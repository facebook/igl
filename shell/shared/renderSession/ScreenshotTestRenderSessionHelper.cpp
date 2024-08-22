/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Macros.h>

#include <shell/shared/imageWriter/ImageWriter.h>
#include <shell/shared/renderSession/ScreenshotTestRenderSessionHelper.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

void SaveFrameBufferToPng(const char* absoluteFilename,
                          const std::shared_ptr<IFramebuffer>& framebuffer,
                          Platform& platform) {
  auto drawableSurface = framebuffer->getColorAttachment(0);
  auto frameBuffersize = drawableSurface->getDimensions();
  const int bytesPerPixel = 4;
  const auto rangeDesc =
      TextureRangeDesc::new2D(0, 0, frameBuffersize.width, frameBuffersize.height);
  igl::shell::ImageData imageData;
  imageData.desc.format = drawableSurface->getFormat();
  imageData.desc.width = frameBuffersize.width;
  imageData.desc.height = frameBuffersize.height;
  auto buffer =
      std::make_unique<uint8_t[]>(frameBuffersize.width * frameBuffersize.height * bytesPerPixel);

  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  auto commandQueue = platform.getDevice().createCommandQueue(desc, nullptr);
  framebuffer->copyBytesColorAttachment(*commandQueue, 0, buffer.get(), rangeDesc);

  const size_t numPixels = frameBuffersize.width * frameBuffersize.height * bytesPerPixel;

#if IGL_PLATFORM_WIN
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

  IGLLog(IGLLogInfo, "Writing screenshot to: %s", absoluteFilename);
  platform.getImageWriter().writeImage(absoluteFilename, imageData);
}

} // namespace igl::shell
