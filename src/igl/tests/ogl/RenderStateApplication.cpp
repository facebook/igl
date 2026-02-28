/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../data/ShaderData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"

#include <igl/CommandBuffer.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/VertexInputState.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/RenderPipelineState.h>

namespace igl::tests {

#define OFFSCREEN_TEX_WIDTH 2
#define OFFSCREEN_TEX_HEIGHT 2

//
// RenderStateApplicationOGLTest
//
// Tests that render pipeline state settings are correctly applied to OpenGL state.
//
class RenderStateApplicationOGLTest : public ::testing::Test {
 public:
  RenderStateApplicationOGLTest() = default;
  ~RenderStateApplicationOGLTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);

    context_ = &static_cast<opengl::Device&>(*iglDev_).getContext();

    Result ret;

    // Create offscreen texture
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   OFFSCREEN_TEX_WIDTH,
                                                   OFFSCREEN_TEX_HEIGHT,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment);
    offscreenTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);

    // Create framebuffer
    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = offscreenTexture_;
    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);

    // Initialize render pass
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    igl::tests::util::createSimpleShaderStages(iglDev_, stages);
    shaderStages_ = std::move(stages);

    // Initialize vertex input state
    VertexInputStateDesc inputDesc;
    inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
    inputDesc.attributes[0].offset = 0;
    inputDesc.attributes[0].bufferIndex = data::shader::kSimplePosIndex;
    inputDesc.attributes[0].name = data::shader::kSimplePos;
    inputDesc.attributes[0].location = 0;
    inputDesc.inputBindings[0].stride = sizeof(float) * 4;

    inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
    inputDesc.attributes[1].offset = 0;
    inputDesc.attributes[1].bufferIndex = data::shader::kSimpleUvIndex;
    inputDesc.attributes[1].name = data::shader::kSimpleUv;
    inputDesc.attributes[1].location = 1;
    inputDesc.inputBindings[1].stride = sizeof(float) * 2;

    inputDesc.numAttributes = inputDesc.numInputBindings = 2;

    vertexInputState_ = iglDev_->createVertexInputState(inputDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

    // Create index buffer
    BufferDesc ibDesc;
    ibDesc.type = BufferDesc::BufferTypeBits::Index;
    ibDesc.data = data::vertex_index::kQuadInd.data();
    ibDesc.length = sizeof(data::vertex_index::kQuadInd);
    ib_ = iglDev_->createBuffer(ibDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  }

  void TearDown() override {}

  // Helper to create a pipeline with specific settings, bind it, and do a dummy draw
  void bindPipelineWithSettings(CullMode cullMode, WindingMode winding, PolygonFillMode fillMode) {
    RenderPipelineDesc desc;
    desc.vertexInputState = vertexInputState_;
    desc.shaderStages = shaderStages_;
    desc.targetDesc.colorAttachments.resize(1);
    desc.targetDesc.colorAttachments[0].textureFormat = offscreenTexture_->getFormat();
    desc.cullMode = cullMode;
    desc.frontFaceWinding = winding;
    desc.polygonFillMode = fillMode;

    Result ret;
    auto pipelineState = iglDev_->createRenderPipeline(desc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_NE(pipelineState, nullptr);

    CommandBufferDesc cbDesc;
    auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);

    auto cmdEncoder = cmdBuf->createRenderCommandEncoder(renderPass_, framebuffer_);
    cmdEncoder->bindRenderPipelineState(pipelineState);
    cmdEncoder->bindIndexBuffer(*ib_, IndexFormat::UInt16);
    cmdEncoder->drawIndexed(0);
    cmdEncoder->endEncoding();
    cmdQueue_->submit(*cmdBuf);
  }

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  opengl::IContext* context_ = nullptr;

  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::unique_ptr<IBuffer> ib_;
};

//
// CullModeBack
//
// Verify GL state after binding pipeline with CullMode::Back.
//
TEST_F(RenderStateApplicationOGLTest, CullModeBack) {
  bindPipelineWithSettings(CullMode::Back, WindingMode::CounterClockwise, PolygonFillMode::Fill);

  GLboolean cullEnabled = GL_FALSE;
  context_->getBooleanv(GL_CULL_FACE, &cullEnabled);
  ASSERT_EQ(cullEnabled, GL_TRUE);

  GLint cullFaceMode = 0;
  context_->getIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
  ASSERT_EQ(cullFaceMode, GL_BACK);
}

//
// CullModeFront
//
// Verify GL state after binding pipeline with CullMode::Front.
//
TEST_F(RenderStateApplicationOGLTest, CullModeFront) {
  bindPipelineWithSettings(CullMode::Front, WindingMode::CounterClockwise, PolygonFillMode::Fill);

  GLboolean cullEnabled = GL_FALSE;
  context_->getBooleanv(GL_CULL_FACE, &cullEnabled);
  ASSERT_EQ(cullEnabled, GL_TRUE);

  GLint cullFaceMode = 0;
  context_->getIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
  ASSERT_EQ(cullFaceMode, GL_FRONT);
}

//
// CullModeDisabled
//
// Verify GL state after binding pipeline with CullMode::Disabled.
//
TEST_F(RenderStateApplicationOGLTest, CullModeDisabled) {
  bindPipelineWithSettings(
      CullMode::Disabled, WindingMode::CounterClockwise, PolygonFillMode::Fill);

  GLboolean cullEnabled = GL_TRUE;
  context_->getBooleanv(GL_CULL_FACE, &cullEnabled);
  ASSERT_EQ(cullEnabled, GL_FALSE);
}

//
// WindingModeCCW
//
// Verify CounterClockwise winding mode.
//
TEST_F(RenderStateApplicationOGLTest, WindingModeCCW) {
  bindPipelineWithSettings(CullMode::Back, WindingMode::CounterClockwise, PolygonFillMode::Fill);

  GLint frontFace = 0;
  context_->getIntegerv(GL_FRONT_FACE, &frontFace);
  ASSERT_EQ(frontFace, GL_CCW);
}

//
// PolygonFillLine
//
// Verify PolygonFillMode::Line (only on desktop GL).
//
TEST_F(RenderStateApplicationOGLTest, PolygonFillLine) {
  if (!context_->deviceFeatures().hasInternalFeature(opengl::InternalFeatures::PolygonFillMode)) {
    GTEST_SKIP() << "PolygonFillMode not supported (likely OpenGL ES)";
  }

  bindPipelineWithSettings(
      CullMode::Disabled, WindingMode::CounterClockwise, PolygonFillMode::Line);

#if defined(GL_POLYGON_MODE)
  GLint polygonMode[2] = {0, 0};
  context_->getIntegerv(GL_POLYGON_MODE, polygonMode);
  ASSERT_EQ(polygonMode[0], GL_LINE);
#else
  // GL_POLYGON_MODE is not available on OpenGL ES; skip the assertion
  // but validate no GL errors occurred
  ASSERT_EQ(context_->checkForErrors(__FILE__, __LINE__), GL_NO_ERROR);
#endif
}

} // namespace igl::tests
