/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/CommandBuffer.h>
#include <igl/metal/Device.h>

namespace igl::tests {

#define ENCODER_TEX_WIDTH 8
#define ENCODER_TEX_HEIGHT 8

//
// MetalRenderEncoderStateTest
//
// This test covers render encoder state-setting operations on Metal.
// Tests viewport, scissor, stencil reference, blend color, and depth bias.
//
class MetalRenderEncoderStateTest : public ::testing::Test {
 public:
  MetalRenderEncoderStateTest() = default;
  ~MetalRenderEncoderStateTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);

    Result res;

    // Create render target
    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             ENCODER_TEX_WIDTH,
                                             ENCODER_TEX_HEIGHT,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
    colorTexture_ = device_->createTexture(texDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;

    FramebufferDesc fbDesc;
    fbDesc.colorAttachments[0].texture = colorTexture_;
    framebuffer_ = device_->createFramebuffer(fbDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;

    const CommandBufferDesc cbDesc;
    commandBuffer_ = cmdQueue_->createCommandBuffer(cbDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ITexture> colorTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<ICommandBuffer> commandBuffer_;
};

//
// BindViewport
//
// Test setting a viewport on the render encoder without crash.
//
TEST_F(MetalRenderEncoderStateTest, BindViewport) {
  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  Viewport viewport = {0.0f, 0.0f, (float)ENCODER_TEX_WIDTH, (float)ENCODER_TEX_HEIGHT, 0.0f, 1.0f};
  encoder->bindViewport(viewport);
  encoder->endEncoding();
}

//
// BindScissorRect
//
// Test setting a scissor rect on the render encoder without crash.
//
TEST_F(MetalRenderEncoderStateTest, BindScissorRect) {
  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  ScissorRect scissor = {0, 0, ENCODER_TEX_WIDTH, ENCODER_TEX_HEIGHT};
  encoder->bindScissorRect(scissor);
  encoder->endEncoding();
}

//
// SetStencilReferenceValue
//
// Test setting a stencil reference value without crash.
//
TEST_F(MetalRenderEncoderStateTest, SetStencilReferenceValue) {
  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  encoder->setStencilReferenceValue(128);
  encoder->endEncoding();
}

//
// SetBlendColor
//
// Test setting a blend color without crash.
//
TEST_F(MetalRenderEncoderStateTest, SetBlendColor) {
  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  encoder->setBlendColor(Color(1.0f, 0.5f, 0.25f, 1.0f));
  encoder->endEncoding();
}

//
// SetDepthBias
//
// Test setting depth bias without crash.
//
TEST_F(MetalRenderEncoderStateTest, SetDepthBias) {
  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  encoder->setDepthBias(1.0f, 1.0f, 0.0f);
  encoder->endEncoding();
}

} // namespace igl::tests
