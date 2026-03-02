/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/Device.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/RenderPass.h>
#include <igl/SamplerState.h>
#include <igl/Texture.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class ResourcesBinderTest : public ::testing::Test {
 public:
  ResourcesBinderTest() = default;
  ~ResourcesBinderTest() override = default;

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

TEST_F(ResourcesBinderTest, BindBufferAndDraw) {
  Result ret;

  BufferDesc bufDesc;
  bufDesc.type = BufferDesc::BufferTypeBits::Uniform;
  bufDesc.storage = ResourceStorage::Shared;
  bufDesc.length = 256;
  auto buffer = iglDev_->createBuffer(bufDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  std::vector<float> data(64, 1.0f);
  ret = buffer->upload(data.data(), BufferRange(256, 0));
  ASSERT_TRUE(ret.isOk());

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           4,
                                           4,
                                           TextureDesc::TextureUsageBits::Attachment |
                                               TextureDesc::TextureUsageBits::Sampled);
  auto colorTex = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk());

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTex;
  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk());

  auto cmdQueue = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
  ASSERT_TRUE(ret.isOk());
  auto cmdBuf = cmdQueue->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, fb, {}, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(encoder, nullptr);

  encoder->bindBuffer(0, buffer.get(), 0, 256);

  encoder->endEncoding();
  cmdQueue->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

TEST_F(ResourcesBinderTest, BindTextureAndSamplerAndDraw) {
  Result ret;

  TextureDesc sampledTexDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  auto sampledTex = iglDev_->createTexture(sampledTexDesc, &ret);
  ASSERT_TRUE(ret.isOk());

  const std::vector<uint32_t> pixels(16, 0xFF0000FF);
  ret = sampledTex->upload(sampledTex->getFullRange(0), pixels.data());
  ASSERT_TRUE(ret.isOk());

  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = SamplerMinMagFilter::Linear;
  samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  auto sampler = iglDev_->createSamplerState(samplerDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(sampler, nullptr);

  TextureDesc colorTexDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                4,
                                                4,
                                                TextureDesc::TextureUsageBits::Attachment |
                                                    TextureDesc::TextureUsageBits::Sampled);
  auto colorTex = iglDev_->createTexture(colorTexDesc, &ret);
  ASSERT_TRUE(ret.isOk());

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTex;
  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk());

  auto cmdQueue = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
  ASSERT_TRUE(ret.isOk());
  auto cmdBuf = cmdQueue->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, fb, {}, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(encoder, nullptr);

  encoder->bindTexture(0, sampledTex.get());
  encoder->bindSamplerState(0, 0, sampler.get());

  encoder->endEncoding();
  cmdQueue->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
