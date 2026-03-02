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
#include <igl/Texture.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class FramebufferVulkanTest : public ::testing::Test {
 public:
  FramebufferVulkanTest() = default;
  ~FramebufferVulkanTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
};

TEST_F(FramebufferVulkanTest, CreateWithColorAttachment) {
  Result ret;

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           8,
                                           8,
                                           TextureDesc::TextureUsageBits::Attachment |
                                               TextureDesc::TextureUsageBits::Sampled);
  auto colorTex = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTex;

  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(fb, nullptr);

  auto indices = fb->getColorAttachmentIndices();
  EXPECT_FALSE(indices.empty());
  EXPECT_NE(fb->getColorAttachment(0), nullptr);
}

TEST_F(FramebufferVulkanTest, CreateWithColorAndDepth) {
  Result ret;

  TextureDesc colorDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             8,
                                             8,
                                             TextureDesc::TextureUsageBits::Attachment |
                                                 TextureDesc::TextureUsageBits::Sampled);
  auto colorTex = iglDev_->createTexture(colorDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  TextureDesc depthDesc = TextureDesc::new2D(TextureFormat::Z_UNorm24,
                                             8,
                                             8,
                                             TextureDesc::TextureUsageBits::Attachment |
                                                 TextureDesc::TextureUsageBits::Sampled);
  auto depthTex = iglDev_->createTexture(depthDesc, &ret);
  if (!ret.isOk()) {
    depthDesc.format = TextureFormat::Z_UNorm16;
    depthTex = iglDev_->createTexture(depthDesc, &ret);
  }
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTex;
  fbDesc.depthAttachment.texture = depthTex;

  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(fb, nullptr);
  EXPECT_NE(fb->getDepthAttachment(), nullptr);
}

TEST_F(FramebufferVulkanTest, CopyBytesColorAttachment) {
  Result ret;

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           2,
                                           2,
                                           TextureDesc::TextureUsageBits::Attachment |
                                               TextureDesc::TextureUsageBits::Sampled);
  auto colorTex = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  constexpr uint32_t kColor = 0xFF00FF00;
  const std::array<uint32_t, 4> pixels = {kColor, kColor, kColor, kColor};
  ret = colorTex->upload(colorTex->getFullRange(0), pixels.data());
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTex;
  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto cmdQueue = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
  ASSERT_TRUE(ret.isOk());

  auto cmdBuf = cmdQueue->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());
  cmdQueue->submit(*cmdBuf);

  std::array<uint32_t, 4> result = {};
  fb->copyBytesColorAttachment(*cmdQueue, 0, result.data(), colorTex->getFullRange(0));

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(result[i], kColor);
  }
}

TEST_F(FramebufferVulkanTest, UpdateDrawable) {
  Result ret;

  TextureDesc texDesc1 = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                            4,
                                            4,
                                            TextureDesc::TextureUsageBits::Attachment |
                                                TextureDesc::TextureUsageBits::Sampled);
  auto colorTex1 = iglDev_->createTexture(texDesc1, &ret);
  ASSERT_TRUE(ret.isOk());

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTex1;
  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(fb, nullptr);

  TextureDesc texDesc2 = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                            8,
                                            8,
                                            TextureDesc::TextureUsageBits::Attachment |
                                                TextureDesc::TextureUsageBits::Sampled);
  auto colorTex2 = iglDev_->createTexture(texDesc2, &ret);
  ASSERT_TRUE(ret.isOk());

  fb->updateDrawable(colorTex2);
  EXPECT_EQ(fb->getColorAttachment(0), colorTex2);
}

TEST_F(FramebufferVulkanTest, MultipleColorAttachments) {
  Result ret;

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           4,
                                           4,
                                           TextureDesc::TextureUsageBits::Attachment |
                                               TextureDesc::TextureUsageBits::Sampled);

  auto colorTex0 = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  auto colorTex1 = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk());

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTex0;
  fbDesc.colorAttachments[1].texture = colorTex1;

  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(fb, nullptr);

  auto indices = fb->getColorAttachmentIndices();
  EXPECT_EQ(indices.size(), 2u);
  EXPECT_NE(fb->getColorAttachment(0), nullptr);
  EXPECT_NE(fb->getColorAttachment(1), nullptr);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
