/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/Common.h"

#include <igl/CommandBuffer.h>
#include <igl/RenderPass.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/DeviceFeatureSet.h>
#include <igl/opengl/IContext.h>

namespace igl::tests {

//
// MSAATextureAttachmentOGLTest
//
// Tests for MSAA (Multi-Sample Anti-Aliasing) texture attachment in OpenGL.
//
class MSAATextureAttachmentOGLTest : public ::testing::Test {
 public:
  MSAATextureAttachmentOGLTest() = default;
  ~MSAATextureAttachmentOGLTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);

    context_ = &static_cast<opengl::Device&>(*iglDev_).getContext();
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  opengl::IContext* context_ = nullptr;
};

//
// CreateMSAARenderbuffer
//
// Create a multisampled texture for use as a render target.
//
TEST_F(MSAATextureAttachmentOGLTest, CreateMSAARenderbuffer) {
  if (!iglDev_->hasFeature(DeviceFeatures::MultiSample)) {
    GTEST_SKIP() << "MultiSample not supported";
  }

  Result ret;

  TextureDesc texDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Attachment);
  texDesc.numSamples = 4;

  auto msaaTexture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(msaaTexture, nullptr);

  ASSERT_EQ(context_->checkForErrors(__FILE__, __LINE__), GL_NO_ERROR);
}

//
// ResolveMSAA
//
// Create MSAA and resolve textures, attach to framebuffer, clear, and resolve.
//
TEST_F(MSAATextureAttachmentOGLTest, ResolveMSAA) {
  if (!iglDev_->hasFeature(DeviceFeatures::MultiSample)) {
    GTEST_SKIP() << "MultiSample not supported";
  }
  if (!iglDev_->hasFeature(DeviceFeatures::MultiSampleResolve)) {
    GTEST_SKIP() << "MultiSampleResolve not supported";
  }

  Result ret;

  // Create MSAA texture
  TextureDesc msaaTexDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Attachment);
  msaaTexDesc.numSamples = 4;

  auto msaaTexture = iglDev_->createTexture(msaaTexDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(msaaTexture, nullptr);

  // Create resolve texture
  TextureDesc resolveTexDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                  4,
                                                  4,
                                                  TextureDesc::TextureUsageBits::Sampled |
                                                      TextureDesc::TextureUsageBits::Attachment);
  auto resolveTexture = iglDev_->createTexture(resolveTexDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(resolveTexture, nullptr);

  // Create framebuffer with MSAA and resolve attachments
  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = msaaTexture;
  fbDesc.colorAttachments[0].resolveTexture = resolveTexture;

  auto framebuffer = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(framebuffer, nullptr);

  // Clear the framebuffer
  RenderPassDesc renderPass;
  renderPass.colorAttachments.resize(1);
  renderPass.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass.colorAttachments[0].storeAction = StoreAction::MsaaResolve;
  renderPass.colorAttachments[0].clearColor = {1.0, 0.0, 0.0, 1.0};

  CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  auto cmdEncoder = cmdBuf->createRenderCommandEncoder(renderPass, framebuffer);
  ASSERT_NE(cmdEncoder, nullptr);
  cmdEncoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);

  ASSERT_EQ(context_->checkForErrors(__FILE__, __LINE__), GL_NO_ERROR);
}

} // namespace igl::tests
