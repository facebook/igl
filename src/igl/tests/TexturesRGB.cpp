/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "data/ShaderData.h"
#include "data/TextureData.h"
#include "data/VertexIndexData.h"
#include "util/Color.h"
#include "util/Common.h"
#include "util/TestDevice.h"

#include <gtest/gtest.h>
#include <igl/NameHandle.h>
#include <string>

namespace igl::tests {

//
// TexturesRGBBaseTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class TexturesRGBBaseTest : public ::testing::Test {
 private:
 public:
  TexturesRGBBaseTest() = default;
  ~TexturesRGBBaseTest() override = default;

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
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_SRGB,
                                                   offscreenTexWidth,
                                                   offscreenTexHeight,
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

    // Initialize Graphics Pipeline Descriptor, but leave the creation
    // to the individual tests in case further customization is required
    renderPipelineDesc_.vertexInputState = vertexInputState_;
    renderPipelineDesc_.shaderStages = shaderStages_;
    renderPipelineDesc_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc_.targetDesc.colorAttachments[0].textureFormat =
        offscreenTexture_->getFormat();
    renderPipelineDesc_.fragmentUnitSamplerMap[textureUnit_] =
        IGL_NAMEHANDLE(data::shader::simpleSampler);
    renderPipelineDesc_.cullMode = igl::CullMode::Disabled;

    if (iglDev_->getBackendType() == BackendType::OpenGL) {
      kTolerance = 1; // OpenGL is not accurate enough.
    }
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
  std::shared_ptr<ITexture> inputTexture_;

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> vb_, uv_, ib_;

  std::shared_ptr<ISamplerState> samp_;

  RenderPipelineDesc renderPipelineDesc_;
  size_t textureUnit_ = 0;

  // Picking this just to match the texture we will use. If you use a different
  // size texture, then you will have to either create a new offscreenTexture_
  // and the framebuffer object in your test, so know exactly what the end result
  // would be after sampling
  size_t offscreenTexWidth = 2;
  size_t offscreenTexHeight = 2;

  uint8_t kTolerance = 0; // some platforms aren't perfect and need some tolerance
};

class TexturesRGBSmallTest : public TexturesRGBBaseTest {
  void SetUp() override {
    TexturesRGBBaseTest::SetUp();
  }
};

class TexturesRGBBigTest : public TexturesRGBBaseTest {
  void SetUp() override {
    offscreenTexWidth = 4096;
    offscreenTexHeight = 4096;
    TexturesRGBBaseTest::SetUp();
  }
};
//
// isSRGB test
//
// This test checks whether the texture format can be detected as sRGB or not
//
TEST_F(TexturesRGBSmallTest, TextureisSRGB) {
  Result ret;
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_SRGB,
                                                 offscreenTexWidth,
                                                 offscreenTexHeight,
                                                 TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(inputTexture_->getProperties().isSRGB());

  const TextureDesc texDesc2 = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                  offscreenTexWidth,
                                                  offscreenTexHeight,
                                                  TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_FALSE(!inputTexture_->getProperties().isSRGB());
}

//
// TexturesRGB Passthrough Test
//
// This test uses a simple shader to copy the input texture to a same
// sized output texture (offscreenTexture_) and make sure colors are being preserved
//
TEST_F(TexturesRGBSmallTest, Passthrough) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_SRGB,
                                                 offscreenTexWidth,
                                                 offscreenTexHeight,
                                                 TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, offscreenTexWidth, offscreenTexHeight);

  inputTexture_->upload(rangeDesc, data::texture::TEX_RGBA_2x2);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  //-------
  // Render
  //-------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(6);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------
  // Read back framebuffer
  //----------------------
  auto pixels = std::vector<uint32_t>(offscreenTexWidth * offscreenTexHeight);

  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);

  //--------------------------------
  // Verify against original texture
  //--------------------------------
  for (size_t i = 0; i < offscreenTexWidth * offscreenTexHeight; i++) {
    const util::sRGBColor currentColor(pixels[i]);
    const util::sRGBColor testColor(data::texture::TEX_RGBA_2x2[i]);
    ASSERT_LE(abs(currentColor.r - testColor.r), kTolerance);
    ASSERT_LE(abs(currentColor.g - testColor.g), kTolerance);
    ASSERT_LE(abs(currentColor.b - testColor.b), kTolerance);
    ASSERT_LE(abs(currentColor.a - testColor.a), kTolerance);
  }
}

TEST_F(TexturesRGBBigTest, Passthrough) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_SRGB,
                                                 offscreenTexWidth,
                                                 offscreenTexHeight,
                                                 TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, offscreenTexWidth, offscreenTexHeight);

  const size_t kAllColorsCount = 256ul * 256ul * 256ul;
  ASSERT_TRUE(kAllColorsCount == offscreenTexWidth * offscreenTexHeight);
  std::vector<uint32_t> allColorsBuffer(offscreenTexWidth * offscreenTexHeight);
  size_t index = 0;
  for (size_t r = 0; r < 256ul; ++r) {
    for (size_t g = 0; g < 256ul; ++g) {
      for (size_t b = 0; b < 256ul; ++b) {
        ASSERT_TRUE(index < kAllColorsCount);
        allColorsBuffer[index] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
        ++index;
      }
    }
  }
  inputTexture_->upload(rangeDesc, allColorsBuffer.data());

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  //-------
  // Render
  //-------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(6);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------
  // Read back framebuffer
  //----------------------
  auto pixels = std::vector<uint32_t>(offscreenTexWidth * offscreenTexHeight);

  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);

  //--------------------------------
  // Verify against original texture
  //--------------------------------
  for (size_t i = 0; i < offscreenTexWidth * offscreenTexHeight; i++) {
    const util::sRGBColor currentColor(pixels[i]);
    const util::sRGBColor testColor(allColorsBuffer[i]);
    ASSERT_LE(abs(currentColor.r - testColor.r), kTolerance);
    ASSERT_LE(abs(currentColor.g - testColor.g), kTolerance);
    ASSERT_LE(abs(currentColor.b - testColor.b), kTolerance);
    ASSERT_LE(abs(currentColor.a - testColor.a), kTolerance);
  }
}

} // namespace igl::tests
