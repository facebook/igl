/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../data/ShaderData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/opengl/DepthStencilState.h>
#include <string>

namespace igl::tests {

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
#define OFFSCREEN_TEX_WIDTH 2
#define OFFSCREEN_TEX_HEIGHT 2

//
// DepthStencilStateTest
//
// Tests functions within DepthStencilState module.
//
class DepthStencilStateTest : public ::testing::Test {
 public:
  DepthStencilStateTest() = default;
  ~DepthStencilStateTest() override = default;

  // Set up common resources.
  // This function sets up a render pass and a graphics pipeline descriptor
  // so it is ready to render a simple quad with an input texture to an
  // offscreen texture.
  void SetUp() override {
    igl::setDebugBreakEnabled(false); // only use in debug mode
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    Result ret;

    // Create an offscreen texture to render to
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   OFFSCREEN_TEX_WIDTH,
                                                   OFFSCREEN_TEX_HEIGHT,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment);

    offscreenTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(offscreenTexture_ != nullptr);

    // Create framebuffer using the offscreen texture
    FramebufferDesc framebufferDesc;

    framebufferDesc.colorAttachments[0].texture = offscreenTexture_;
    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(framebuffer_ != nullptr);

    // Initialize render pass descriptor
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

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
    ASSERT_TRUE(ret.isOk());
    ASSERT_TRUE(vertexInputState_ != nullptr);

    // Initialize index buffer
    BufferDesc bufDesc;

    bufDesc.type = BufferDesc::BufferTypeBits::Index;
    bufDesc.data = data::vertex_index::QUAD_IND;
    bufDesc.length = sizeof(data::vertex_index::QUAD_IND);

    ib_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk());
    ASSERT_TRUE(ib_ != nullptr);

    // Initialize Render Pipeline Descriptor, but leave the creation
    // to the individual tests in case further customization is required
    renderPipelineDesc_.vertexInputState = vertexInputState_;
    renderPipelineDesc_.shaderStages = shaderStages_;
    renderPipelineDesc_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc_.targetDesc.colorAttachments[0].textureFormat =
        offscreenTexture_->getFormat();
    renderPipelineDesc_.cullMode = igl::CullMode::Disabled;
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> cmdBuf_;
  std::shared_ptr<ICommandBuffer> cmdEncoder_;
  CommandBufferDesc cbDesc_ = {};

  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> ib_;

  RenderPipelineDesc renderPipelineDesc_;
};

//
// Passthrough Test
//
// Check binding of Depth Stencil State is successful.
//
TEST_F(DepthStencilStateTest, Passthrough) {
  Result ret;
  std::shared_ptr<IDepthStencilState> idss;

  const DepthStencilStateDesc dsDesc;
  idss = iglDev_->createDepthStencilState(dsDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(idss != nullptr);

  // Test initialization of DepthStencilState in CommandEncoder
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);
  auto cmdEncoder = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  // No asserts, just test the passthroughs are successful
  cmdEncoder->bindDepthStencilState(idss);
  auto dss = std::static_pointer_cast<igl::opengl::DepthStencilState>(idss);
  dss->bind(); // Test bind passthrough
  dss->unbind();
}

//
// Compare Function to OGL Test
//
// Check expected outputs for igl::opengl::DepthStencilState::convertCompareFunction
//
TEST_F(DepthStencilStateTest, CompareFunctionToOGL) {
  struct CompareFuncConversion {
    igl::CompareFunction igl = igl::CompareFunction::Never;
    GLenum ogl = GL_NEVER;
  };

  const std::vector<CompareFuncConversion> conversions{
      CompareFuncConversion{igl::CompareFunction::Never, GL_NEVER},
      CompareFuncConversion{igl::CompareFunction::Less, GL_LESS},
      CompareFuncConversion{igl::CompareFunction::Equal, GL_EQUAL},
      CompareFuncConversion{igl::CompareFunction::LessEqual, GL_LEQUAL},
      CompareFuncConversion{igl::CompareFunction::Greater, GL_GREATER},
      CompareFuncConversion{igl::CompareFunction::NotEqual, GL_NOTEQUAL},
      CompareFuncConversion{igl::CompareFunction::GreaterEqual, GL_GEQUAL},
      CompareFuncConversion{igl::CompareFunction::AlwaysPass, GL_ALWAYS},
  };

  for (auto data : conversions) {
    const GLenum ogl = igl::opengl::DepthStencilState::convertCompareFunction(data.igl);
    ASSERT_EQ(ogl, data.ogl);
  }
}

