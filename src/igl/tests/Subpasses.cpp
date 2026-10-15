/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "util/Common.h"

#include <igl/CommandBuffer.h>
#include <igl/Framebuffer.h>
#include <igl/RenderPass.h>

namespace igl::tests {

class SubpassesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);
  }

  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  [[nodiscard]] std::shared_ptr<ITexture> createColorTexture(
      TextureDesc::TextureUsage usage) const {
    const TextureDesc desc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, usage);
    Result ret;
    const auto tex = iglDev_->createTexture(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    EXPECT_NE(tex, nullptr);
    return tex;
  }

  [[nodiscard]] std::shared_ptr<IFramebuffer> createFramebuffer(
      const std::vector<std::shared_ptr<ITexture>>& colorAttachments) const {
    FramebufferDesc desc;
    for (size_t i = 0; i < colorAttachments.size() && i < IGL_COLOR_ATTACHMENTS_MAX; ++i) {
      desc.colorAttachments[i].texture = colorAttachments[i];
    }
    Result ret;
    const auto fb = iglDev_->createFramebuffer(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    EXPECT_NE(fb, nullptr);
    return fb;
  }

  [[nodiscard]] static RenderPassDesc makeRenderPass(
      size_t numColorAttachments,
      std::vector<RenderPassDesc::SubpassDesc> subpasses) {
    RenderPassDesc renderPass;
    renderPass.colorAttachments.resize(numColorAttachments);
    for (size_t i = 0; i < numColorAttachments; ++i) {
      renderPass.colorAttachments[i].loadAction = LoadAction::Clear;
      renderPass.colorAttachments[i].storeAction = StoreAction::Store;
    }
    renderPass.subpasses = std::move(subpasses);
    return renderPass;
  }
};

TEST_F(SubpassesTest, ValidateAcceptsDeferredLayout) {
  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto att2 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto fb = createFramebuffer({att0, att1, att2});
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass =
      makeRenderPass(3,
                     {{.colorAttachmentMask = 0b110},
                      {.colorAttachmentMask = 0b001, .inputAttachmentMask = 0b110}});

  const Result ret = renderPass.validate(*fb);
  EXPECT_TRUE(ret.isOk());
}

TEST_F(SubpassesTest, ValidateRejectsStereoFramebuffer) {
  if (!iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets)) {
    GTEST_SKIP() << "Device does not support multiple render targets";
  }

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture =
      createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  fbDesc.mode = FramebufferMode::Stereo;
  Result ret;
  const auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result validateRet = renderPass.validate(*fb);
  EXPECT_EQ(validateRet.code, Result::Code::Unsupported);
}

TEST_F(SubpassesTest, ValidateRejectsInputWithoutUsageBit) {
  if (!iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets)) {
    GTEST_SKIP() << "Device does not support multiple render targets";
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = createFramebuffer({att0, att1});
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass = makeRenderPass(
      2,
      {{.colorAttachmentMask = 0b01}, {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0b01}});

  const Result ret = renderPass.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST_F(SubpassesTest, ValidateRejectsMultisampled) {
  if (!iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets)) {
    GTEST_SKIP() << "Device does not support multiple render targets";
  }

  if (!iglDev_->hasFeature(DeviceFeatures::MultiSample)) {
    GTEST_SKIP() << "MultiSample feature not supported";
  }

  TextureDesc msaaDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Attachment);
  msaaDesc.numSamples = 4;

  Result ret;
  const auto msTex = iglDev_->createTexture(msaaDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(msTex, nullptr);

  const auto fb = createFramebuffer({msTex});
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result validateRet = renderPass.validate(*fb);
  EXPECT_EQ(validateRet.code, Result::Code::Unsupported);
}

TEST_F(SubpassesTest, EncoderRejectsSubpassesWhenUnsupported) {
  if (iglDev_->hasFeature(DeviceFeatures::Subpasses)) {
    GTEST_SKIP() << "Device supports subpasses";
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = createFramebuffer({att0});
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  Result ret;
  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(cmdBuf, nullptr);

  const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
  EXPECT_EQ(encoder, nullptr);
}

} // namespace igl::tests
