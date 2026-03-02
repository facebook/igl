/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <igl/CommandBuffer.h>
#include <igl/Device.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/RenderPass.h>
#include <igl/Texture.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class ImageLayoutTransitionTest : public ::testing::Test {
 public:
  ImageLayoutTransitionTest() = default;
  ~ImageLayoutTransitionTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";

    Result ret;
    cmdQueue_ = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_NE(cmdQueue_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  void waitForGpu() {
    Result ret;
    auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
    if (cmdBuf) {
      cmdQueue_->submit(*cmdBuf);
      cmdBuf->waitUntilCompleted();
    }
  }
};

TEST_F(ImageLayoutTransitionTest, TransitionToColorAttachment) {
  Result ret;

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           4,
                                           4,
                                           TextureDesc::TextureUsageBits::Attachment |
                                               TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = texture;
  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(fb, nullptr);

  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(cmdBuf, nullptr);

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;
  rpDesc.colorAttachments[0].clearColor = {0, 0, 0, 1};

  auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, fb, {}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(encoder, nullptr);

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

TEST_F(ImageLayoutTransitionTest, TransitionToDepthStencilAttachment) {
  Result ret;

  TextureDesc colorDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             4,
                                             4,
                                             TextureDesc::TextureUsageBits::Attachment |
                                                 TextureDesc::TextureUsageBits::Sampled);
  auto colorTex = iglDev_->createTexture(colorDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(colorTex, nullptr);

  TextureDesc depthDesc = TextureDesc::new2D(TextureFormat::Z_UNorm24,
                                             4,
                                             4,
                                             TextureDesc::TextureUsageBits::Attachment |
                                                 TextureDesc::TextureUsageBits::Sampled);
  auto depthTex = iglDev_->createTexture(depthDesc, &ret);
  if (!ret.isOk()) {
    depthDesc.format = TextureFormat::Z_UNorm16;
    depthTex = iglDev_->createTexture(depthDesc, &ret);
  }
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(depthTex, nullptr);

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTex;
  fbDesc.depthAttachment.texture = depthTex;
  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(fb, nullptr);

  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(cmdBuf, nullptr);

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;
  rpDesc.depthAttachment.loadAction = LoadAction::Clear;
  rpDesc.depthAttachment.storeAction = StoreAction::Store;
  rpDesc.depthAttachment.clearDepth = 1.0f;

  auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, fb, {}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(encoder, nullptr);

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

TEST_F(ImageLayoutTransitionTest, TransitionToShaderReadOnly) {
  Result ret;

  TextureDesc texDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);

  const std::vector<uint32_t> pixels(16, 0xFF0000FF);
  ret = texture->upload(texture->getFullRange(0), pixels.data());
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  waitForGpu();
}

TEST_F(ImageLayoutTransitionTest, TransitionToGeneral) {
  Result ret;

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           4,
                                           4,
                                           TextureDesc::TextureUsageBits::Storage |
                                               TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);

  waitForGpu();
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