//
// Stencil Operation to OGL Test
//
// Check expected outputs for igl::opengl::DepthStencilState::convertStencilOperation
//
TEST_F(DepthStencilStateTest, StencilOperationToOGL) {
  struct StencilOpConversion {
    igl::StencilOperation igl = igl::StencilOperation::Keep;
    GLenum ogl = GL_KEEP;
  };

  const std::vector<StencilOpConversion> conversions{
      StencilOpConversion{igl::StencilOperation::Keep, GL_KEEP},
      StencilOpConversion{igl::StencilOperation::Zero, GL_ZERO},
      StencilOpConversion{igl::StencilOperation::Replace, GL_REPLACE},
      StencilOpConversion{igl::StencilOperation::IncrementClamp, GL_INCR},
      StencilOpConversion{igl::StencilOperation::DecrementClamp, GL_DECR},
      StencilOpConversion{igl::StencilOperation::Invert, GL_INVERT},
      StencilOpConversion{igl::StencilOperation::IncrementWrap, GL_INCR_WRAP},
      StencilOpConversion{igl::StencilOperation::DecrementWrap, GL_DECR_WRAP},
  };

  for (auto data : conversions) {
    const GLenum ogl = igl::opengl::DepthStencilState::convertStencilOperation(data.igl);
    ASSERT_EQ(ogl, data.ogl);
  }
}

