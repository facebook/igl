/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../snapshot_test_support/TinyRenderable.hpp"

#include <memory>
#include <nlohmann/json.hpp>
#include <igl/IGL.h>
#include <igl/tests/util/device/TestDevice.h>

namespace iglu::kit::tests {

class TinyRenderableTest : public ::testing::Test {
 public:
  void SetUp() override {
    igl::setDebugBreakEnabled(false);

    device_ = igl::tests::util::device::createTestDevice(igl::BackendType::Metal);
    if (!device_) {
      GTEST_SKIP() << "Metal backend not available";
    }

    igl::Result queueResult;
    igl::CommandQueueDesc cqDesc = {};
    cmdQueue_ = device_->createCommandQueue(cqDesc, &queueResult);
    ASSERT_TRUE(queueResult.isOk()) << queueResult.message;
    ASSERT_TRUE(cmdQueue_ != nullptr);

    igl::TextureDesc texDesc =
        igl::TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                kWidth,
                                kHeight,
                                igl::TextureDesc::TextureUsageBits::Sampled |
                                    igl::TextureDesc::TextureUsageBits::Attachment);

    igl::Result result;
    offscreenTexture_ = device_->createTexture(texDesc, &result);
    ASSERT_TRUE(result.isOk()) << result.message;
    ASSERT_TRUE(offscreenTexture_ != nullptr);

    igl::FramebufferDesc fbDesc;
    fbDesc.colorAttachments[0].texture = offscreenTexture_;
    framebuffer_ = device_->createFramebuffer(fbDesc, &result);
    ASSERT_TRUE(result.isOk()) << result.message;
    ASSERT_TRUE(framebuffer_ != nullptr);
  }

 protected:
  static constexpr uint32_t kWidth = 64;
  static constexpr uint32_t kHeight = 64;

  std::unique_ptr<igl::IDevice> device_;
  std::shared_ptr<igl::ICommandQueue> cmdQueue_;
  std::shared_ptr<igl::ITexture> offscreenTexture_;
  std::shared_ptr<igl::IFramebuffer> framebuffer_;
};

TEST_F(TinyRenderableTest, GetPropertiesReturnsEmptyJson) {
  TinyRenderable renderable;
  const auto& props = renderable.getProperties();
  EXPECT_TRUE(props.empty());
  EXPECT_TRUE(props.is_null());
}

TEST_F(TinyRenderableTest, GetPropertiesReturnsSameStaticInstance) {
  TinyRenderable renderable;
  const auto& props1 = renderable.getProperties();
  const auto& props2 = renderable.getProperties();
  EXPECT_EQ(&props1, &props2);
}

TEST_F(TinyRenderableTest, InitializeAndSubmitRenderPipeline) {
  TinyRenderable renderable;

  igl::DeviceScope scope(*device_);
  renderable.initialize(*device_, *framebuffer_);
  renderable.update(*device_);

  igl::Result result;
  igl::CommandBufferDesc cbDesc;
  auto commandBuffer = cmdQueue_->createCommandBuffer(cbDesc, &result);
  ASSERT_TRUE(result.isOk()) << result.message;

  igl::RenderPassDesc renderPass;
  renderPass.colorAttachments.resize(1);
  renderPass.colorAttachments[0].loadAction = igl::LoadAction::Clear;
  renderPass.colorAttachments[0].storeAction = igl::StoreAction::Store;
  renderPass.colorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

  auto cmds = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer_);
  ASSERT_TRUE(cmds != nullptr);

  renderable.submit(*cmds);
  cmds->endEncoding();

  cmdQueue_->submit(*commandBuffer);
}

} // namespace iglu::kit::tests
