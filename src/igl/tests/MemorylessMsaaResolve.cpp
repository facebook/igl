/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "util/Common.h"

#include <array>
#include <memory>
#include <igl/CommandBuffer.h>
#include <igl/RenderPass.h>

namespace igl::tests {

class MemorylessMsaaResolveTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    // Skip if MultiSample or MultiSampleResolve not supported
    if (!iglDev_->hasFeature(DeviceFeatures::MultiSample) ||
        !iglDev_->hasFeature(DeviceFeatures::MultiSampleResolve)) {
      GTEST_SKIP() << "MultiSample or MultiSampleResolve not supported";
    }

    const TextureDesc msaaDesc{
        .width = kWidth,
        .height = kHeight,
        .numSamples = 4,
        .usage = TextureDesc::TextureUsageBits::Attachment,
        .type = TextureType::TwoD,
        .format = TextureFormat::RGBA_UNorm8,
        .storage = ResourceStorage::Memoryless,
    };

    Result ret;
    msaaTexture_ = iglDev_->createTexture(msaaDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_TRUE(msaaTexture_ != nullptr);

    const TextureDesc resolveDesc{
        .width = kWidth,
        .height = kHeight,
        .usage = TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled,
        .type = TextureType::TwoD,
        .format = TextureFormat::RGBA_UNorm8,
    };

    resolveTexture_ = iglDev_->createTexture(resolveDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_TRUE(resolveTexture_ != nullptr);

    // Create framebuffer with MSAA attachment and resolve
    const FramebufferDesc framebufferDesc{
        .colorAttachments = {{.texture = msaaTexture_, .resolveTexture = resolveTexture_}}};

    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_TRUE(framebuffer_ != nullptr);

    // Initialize render pass: clear to red, resolve to single-sample
    renderPass_ = {
        .colorAttachments = {{.loadAction = LoadAction::Clear,
                              .storeAction = StoreAction::MsaaResolve,
                              .clearColor = {1.0f, 0.0f, 0.0f, 1.0f}}},
    };
  }

 protected:
  static constexpr uint32_t kWidth = 4;
  static constexpr uint32_t kHeight = 4;

  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ITexture> msaaTexture_;
  std::shared_ptr<ITexture> resolveTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  RenderPassDesc renderPass_;
};

TEST_F(MemorylessMsaaResolveTest, MemorylessMsaaResolvesCorrectly) {
  Result ret;

  auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto encoder = cmdBuf->createRenderCommandEncoder(renderPass_, framebuffer_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  const auto readbackFb = iglDev_->createFramebuffer(
      FramebufferDesc{.colorAttachments = {{.texture = resolveTexture_}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  std::array<uint32_t, kWidth * kHeight> pixels{};
  readbackFb->copyBytesColorAttachment(
      *cmdQueue_, 0, pixels.data(), TextureRangeDesc::new2D(0, 0, kWidth, kHeight));

  for (const uint32_t pixel : pixels) {
    ASSERT_EQ(pixel, 0xff0000ffu) << "Expected opaque red; got " << std::hex << pixel;
  }
}

} // namespace igl::tests
