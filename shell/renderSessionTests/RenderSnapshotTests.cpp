/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <string>
#include <shell/renderSessions/ColorSession.h>
#include <shell/renderSessions/HelloWorldSession.h>
#include <shell/renderSessions/TQSession.h>
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

  void render(RenderSession& session, const char* name) {
    ShellParams params;
    params.viewportSize = glm::vec2(static_cast<float>(kSize), static_cast<float>(kSize));
    session.setShellParams(params);
    // The default clear color identifies the backend; comparisons need the same one.
    session.setPreferredClearColor({0.2f, 0.3f, 0.4f, 1.0f});
    session.initialize();
    const SurfaceTextures surfaceTextures = {.color = offscreenTexture_,
                                             .depth = offscreenDepthTexture_};
    {
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

TEST_F(RenderSnapshotTests, HelloWorldSession) {
  HelloWorldSession session(platform_);
  render(session, "HelloWorldSession");
}

TEST_F(RenderSnapshotTests, ColorSession) {
  ColorSession session(platform_);
  render(session, "ColorSession");
}

TEST_F(RenderSnapshotTests, TQSession) {
  TQSession session(platform_);
  render(session, "TQSession");
}

} // namespace igl::shell