//
// Set Stencil reference value and Check back Test
//
TEST_F(DepthStencilStateTest, SetStencilReferenceValueAndCheck) {
  Result ret;
  std::shared_ptr<IDepthStencilState> idss;

  DepthStencilStateDesc dsDesc;
  dsDesc.isDepthWriteEnabled = true;

  idss = iglDev_->createDepthStencilState(dsDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(idss != nullptr);

  std::shared_ptr<IRenderPipelineState> pipelineState;

  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(pipelineState != nullptr);

  // Test initialization of DepthStencilState in CommandEncoder
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);
  auto cmdEncoder = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);

  //------------------------------------------
  // First Read the Default Values
  //------------------------------------------

  // Dummy draw just to force binding of the states
  cmdEncoder->bindRenderPipelineState(pipelineState);
  cmdEncoder->bindDepthStencilState(idss);
  cmdEncoder->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmdEncoder->drawIndexed(0);
  cmdEncoder->endEncoding();
  cmdQueue_->submit(*cmdBuf_);

  // Read back default reference value here
  opengl::IContext* ctx;
  ctx = &static_cast<igl::opengl::Device&>(*iglDev_).getContext();
  GLboolean origIsDepthWriteEnabled, newIsDepthWriteEnabled;
  GLint origDepthFunVal, newDepthFunVal;
  GLint origFrontCompareFunc, newFrontCompareFunc;
  GLint origBackCompareFunc, newBackCompareFunc;

  GLint origStencilBackFail, newStencilBackFail;
  GLint origStencilFail, newStencilFail;
  GLint origStencilPassDepthFail, newStencilPassDepthFail;
  GLint origStencilPassDepthPass, newStencilPassDepthPass;

  GLint origStencilBackWriteMask, newStencilBackWriteMask;
  GLint origStencilWriteMask, newStencilWriteMask;

  ctx->getBooleanv(GL_DEPTH_WRITEMASK, &origIsDepthWriteEnabled);
  ctx->getIntegerv(GL_DEPTH_FUNC, &origDepthFunVal);

  ASSERT_TRUE((bool)origIsDepthWriteEnabled == dsDesc.isDepthWriteEnabled);
  ASSERT_TRUE(origDepthFunVal ==
              opengl::DepthStencilState::convertCompareFunction(dsDesc.compareFunction));

  // Read Front Compare Func
  ctx->getIntegerv(GL_STENCIL_FUNC, &origFrontCompareFunc);
  // Read Back Compare Func
  ctx->getIntegerv(GL_STENCIL_BACK_FUNC, &origBackCompareFunc);

  // Read GL_STENCIL_BACK_FAIL (initial value = GL_KEEP)
  ctx->getIntegerv(GL_STENCIL_BACK_FAIL, &origStencilBackFail);
  // Read GL_STENCIL_FAIL (initial value = GL_KEEP)
  ctx->getIntegerv(GL_STENCIL_FAIL, &origStencilFail);
  // Read GL_STENCIL_PASS_DEPTH_FAIL (initial value = GL_KEEP)
  ctx->getIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &origStencilPassDepthFail);
  // Read GL_STENCIL_PASS_DEPTH_PASS (initial value = GL_KEEP)
  ctx->getIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &origStencilPassDepthPass);
  // Read GL_STENCIL_BACK_WRITEMASK (initial value = 0xff)
  ctx->getIntegerv(GL_STENCIL_BACK_WRITEMASK, &origStencilBackWriteMask);
  // Read GL_STENCIL_WRITEMASK (initial value = 0xff)
  ctx->getIntegerv(GL_STENCIL_WRITEMASK, &origStencilWriteMask);

  //-------------------------------------------------------
  // Try Setting Stencil Reference values
  //-------------------------------------------------------
  // Create a new DepthStencilDesc
  DepthStencilStateDesc newDsDesc;
  newDsDesc.isDepthWriteEnabled = true;
  newDsDesc.compareFunction = CompareFunction::Greater;
  newDsDesc.frontFaceStencil.stencilCompareFunction = CompareFunction::Greater;
  newDsDesc.backFaceStencil.stencilCompareFunction = CompareFunction::Greater;

  newDsDesc.backFaceStencil.stencilFailureOperation = StencilOperation::DecrementClamp;
  newDsDesc.backFaceStencil.depthFailureOperation = StencilOperation::Invert;
  newDsDesc.backFaceStencil.depthStencilPassOperation = StencilOperation::IncrementWrap;

  newDsDesc.frontFaceStencil.stencilFailureOperation = StencilOperation::DecrementClamp;
  newDsDesc.frontFaceStencil.depthFailureOperation = StencilOperation::Invert;
  newDsDesc.frontFaceStencil.depthStencilPassOperation = StencilOperation::IncrementWrap;

  // GLES stencil is limited to 8 bits (0xFF), at least on Adreno GPUs
  if (opengl::DeviceFeatureSet::usesOpenGLES()) {
    newDsDesc.backFaceStencil.writeMask = 0xad;
    newDsDesc.frontFaceStencil.writeMask = 0xef;
  } else {
    newDsDesc.backFaceStencil.writeMask = 0xdead;
    newDsDesc.frontFaceStencil.writeMask = 0xbeef;
  }

  // Create the Depth Stencil State from the new Depth Stencil Descriptor
  idss = iglDev_->createDepthStencilState(newDsDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(idss != nullptr);

  ctx->enable(GL_DEPTH_TEST);
  ctx->depthFunc(opengl::DepthStencilState::convertCompareFunction(newDsDesc.compareFunction));
  const GLuint mask{0xff};
  ctx->stencilFuncSeparate(
      GL_FRONT,
      opengl::DepthStencilState::convertCompareFunction(newDsDesc.compareFunction),
      0xaa,
      mask);
  ctx->stencilFuncSeparate(
      GL_BACK,
      opengl::DepthStencilState::convertCompareFunction(newDsDesc.compareFunction),
      0xbb,
      mask);
  GLenum sfail = opengl::DepthStencilState::convertStencilOperation(
      newDsDesc.backFaceStencil.stencilFailureOperation);
  GLenum dpfail = opengl::DepthStencilState::convertStencilOperation(
      newDsDesc.backFaceStencil.depthFailureOperation);
  GLenum dppass = opengl::DepthStencilState::convertStencilOperation(
      newDsDesc.backFaceStencil.depthStencilPassOperation);
  ctx->stencilOpSeparate(GL_BACK, sfail, dpfail, dppass);

  sfail = opengl::DepthStencilState::convertStencilOperation(
      newDsDesc.frontFaceStencil.stencilFailureOperation);
  dpfail = opengl::DepthStencilState::convertStencilOperation(
      newDsDesc.frontFaceStencil.depthFailureOperation);
  dppass = opengl::DepthStencilState::convertStencilOperation(
      newDsDesc.frontFaceStencil.depthStencilPassOperation);
  ctx->stencilOpSeparate(GL_FRONT, sfail, dpfail, dppass);

  ctx->stencilMaskSeparate(GL_BACK, newDsDesc.backFaceStencil.writeMask);
  ctx->stencilMaskSeparate(GL_FRONT, newDsDesc.frontFaceStencil.writeMask);

  cmdEncoder = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);

  // Dummy draw just to force binding of the states
  cmdEncoder->bindRenderPipelineState(pipelineState);
  cmdEncoder->bindDepthStencilState(idss);

  cmdEncoder->setStencilReferenceValue(1);
  cmdEncoder->setStencilReferenceValues(2, 3);

  cmdEncoder->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmdEncoder->drawIndexed(0);
  cmdEncoder->endEncoding();
  cmdQueue_->submit(*cmdBuf_);

  // Read back reference value here
  // Read Front Front Compare Func
  ctx->getIntegerv(GL_STENCIL_FUNC, &newFrontCompareFunc);
  // Read Back Comapare Func
  ctx->getIntegerv(GL_STENCIL_BACK_FUNC, &newBackCompareFunc);
  // Read GL_STENCIL_BACK_FAIL (initial value = GL_KEEP)
  ctx->getIntegerv(GL_STENCIL_BACK_FAIL, &newStencilBackFail);
  // Read GL_STENCIL_FAIL (initial value = GL_KEEP)
  ctx->getIntegerv(GL_STENCIL_FAIL, &newStencilFail);
  // Read GL_STENCIL_PASS_DEPTH_FAIL (initial value = GL_KEEP)
  ctx->getIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &newStencilPassDepthFail);
  // Read GL_STENCIL_PASS_DEPTH_PASS (initial value = GL_KEEP)
  ctx->getIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &newStencilPassDepthPass);
  // Read GL_STENCIL_BACK_WRITEMASK (initial value = 0xff)
  ctx->getIntegerv(GL_STENCIL_BACK_WRITEMASK, &newStencilBackWriteMask);
  // Read GL_STENCIL_WRITEMASK (initial value = 0xff)
  ctx->getIntegerv(GL_STENCIL_WRITEMASK, &newStencilWriteMask);

  // Test the default values
  ASSERT_TRUE(GL_ALWAYS == origFrontCompareFunc);
  ASSERT_TRUE(GL_ALWAYS == origBackCompareFunc);
  ASSERT_TRUE(GL_KEEP == origStencilBackFail);
  ASSERT_TRUE(GL_KEEP == origStencilFail);
  ASSERT_TRUE(GL_KEEP == origStencilPassDepthFail);
  ASSERT_TRUE(GL_KEEP == origStencilPassDepthPass);

