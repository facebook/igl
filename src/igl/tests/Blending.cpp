/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "data/ShaderData.h"
#include "data/TextureData.h"
#include "data/VertexIndexData.h"
#include "igl/Texture.h"
#include "util/Common.h"

#include <gtest/gtest.h>
#include <igl/CommandBuffer.h>
#include <igl/Framebuffer.h>
#include <igl/NameHandle.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/VertexInputState.h>
#include <string>

namespace igl::tests {

// The test will render to a texture with two overlapping quads.
// Each test will use different blending operations
#define OFFSCREEN_TEX_WIDTH 4ul
#define OFFSCREEN_TEX_HEIGHT 4ul

//
// BlendingTest
//
class BlendingTest : public ::testing::Test {
 private:
 public:
  BlendingTest() = default;
  ~BlendingTest() override = default;

  //
  // SetUp()
  //
  // This function sets up a render pass and a graphics pipeline descriptor
  // so it is ready to render a simple quad with an input texture to an
  // offscreen texture.
  //
  // The actual creation of the graphics pipeline state object is left
  // to each test so that tests can replace the default settings with
  // something more appropriate.
  //
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    // Create an offscreen texture to render to
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   OFFSCREEN_TEX_WIDTH,
                                                   OFFSCREEN_TEX_HEIGHT,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment);

    Result ret;
    offscreenTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(offscreenTexture_ != nullptr);

    // Create framebuffer using the offscreen texture
    FramebufferDesc framebufferDesc;

    framebufferDesc.colorAttachments[0].texture = offscreenTexture_;
    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(framebuffer_ != nullptr);

    // Initialize render pass descriptor.
    // Framebuffer is completely cleared (including alpha value)
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 0.0};

    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    igl::tests::util::createSimpleShaderStages(iglDev_, stages);
    shaderStages_ = std::move(stages);

    // Initialize input to vertex shader
    VertexInputStateDesc inputDesc;

    inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
    inputDesc.attributes[0].offset = 0;
    inputDesc.attributes[0].bufferIndex = data::shader::simplePosIndex;
    inputDesc.attributes[0].name = data::shader::simplePos;
    inputDesc.attributes[0].location = 0;
    inputDesc.inputBindings[0].stride = sizeof(float) * 4;

    inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
    inputDesc.attributes[1].offset = 0;
    inputDesc.attributes[1].bufferIndex = data::shader::simpleUvIndex;
    inputDesc.attributes[1].name = data::shader::simpleUv;
    inputDesc.attributes[1].location = 1;
    inputDesc.inputBindings[1].stride = sizeof(float) * 2;

    // numAttributes has to equal to bindings when using more than 1 buffer
    inputDesc.numAttributes = inputDesc.numInputBindings = 2;

    vertexInputState_ = iglDev_->createVertexInputState(inputDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vertexInputState_ != nullptr);

    // Initialize index buffer
    BufferDesc bufDesc;

    bufDesc.type = BufferDesc::BufferTypeBits::Index;
    bufDesc.data = data::vertex_index::QUAD_IND;
    bufDesc.length = sizeof(data::vertex_index::QUAD_IND);

    ib_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(ib_ != nullptr);

    // Initialize vertex and sampler buffers
    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = data::vertex_index::QUAD_VERT;
    bufDesc.length = sizeof(data::vertex_index::QUAD_VERT);

    vb_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vb_ != nullptr);

    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = data::vertex_index::QUAD_UV;
    bufDesc.length = sizeof(data::vertex_index::QUAD_UV);

    uv_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(uv_ != nullptr);

    // Initialize sampler state
    const SamplerStateDesc samplerDesc;
    samp_ = iglDev_->createSamplerState(samplerDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(samp_ != nullptr);

    // Create input textures
    const TextureDesc inputTexDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                        OFFSCREEN_TEX_WIDTH,
                                                        OFFSCREEN_TEX_HEIGHT,
                                                        TextureDesc::TextureUsageBits::Sampled);
    inputTexture1_ = iglDev_->createTexture(texDesc, &ret);
    inputTexture2_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(inputTexture1_ != nullptr && inputTexture2_ != nullptr);

    rangeDesc_ = TextureRangeDesc::new2D(0, 0, OFFSCREEN_TEX_WIDTH, OFFSCREEN_TEX_HEIGHT);

    inputTexture1_->upload(rangeDesc_, data::texture::TEX_RGBA_RED_ALPHA_128_4x4);
    inputTexture2_->upload(rangeDesc_, data::texture::TEX_RGBA_BLUE_ALPHA_127_4x4);

    // Initialize Graphics Pipeline Descriptors, but leave the creation
    // to the individual tests in case further customization is required
    renderPipelineDesc_.vertexInputState = vertexInputState_;
    renderPipelineDesc_.shaderStages = shaderStages_;
    renderPipelineDesc_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc_.targetDesc.colorAttachments[0].textureFormat =
        offscreenTexture_->getFormat();
    renderPipelineDesc_.fragmentUnitSamplerMap[textureUnit_] =
        IGL_NAMEHANDLE(data::shader::simpleSampler);
    renderPipelineDesc_.cullMode = igl::CullMode::Disabled;
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> cmdBuf_;
  CommandBufferDesc cbDesc_ = {};
  const std::string backend_ = IGL_BACKEND_TYPE;

  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;

  // Currently it is left to individual tests to initialize this
  std::shared_ptr<ITexture> inputTexture1_;
  std::shared_ptr<ITexture> inputTexture2_;
  TextureDesc texDesc_;
  TextureRangeDesc rangeDesc_;

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> vb_, uv_, ib_;

  std::shared_ptr<ISamplerState> samp_;

  RenderPipelineDesc renderPipelineDesc_;
  size_t textureUnit_ = 0;
};

//
// Blending
//
// This test adds the color and alpha of two textures
//
TEST_F(BlendingTest, RGBASrcAndDstAddTest) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState1, pipelineState2;

  //-----------------
  // Create Pipelines
  //-----------------
  renderPipelineDesc_.targetDesc.colorAttachments[0].blendEnabled = true;
  renderPipelineDesc_.targetDesc.colorAttachments[0].rgbBlendOp = BlendOp::Add;
  renderPipelineDesc_.targetDesc.colorAttachments[0].alphaBlendOp = BlendOp::Add;
  renderPipelineDesc_.targetDesc.colorAttachments[0].srcRGBBlendFactor = BlendFactor::One;
  renderPipelineDesc_.targetDesc.colorAttachments[0].srcAlphaBlendFactor = BlendFactor::One;
  renderPipelineDesc_.targetDesc.colorAttachments[0].dstRGBBlendFactor = BlendFactor::One;
  renderPipelineDesc_.targetDesc.colorAttachments[0].dstAlphaBlendFactor = BlendFactor::One;
  pipelineState1 = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState1 != nullptr);

  //-------
  // Render
  //-------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  // Bind pipeline
  cmds->bindRenderPipelineState(pipelineState1);

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);

  // Draw half red texture
  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture1_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());
  cmds->drawIndexed(6);

  // Draw half blue texture
  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture2_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());
  cmds->drawIndexed(6);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------
  // Read back framebuffer
  //----------------------
  auto pixels = std::vector<uint32_t>(OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT);

  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc_);

  //--------------------------------
  // Verify that addition worked
  // R = 0x80 (128)
  // B = 0x7F (127)
  // A = 0xFF (255)
  //--------------------------------
  const unsigned int finalPixel = 0x80007FFF;
  for (size_t i = 0; i < OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT; i++) {
    ASSERT_EQ(pixels[i], finalPixel);
  }
}

} // namespace igl::tests
