/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/ScreenshotTestRenderSessionHelper.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <shell/shared/testShell/TestShell.h>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Framebuffer.h>

namespace igl::shell {

// Renders sessions at a visible size. With IGL_RENDER_SNAPSHOT_DIR set, each frame is written to
// <dir>/<Session>.png, so the output of different backends can be compared.
class RenderSnapshotTests : public ::testing::Test, public TestShellBase {
 public:
  void SetUp() override {
    setUpInternal({.width = kSize, .height = kSize});
  }

 protected:
  static constexpr size_t kSize = 256;

  // The default depth texture has no stencil.
  void useDepthStencilTexture() {
    IDevice& device = platform_->getDevice();
    const TextureFormat format =
        device.getTextureFormatCapabilities(TextureFormat::S8_UInt_Z32_UNorm) != 0
            ? TextureFormat::S8_UInt_Z32_UNorm
            : TextureFormat::S8_UInt_Z24_UNorm;
    offscreenDepthTexture_ = device.createTexture(
        TextureDesc::new2D(format, kSize, kSize, TextureDesc::TextureUsageBits::Attachment),
        nullptr);
    ASSERT_NE(offscreenDepthTexture_, nullptr);
  }

  // For sessions that have no path for some backends on macOS (or crash there).
  [[nodiscard]] bool backendIs(std::initializer_list<BackendType> backends) const {
    const BackendType backend = platform_->getDevice().getBackendType();
    return std::find(backends.begin(), backends.end(), backend) != backends.end();
  }

  void render(RenderSession& session, const char* name, int frames = 1) {
    ShellParams params;
    params.viewportSize = glm::vec2(static_cast<float>(kSize), static_cast<float>(kSize));
    session.setShellParams(params);
    // The default clear color identifies the backend; comparisons need the same one.
    session.setPreferredClearColor({0.2f, 0.3f, 0.4f, 1.0f});
    session.initialize();
    const SurfaceTextures surfaceTextures = {.color = offscreenTexture_,
                                             .depth = offscreenDepthTexture_};
    for (int frame = 0; frame < frames; ++frame) {
      const DeviceScope scope(platform_->getDevice());
      session.update(surfaceTextures);
    }
    Result ret;
    auto framebuffer = platform_->getDevice().createFramebuffer(
        {.colorAttachments = {{.texture = offscreenTexture_}}}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(framebuffer, nullptr);
    // Waits for the session's work, on the session's queue, before the readback.
    ICommandQueue* queue = session.getCommandQueue();
    ASSERT_NE(queue, nullptr);
    auto cmdBuffer = queue->createCommandBuffer({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(cmdBuffer, nullptr);
    queue->submit(*cmdBuffer, true);
    cmdBuffer->waitUntilCompleted();

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    if (const char* dir = std::getenv("IGL_RENDER_SNAPSHOT_DIR"); dir != nullptr && *dir != '\0') {
      saveFrameBufferToPng(
          (std::string(dir) + "/" + name + ".png").c_str(), framebuffer, *platform_);
    }
    session.teardown();
  }
};

} // namespace igl::shell