#if defined(IGL_ANGLE) && IGL_ANGLE
  // For unknown reasons ANGLE clamps masks to int type.
  ASSERT_TRUE(0x7fffffff == origStencilBackWriteMask);
  ASSERT_TRUE(0x7fffffff == origStencilWriteMask);
#else
  // GLES stencil mask is limited to 8 bits (0xFF), at least on Adreno GPUs
#if IGL_PLATFORM_ANDROID
  if (opengl::DeviceFeatureSet::usesOpenGLES()) {
    GLuint origExpectedStencilValue = 0xffffffff;

    // GLES stencil mask is limited to 8 bits (0xFF) on Adreno GPUs
    const GLubyte* renderer = ctx->getString(GL_RENDERER);
    if (strncmp((char*)renderer, "Adreno", 6) == 0) {
      origExpectedStencilValue = 0xff;
    }

    ASSERT_TRUE(origExpectedStencilValue == origStencilBackWriteMask);
    ASSERT_TRUE(origExpectedStencilValue == origStencilWriteMask);
  } else {
    ASSERT_TRUE(0xffffffff == origStencilBackWriteMask);
    ASSERT_TRUE(0xffffffff == origStencilWriteMask);
  }
#else
  ASSERT_TRUE(0xffffffff == origStencilBackWriteMask);
  ASSERT_TRUE(0xffffffff == origStencilWriteMask);
#endif
#endif
  // Test the newly set values
  ASSERT_TRUE(opengl::DepthStencilState::convertCompareFunction(
                  newDsDesc.frontFaceStencil.stencilCompareFunction) == newFrontCompareFunc);
  ASSERT_TRUE(opengl::DepthStencilState::convertCompareFunction(
                  newDsDesc.backFaceStencil.stencilCompareFunction) == newBackCompareFunc);
  ASSERT_TRUE(opengl::DepthStencilState::convertStencilOperation(
                  newDsDesc.backFaceStencil.stencilFailureOperation) == newStencilBackFail);
  ASSERT_TRUE(opengl::DepthStencilState::convertStencilOperation(
                  newDsDesc.frontFaceStencil.stencilFailureOperation) == newStencilFail);
  ASSERT_TRUE(opengl::DepthStencilState::convertStencilOperation(
                  newDsDesc.frontFaceStencil.depthFailureOperation) == newStencilPassDepthFail);
  ASSERT_TRUE(opengl::DepthStencilState::convertStencilOperation(
                  newDsDesc.frontFaceStencil.depthStencilPassOperation) == newStencilPassDepthPass);
  ASSERT_TRUE(newDsDesc.backFaceStencil.writeMask == newStencilBackWriteMask);
  ASSERT_TRUE(newDsDesc.frontFaceStencil.writeMask == newStencilWriteMask);

  ctx->getBooleanv(GL_DEPTH_WRITEMASK, &newIsDepthWriteEnabled);
  ASSERT_TRUE((bool)newIsDepthWriteEnabled == newDsDesc.isDepthWriteEnabled);

  ctx->getIntegerv(GL_DEPTH_FUNC, &newDepthFunVal);
  ASSERT_TRUE(newDepthFunVal ==
              opengl::DepthStencilState::convertCompareFunction(newDsDesc.compareFunction));

  ASSERT_TRUE(dsDesc != newDsDesc);
  ASSERT_TRUE(newDsDesc.backFaceStencil != newDsDesc.frontFaceStencil);
}

} // namespace igl::tests
