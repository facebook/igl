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

class RenderCommandEncoderVulkanTest : public ::testing::Test {
 public:
  RenderCommandEncoderVulkanTest() = default;
  ~RenderCommandEncoderVulkanTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";

    Result ret;
    cmdQueue_ = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_NE(cmdQueue_, nullptr);

    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             4,
                                             4,
                                             TextureDesc::TextureUsageBits::Attachment |
                                                 TextureDesc::TextureUsageBits::Sampled);
    colorTex_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_NE(colorTex_, nullptr);

    FramebufferDesc fbDesc;
    fbDesc.colorAttachments[0].texture = colorTex_;
    fb_ = iglDev_->createFramebuffer(fbDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_NE(fb_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ITexture> colorTex_;
  std::shared_ptr<IFramebuffer> fb_;

  std::unique_ptr<IRenderCommandEncoder> createEncoder(std::shared_ptr<ICommandBuffer>& cmdBuf) {
    Result ret;
    cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
    if (!ret.isOk() || !cmdBuf) {
      return nullptr;
    }

    RenderPassDesc rpDesc;
    rpDesc.colorAttachments.resize(1);
    rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
    rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

    auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, fb_, {}, &ret);
    if (!ret.isOk()) {
      return nullptr;
    }
    return encoder;
  }
};

TEST_F(RenderCommandEncoderVulkanTest, BindViewport) {
  std::shared_ptr<ICommandBuffer> cmdBuf;
  auto encoder = createEncoder(cmdBuf);
  ASSERT_NE(encoder, nullptr);

  Viewport vp = {
      .x = 0.0f, .y = 0.0f, .width = 4.0f, .height = 4.0f, .minDepth = 0.0f, .maxDepth = 1.0f};
  encoder->bindViewport(vp);

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

TEST_F(RenderCommandEncoderVulkanTest, BindScissorRect) {
  std::shared_ptr<ICommandBuffer> cmdBuf;
  auto encoder = createEncoder(cmdBuf);
  ASSERT_NE(encoder, nullptr);

  ScissorRect rect = {.x = 0, .y = 0, .width = 4, .height = 4};
  encoder->bindScissorRect(rect);

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

TEST_F(RenderCommandEncoderVulkanTest, SetStencilReferenceValue) {
  std::shared_ptr<ICommandBuffer> cmdBuf;
  auto encoder = createEncoder(cmdBuf);
  ASSERT_NE(encoder, nullptr);

  encoder->setStencilReferenceValue(128);

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

TEST_F(RenderCommandEncoderVulkanTest, SetBlendColor) {
  std::shared_ptr<ICommandBuffer> cmdBuf;
  auto encoder = createEncoder(cmdBuf);
  ASSERT_NE(encoder, nullptr);

  encoder->setBlendColor(Color(1.0f, 0.5f, 0.25f, 1.0f));

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

TEST_F(RenderCommandEncoderVulkanTest, SetDepthBias) {
  std::shared_ptr<ICommandBuffer> cmdBuf;
  auto encoder = createEncoder(cmdBuf);
  ASSERT_NE(encoder, nullptr);

  encoder->setDepthBias(1.0f, 1.0f, 0.0f);

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
