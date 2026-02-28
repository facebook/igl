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

#define BYTES_TEX_WIDTH 8
#define BYTES_TEX_HEIGHT 8

//
// MetalBytesBindingTest
//
// This test covers binding bytes to the render encoder on Metal.
// Tests binding small constant data directly to vertex and fragment stages.
//
class MetalBytesBindingTest : public ::testing::Test {
 public:
  MetalBytesBindingTest() = default;
  ~MetalBytesBindingTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);

    Result res;

    // Create render target
    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             BYTES_TEX_WIDTH,
                                             BYTES_TEX_HEIGHT,
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
// BindBytesVertex
//
// Test binding inline bytes to the vertex stage.
//
TEST_F(MetalBytesBindingTest, BindBytesVertex) {
  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  const float uniformData[] = {1.0f, 0.0f, 0.0f};
  encoder->bindBytes(2, BindTarget::kVertex, uniformData, sizeof(uniformData));
  encoder->endEncoding();
}

//
// BindBytesFragment
//
// Test binding inline bytes to the fragment stage.
//
TEST_F(MetalBytesBindingTest, BindBytesFragment) {
  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  const float uniformData[] = {0.0f, 1.0f, 0.0f, 1.0f};
  encoder->bindBytes(2, BindTarget::kFragment, uniformData, sizeof(uniformData));
  encoder->endEncoding();
}

} // namespace igl::tests
