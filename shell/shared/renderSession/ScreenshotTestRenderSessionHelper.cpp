/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/renderSession/ScreenshotTestRenderSessionHelper.h>

#include <cstdlib>
#include <shell/shared/imageWriter/ImageWriter.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {
namespace {
void SaveFrameBufferToPng(const char* absoluteFilename,
                          const std::shared_ptr<IFramebuffer>& framebuffer,
                          Platform& platform) {
  igl::Result ret;
  auto drawableSurface = framebuffer->getColorAttachment(0);
  auto frameBuffersize = drawableSurface->getSize();
  int const bytesPerPixel = 4;
  auto bytesPerRow = frameBuffersize.width * bytesPerPixel;
  const auto rangeDesc =
      TextureRangeDesc::new2D(0, 0, frameBuffersize.width, frameBuffersize.height);
  igl::shell::ImageData imageData;
  imageData.width = frameBuffersize.width;
  imageData.height = frameBuffersize.height;
  imageData.bytesPerRow = bytesPerRow;
  imageData.buffer.resize(frameBuffersize.width * frameBuffersize.height * bytesPerPixel);

  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  auto commandQueue = platform.getDevice().createCommandQueue(desc, nullptr);
  framebuffer->copyBytesColorAttachment(*commandQueue, 0, imageData.buffer.data(), rangeDesc);

  IGLLog(IGLLogLevel::LOG_INFO, "Writing screenshot to: %s", absoluteFilename);
  platform.getImageWriter().writeImage(absoluteFilename, imageData);
}
} // namespace

void ScreenshotTestRenderSessionHelper::initialize(AppParams& appParams) noexcept {
  const char* screenshotTestsOutPath = std::getenv("SCREENSHOT_TESTS_OUT");
  const char* screenshotTestsFrame = std::getenv("SCREENSHOT_TESTS_FRAME");
  if (screenshotTestsOutPath && screenshotTestsFrame) {
    IGLLog(
        IGLLogLevel::LOG_INFO,
        "[screenshot test] Env variables: SCREENSHOT_TESTS_OUT = %s, SCREENSHOT_TESTS_FRAME = %s",
        screenshotTestsOutPath,
        screenshotTestsFrame);
    int frameCount = atoi(screenshotTestsFrame);
    appParams.screenshotTestsParams.outputPath_ = screenshotTestsOutPath;
    appParams.screenshotTestsParams.frameToCapture_ = frameCount;
  }
}

bool ScreenshotTestRenderSessionHelper::update(const AppParams& appParams,
                                               const ShellParams& shellParams,
                                               const igl::SurfaceTextures& surfaceTextures,
                                               Platform& platform) {
  const std::string& screenshotTestsOutPath = appParams.screenshotTestsParams.outputPath_;
  if (!screenshotTestsOutPath.empty()) {
    int frameCount = appParams.screenshotTestsParams.frameToCapture_;
    if (frameTicked_ == frameCount) {
      IGLLog(IGLLogLevel::LOG_INFO,
             "[screenshot test] Saving Screenshot %s",
             screenshotTestsOutPath.c_str());
      igl::FramebufferDesc framebufferDesc;
      framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
      framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
      igl::Result ret;
      auto framebuffer = platform.getDevice().createFramebuffer(framebufferDesc, &ret);
      SaveFrameBufferToPng(screenshotTestsOutPath.c_str(), framebuffer, platform);
      return true;
    }
  }
  ++frameTicked_;
  return false;
}

} // namespace igl::shell
