/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/metal/Framebuffer.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <TargetConditionals.h>
#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

#define FB_TEX_WIDTH 16
#define FB_TEX_HEIGHT 16

//
// MetalFramebufferTest
//
// This test covers igl::metal::Framebuffer.
// Tests creation, attachment accessors, drawable updates, and dimensions.
//
class MetalFramebufferTest : public ::testing::Test {
 public:
  MetalFramebufferTest() = default;
  ~MetalFramebufferTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);

    // Create a color texture for framebuffer attachment
    Result res;
    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             FB_TEX_WIDTH,
                                             FB_TEX_HEIGHT,
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
  std::shared_ptr<ITexture> colorTexture_;
};

//
// CreateWithColorAttachment
//
// Test creating a framebuffer with a color texture attachment.
//
TEST_F(MetalFramebufferTest, CreateWithColorAttachment) {
  Result res;
  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture_;
  fbDesc.debugName = "testFB";

  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(framebuffer, nullptr);
}

//
// GetColorAttachment
//
// Verify that color attachment retrieval returns the correct texture.
//
TEST_F(MetalFramebufferTest, GetColorAttachment) {
  Result res;
  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture_;

  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(framebuffer, nullptr);

  auto retrieved = framebuffer->getColorAttachment(0);
  ASSERT_NE(retrieved, nullptr);
  ASSERT_EQ(retrieved.get(), colorTexture_.get());
}

//
// GetDepthAttachment
//
// Verify that depth attachment retrieval works when a depth texture is set.
//
TEST_F(MetalFramebufferTest, GetDepthAttachment) {
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
      depthFormat, FB_TEX_WIDTH, FB_TEX_HEIGHT, TextureDesc::TextureUsageBits::Attachment);
  auto depthTexture = device_->createTexture(depthDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture_;
  fbDesc.depthAttachment.texture = depthTexture;

  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(framebuffer, nullptr);

  auto retrievedDepth = framebuffer->getDepthAttachment();
  ASSERT_NE(retrievedDepth, nullptr);
  ASSERT_EQ(retrievedDepth.get(), depthTexture.get());
}

//
// UpdateDrawable
//
// Test that updateDrawable replaces the color attachment at index 0.
//
TEST_F(MetalFramebufferTest, UpdateDrawable) {
  Result res;
  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture_;

  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(framebuffer, nullptr);

  // Create a new texture and update drawable
  TextureDesc newTexDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                              FB_TEX_WIDTH,
                                              FB_TEX_HEIGHT,
                                              TextureDesc::TextureUsageBits::Sampled |
                                                  TextureDesc::TextureUsageBits::Attachment);
  auto newTexture = device_->createTexture(newTexDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(newTexture, nullptr);

  framebuffer->updateDrawable(newTexture);

  auto retrieved = framebuffer->getColorAttachment(0);
  ASSERT_NE(retrieved, nullptr);
  ASSERT_EQ(retrieved.get(), newTexture.get());
}

//
// GetDimensions
//
// Verify that the framebuffer's color attachment dimensions match the texture.
//
TEST_F(MetalFramebufferTest, GetDimensions) {
  Result res;
  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture_;

  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(framebuffer, nullptr);

  auto colorAttachment = framebuffer->getColorAttachment(0);
  ASSERT_NE(colorAttachment, nullptr);

  auto dims = colorAttachment->getDimensions();
  ASSERT_EQ(dims.width, FB_TEX_WIDTH);
  ASSERT_EQ(dims.height, FB_TEX_HEIGHT);
}

} // namespace igl::tests
