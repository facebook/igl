/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageWriter/ImageWriter.h>
#include <shell/shared/renderSession/ScreenshotTestRenderSessionHelper.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

void SaveFrameBufferToPng(const char* absoluteFilename,
                          const std::shared_ptr<IFramebuffer>& framebuffer,
                          Platform& platform) {
  igl::Result ret;
  auto drawableSurface = framebuffer->getColorAttachment(0);
  auto frameBuffersize = drawableSurface->getDimensions();
  int const bytesPerPixel = 4;
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

  imageData.data = iglu::textureloader::IData::tryCreate(
      std::move(buffer), frameBuffersize.width * frameBuffersize.height * bytesPerPixel, nullptr);

  IGLLog(IGLLogLevel::LOG_INFO, "Writing screenshot to: %s", absoluteFilename);
  platform.getImageWriter().writeImage(absoluteFilename, imageData);
}

} // namespace igl::shell
