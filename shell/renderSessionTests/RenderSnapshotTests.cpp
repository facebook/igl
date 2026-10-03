/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdlib>
#include <string>
#include <shell/renderSessions/CheckerboardMipmapSession.h>
#include <shell/renderSessions/ColorSession.h>
#include <shell/renderSessions/ComputeSession.h>
#include <shell/renderSessions/DrawInstancedSession.h>
#include <shell/renderSessions/GraphSampleSession.h>
#include <shell/renderSessions/HelloWorldSession.h>
#include <shell/renderSessions/ImguiSession.h>
#include <shell/renderSessions/MRTSession.h>
#include <shell/renderSessions/MSAASession.h>
#include <shell/renderSessions/TQMultiRenderPassSession.h>
#include <shell/renderSessions/TQSession.h>
#include <shell/renderSessions/Texture3DSession.h>
#include <shell/renderSessions/TextureAccessorSession.h>
#include <shell/renderSessions/TextureRotationSession.h>
#include <shell/renderSessions/Textured3DCubeSession.h>
#include <shell/renderSessions/UniformArrayTestSession.h>
#include <shell/renderSessions/UniformPackedTestSession.h>
#include <shell/renderSessions/UniformTestSession.h>
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

TEST_F(RenderSnapshotTests, CheckerboardMipmapSession) {
  CheckerboardMipmapSession session(platform_);
  render(session, "CheckerboardMipmapSession");
}

TEST_F(RenderSnapshotTests, DrawInstancedSession) {
  DrawInstancedSession session(platform_);
  render(session, "DrawInstancedSession");
}

TEST_F(RenderSnapshotTests, MRTSession) {
  MRTSession session(platform_);
  render(session, "MRTSession");
}

TEST_F(RenderSnapshotTests, TQMultiRenderPassSession) {
  TQMultiRenderPassSession session(platform_);
  render(session, "TQMultiRenderPassSession");
}

TEST_F(RenderSnapshotTests, Texture3DSession) {
  Texture3DSession session(platform_);
  render(session, "Texture3DSession");
}

TEST_F(RenderSnapshotTests, Textured3DCubeSession) {
  Textured3DCubeSession session(platform_);
  render(session, "Textured3DCubeSession");
}

// The inset shows frame 5 as read back by the texture accessor at frame 10.
TEST_F(RenderSnapshotTests, TextureAccessorSession) {
  TextureAccessorSession session(platform_);
  render(session, "TextureAccessorSession", 12);
}

TEST_F(RenderSnapshotTests, MSAASession) {
  MSAASession session(platform_);
  render(session, "MSAASession");
}

TEST_F(RenderSnapshotTests, ComputeSession) {
  if (!platform_->getDevice().hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "Compute is not supported";
  }
  ComputeSession session(platform_);
  render(session, "ComputeSession");
}

TEST_F(RenderSnapshotTests, GraphSampleSession) {
  GraphSampleSession session(platform_);
  render(session, "GraphSampleSession");
}

TEST_F(RenderSnapshotTests, TextureRotationSession) {
  TextureRotationSession session(platform_);
  render(session, "TextureRotationSession");
}

TEST_F(RenderSnapshotTests, UniformTestSession) {
  UniformTestSession session(platform_);
  render(session, "UniformTestSession");
}

TEST_F(RenderSnapshotTests, UniformPackedTestSession) {
  UniformPackedTestSession session(platform_);
  render(session, "UniformPackedTestSession");
}

TEST_F(RenderSnapshotTests, ImguiSession) {
  ImguiSession session(platform_);
  // ImGui lays windows out in the first frame and draws them from the second.
  render(session, "ImguiSession", 2);
}

TEST_F(RenderSnapshotTests, UniformArrayTestSession) {
  UniformArrayTestSession session(platform_);
  render(session, "UniformArrayTestSession");
}

} // namespace igl::shell
