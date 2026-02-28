/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../data/ShaderData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"

#include <igl/CommandBuffer.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/VertexInputState.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/IContext.h>

namespace igl::tests {

#define OFFSCREEN_TEX_WIDTH 2
#define OFFSCREEN_TEX_HEIGHT 2

//
// CommandBufferOGLTest
//
// Tests for the OpenGL CommandBuffer.
//
class CommandBufferOGLTest : public ::testing::Test {
 public:
  CommandBufferOGLTest() = default;
  ~CommandBufferOGLTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);

    context_ = &static_cast<opengl::Device&>(*iglDev_).getContext();

    Result ret;

    // Create offscreen texture
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   OFFSCREEN_TEX_WIDTH,
                                                   OFFSCREEN_TEX_HEIGHT,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment);
    offscreenTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);

    // Create framebuffer
    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = offscreenTexture_;
    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);

    // Initialize render pass
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  opengl::IContext* context_ = nullptr;

  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;
};

//
// CreateFromQueue
//
// Create a command buffer from the command queue.
//
TEST_F(CommandBufferOGLTest, CreateFromQueue) {
  Result ret;
  CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_NE(cmdBuf, nullptr);
}

//
// CreateRenderEncoder
//
// Create a render command encoder from a command buffer.
//
TEST_F(CommandBufferOGLTest, CreateRenderEncoder) {
  Result ret;
  CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_NE(cmdBuf, nullptr);

  auto cmdEncoder = cmdBuf->createRenderCommandEncoder(renderPass_, framebuffer_);
  ASSERT_NE(cmdEncoder, nullptr);

  cmdEncoder->endEncoding();
}

//
// SubmitToQueue
//
// Submit a command buffer to the queue.
//
TEST_F(CommandBufferOGLTest, SubmitToQueue) {
  Result ret;
  CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_NE(cmdBuf, nullptr);

  auto cmdEncoder = cmdBuf->createRenderCommandEncoder(renderPass_, framebuffer_);
  ASSERT_NE(cmdEncoder, nullptr);
  cmdEncoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);

  // Verify no GL errors after submission
  ASSERT_EQ(context_->checkForErrors(__FILE__, __LINE__), GL_NO_ERROR);
}

} // namespace igl::tests
