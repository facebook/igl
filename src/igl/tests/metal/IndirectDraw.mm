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

#define INDIRECT_TEX_WIDTH 8
#define INDIRECT_TEX_HEIGHT 8

//
// MetalIndirectDrawTest
//
// This test covers indirect draw operations on Metal.
//
class MetalIndirectDrawTest : public ::testing::Test {
 public:
  MetalIndirectDrawTest() = default;
  ~MetalIndirectDrawTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// MultiDrawIndirect
//
// Test performing an indirect draw with drawCount=1.
//
TEST_F(MetalIndirectDrawTest, MultiDrawIndirect) {
  Result res;

  // Create render target
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           INDIRECT_TEX_WIDTH,
                                           INDIRECT_TEX_HEIGHT,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  auto colorTexture = device_->createTexture(texDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = colorTexture;
  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create shader stages and pipeline
  std::unique_ptr<IShaderStages> shaderStages;
  util::createSimpleShaderStages(device_, shaderStages);
  ASSERT_NE(shaderStages, nullptr);

  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(shaderStages);
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
  auto pipeline = device_->createRenderPipeline(pipelineDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create vertex buffer
  const float vertexData[] = {
      0.0f,
      0.5f,
      0.0f,
      1.0f,
      -0.5f,
      -0.5f,
      0.0f,
      1.0f,
      0.5f,
      -0.5f,
      0.0f,
      1.0f,
  };
  BufferDesc vbDesc(
      BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData), ResourceStorage::Shared);
  auto vertexBuffer = device_->createBuffer(vbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create UV buffer
  const float uvData[] = {
      0.5f,
      0.0f,
      0.0f,
      1.0f,
      1.0f,
      1.0f,
  };
  BufferDesc uvBufDesc(
      BufferDesc::BufferTypeBits::Vertex, uvData, sizeof(uvData), ResourceStorage::Shared);
  auto uvBuffer = device_->createBuffer(uvBufDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create indirect buffer
  // MTLDrawPrimitivesIndirectArguments: vertexCount, instanceCount, vertexStart, baseInstance
  struct DrawIndirectArgs {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t vertexStart;
    uint32_t baseInstance;
  };
  DrawIndirectArgs args = {3, 1, 0, 0};
  BufferDesc indirectDesc(
      BufferDesc::BufferTypeBits::Indirect, &args, sizeof(args), ResourceStorage::Shared);
  auto indirectBuffer = device_->createBuffer(indirectDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  const CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, framebuffer);
  ASSERT_NE(encoder, nullptr);

  encoder->bindRenderPipelineState(pipeline);
  encoder->bindVertexBuffer(0, *vertexBuffer);
  encoder->bindVertexBuffer(1, *uvBuffer);
  encoder->multiDrawIndirect(*indirectBuffer, 0, 1);
  encoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

} // namespace igl::tests
