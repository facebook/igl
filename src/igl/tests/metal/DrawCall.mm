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

#define DRAW_TEX_WIDTH 8
#define DRAW_TEX_HEIGHT 8

//
// MetalDrawCallTest
//
// This test covers draw call operations on Metal.
// Tests drawing vertices, indexed drawing, and draw count tracking.
//
class MetalDrawCallTest : public ::testing::Test {
 public:
  MetalDrawCallTest() = default;
  ~MetalDrawCallTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);

    Result res;

    // Create render target texture
    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             DRAW_TEX_WIDTH,
                                             DRAW_TEX_HEIGHT,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
    colorTexture_ = device_->createTexture(texDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;

    // Create framebuffer
    FramebufferDesc fbDesc;
    fbDesc.colorAttachments[0].texture = colorTexture_;
    framebuffer_ = device_->createFramebuffer(fbDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;

    // Create shader stages
    util::createSimpleShaderStages(device_, shaderStages_);
    ASSERT_NE(shaderStages_, nullptr);

    // Create render pipeline
    RenderPipelineDesc pipelineDesc;
    pipelineDesc.shaderStages = std::move(shaderStages_);
    pipelineDesc.targetDesc.colorAttachments.resize(1);
    pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
    pipelineDesc.debugName = genNameHandle("drawTestPipeline");

    renderPipeline_ = device_->createRenderPipeline(pipelineDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;

    // Create vertex buffer with triangle data
    // clang-format off
    const float vertexData[] = {
        // position (x, y, z, w)
         0.0f,  0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f,
    };
    // clang-format on

    BufferDesc vbDesc(BufferDesc::BufferTypeBits::Vertex,
                      vertexData,
                      sizeof(vertexData),
                      ResourceStorage::Shared);
    vertexBuffer_ = device_->createBuffer(vbDesc, &res);
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

    BufferDesc uvDesc(
        BufferDesc::BufferTypeBits::Vertex, uvData, sizeof(uvData), ResourceStorage::Shared);
    uvBuffer_ = device_->createBuffer(uvDesc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ITexture> colorTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::unique_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IRenderPipelineState> renderPipeline_;
  std::unique_ptr<IBuffer> vertexBuffer_;
  std::unique_ptr<IBuffer> uvBuffer_;
};

//
// DrawTriangle
//
// Draw 3 vertices to form a triangle without crashing.
//
TEST_F(MetalDrawCallTest, DrawTriangle) {
  Result res;
  const CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  encoder->bindRenderPipelineState(renderPipeline_);
  encoder->bindVertexBuffer(0, *vertexBuffer_);
  encoder->bindVertexBuffer(1, *uvBuffer_);
  encoder->draw(3);
  encoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

//
// DrawIndexed
//
// Draw with an index buffer without crashing.
//
TEST_F(MetalDrawCallTest, DrawIndexed) {
  Result res;

  // Create index buffer
  const uint16_t indices[] = {0, 1, 2};
  BufferDesc ibDesc(
      BufferDesc::BufferTypeBits::Index, indices, sizeof(indices), ResourceStorage::Shared);
  auto indexBuffer = device_->createBuffer(ibDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  const CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  encoder->bindRenderPipelineState(renderPipeline_);
  encoder->bindVertexBuffer(0, *vertexBuffer_);
  encoder->bindVertexBuffer(1, *uvBuffer_);
  encoder->bindIndexBuffer(*indexBuffer, IndexFormat::UInt16);
  encoder->drawIndexed(3);
  encoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

//
// DrawCountIncrements
//
// Verify that the command buffer draw count increments after a draw call.
//
TEST_F(MetalDrawCallTest, DrawCountIncrements) {
  Result res;
  const CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  ASSERT_EQ(cmdBuf->getCurrentDrawCount(), 0u);

  RenderPassDesc rpDesc;
  rpDesc.colorAttachments.resize(1);
  rpDesc.colorAttachments[0].loadAction = LoadAction::Clear;
  rpDesc.colorAttachments[0].storeAction = StoreAction::Store;

  auto encoder = cmdBuf->createRenderCommandEncoder(rpDesc, framebuffer_);
  ASSERT_NE(encoder, nullptr);

  encoder->bindRenderPipelineState(renderPipeline_);
  encoder->bindVertexBuffer(0, *vertexBuffer_);
  encoder->bindVertexBuffer(1, *uvBuffer_);
  encoder->draw(3);
  encoder->endEncoding();

  // The draw count should have been incremented
  ASSERT_GE(cmdBuf->getCurrentDrawCount(), 1u);
}

} // namespace igl::tests
