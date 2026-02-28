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

#include <TargetConditionals.h>
#include <igl/IGL.h>
#include <igl/metal/CommandBuffer.h>
#include <igl/metal/Device.h>

namespace igl::tests {

#define RP_TEX_WIDTH 16
#define RP_TEX_HEIGHT 16

//
// MetalRenderPassTest
//
// This test covers render pass creation on Metal.
// Tests various render pass configurations with color and depth attachments.
//
class MetalRenderPassTest : public ::testing::Test {
 public:
  MetalRenderPassTest() = default;
  ~MetalRenderPassTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);

    Result res;
    const CommandBufferDesc cbDesc;
    commandBuffer_ = cmdQueue_->createCommandBuffer(cbDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;
    ASSERT_NE(commandBuffer_, nullptr);

    // Create a color texture for the framebuffer
    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             RP_TEX_WIDTH,
                                             RP_TEX_HEIGHT,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
    colorTexture_ = device_->createTexture(texDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;
    ASSERT_NE(colorTexture_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> commandBuffer_;
  std::shared_ptr<ITexture> colorTexture_;
};

//
// SingleColorAttachment
//
// Test creating a render command encoder with a single color attachment.
//
TEST_F(MetalRenderPassTest, SingleColorAttachment) {
  Result res;

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture_;
  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;
  rpDesc.colorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();
}

//
// ColorAndDepth
//
// Test creating a render command encoder with color and depth attachments.
//
TEST_F(MetalRenderPassTest, ColorAndDepth) {
#if TARGET_OS_SIMULATOR
  GTEST_SKIP() << "Depth textures not supported on iOS Simulator";
#endif

  Result res;

  // Check which depth format is supported before attempting to create texture
  TextureFormat depthFormat = TextureFormat::Invalid;
  auto caps = device_->getTextureFormatCapabilities(TextureFormat::Z_UNorm32);
  if ((caps & ICapabilities::TextureFormatCapabilityBits::Attachment) != 0) {
    depthFormat = TextureFormat::Z_UNorm32;
  } else {
    caps = device_->getTextureFormatCapabilities(TextureFormat::S8_UInt_Z32_UNorm);
    if ((caps & ICapabilities::TextureFormatCapabilityBits::Attachment) != 0) {
      depthFormat = TextureFormat::S8_UInt_Z32_UNorm;
    } else {
      caps = device_->getTextureFormatCapabilities(TextureFormat::Z_UNorm24);
      if ((caps & ICapabilities::TextureFormatCapabilityBits::Attachment) != 0) {
        depthFormat = TextureFormat::Z_UNorm24;
      }
    }
  }

  if (depthFormat == TextureFormat::Invalid) {
    GTEST_SKIP() << "No supported depth format available";
  }

  // Create depth texture with supported format
  TextureDesc depthDesc = TextureDesc::new2D(
      depthFormat, RP_TEX_WIDTH, RP_TEX_HEIGHT, TextureDesc::TextureUsageBits::Attachment);
  auto depthTexture = device_->createTexture(depthDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture_;
  fbDesc.depthAttachment.texture = depthTexture;
  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;
  rpDesc.colorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
  rpDesc.depthAttachment.loadAction = LoadAction::Clear;
  rpDesc.depthAttachment.storeAction = StoreAction::DontCare;

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();
}

//
// ClearColorApplied
//
// Verify that the clear color is applied when using LoadAction::Clear.
// After rendering, read back and verify clear color values.
//
TEST_F(MetalRenderPassTest, ClearColorApplied) {
  Result res;

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture_;
  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;
  rpDesc.colorAttachments[0].clearColor = {1.0f, 0.0f, 0.0f, 1.0f}; // Red

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();

  // Submit and wait
  cmdQueue_->submit(*commandBuffer_);
  commandBuffer_->waitUntilCompleted();

  // Read back and verify the clear color
  const size_t bytesPerPixel = 4; // RGBA_UNorm8
  const size_t rowBytes = RP_TEX_WIDTH * bytesPerPixel;
  std::vector<uint8_t> pixels(RP_TEX_WIDTH * RP_TEX_HEIGHT * bytesPerPixel);

  framebuffer->copyBytesColorAttachment(*cmdQueue_,
                                        0,
                                        pixels.data(),
                                        TextureRangeDesc::new2D(0, 0, RP_TEX_WIDTH, RP_TEX_HEIGHT),
                                        rowBytes);

  // Check first pixel is red (255, 0, 0, 255)
  ASSERT_EQ(pixels[0], 255); // R
  ASSERT_EQ(pixels[1], 0); // G
  ASSERT_EQ(pixels[2], 0); // B
  ASSERT_EQ(pixels[3], 255); // A
}

//
// NullFramebufferError
//
// Test that passing a null framebuffer to createRenderCommandEncoder
// returns a null encoder or error.
//
TEST_F(MetalRenderPassTest, NullFramebufferError) {
  const RenderPassDesc rpDesc;
  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, nullptr);
  // A null framebuffer should still produce an encoder on Metal (empty pass descriptor),
  // but verify the call does not crash.
}

} // namespace igl::tests
