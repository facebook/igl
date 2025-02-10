/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "data/ShaderData.h"
#include "data/VertexIndexData.h"
#include "util/Common.h"
#include "util/TestDevice.h"

#include <string>

// Use a 1x1 Framebuffer for this test
#define OFFSCREEN_RT_WIDTH 1
#define OFFSCREEN_RT_HEIGHT 1

namespace igl::tests {

//
// DeviceTest
//
// This category of tests are meant for testing Device APIs that are not
// related to resource creation, e.g. ICapabilities, and getting device
// statics.
//
class DeviceTest : public ::testing::Test {
 public:
  DeviceTest() = default;
  ~DeviceTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);

    // Create an offscreen texture to render to
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   OFFSCREEN_RT_WIDTH,
                                                   OFFSCREEN_RT_HEIGHT,
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
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_TRUE(vertexInputState_ != nullptr);

    // Initialize index buffer
    BufferDesc bufDesc;

    bufDesc.type = BufferDesc::BufferTypeBits::Index;
    bufDesc.data = data::vertex_index::QUAD_IND;
    bufDesc.length = sizeof(data::vertex_index::QUAD_IND);

    ib_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
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

  CommandBufferDesc cbDesc_ = {};
  const std::string backend_ = IGL_BACKEND_TYPE;

  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> ib_;

  RenderPipelineDesc renderPipelineDesc_;
};

//
// Last Draw Statics
//
// Check and make sure getDrawCount() is working properly.
//
TEST_F(DeviceTest, LastDrawStat) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  size_t drawCount = iglDev_->getCurrentDrawCount();

  // Nothing has been drawn yet, so should be 0
  ASSERT_EQ(drawCount, 0);

  // Do a dummy draw
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(cmdBuf_ != nullptr);

  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(pipelineState != nullptr);

  renderPass_.colorAttachments[0].clearColor = {0, 0, 0, 0};

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindRenderPipelineState(pipelineState);
  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(0); // draw 0 indices
  cmds->endEncoding();
  cmdQueue_->submit(*cmdBuf_);

  // Check draw count again
  drawCount = iglDev_->getCurrentDrawCount();

  // After the dummy draw, this should be 1
  ASSERT_EQ(drawCount, 1);
}

//
// In Development Feature API Test
//
// Make sure what flags have the right defaults, and setter and getter work.
//
TEST_F(DeviceTest, InDevelopmentFeature) {
  // Set a flag
  iglDev_->setDevelopmentFlags(igl::InDevelopementFeatures::DummyFeatureExample, true);
  ASSERT_TRUE(iglDev_->testDevelopmentFlags(igl::InDevelopementFeatures::DummyFeatureExample) != 0);

  // Reset the flag
  iglDev_->setDevelopmentFlags(igl::InDevelopementFeatures::DummyFeatureExample, false);
  ASSERT_TRUE(iglDev_->testDevelopmentFlags(igl::InDevelopementFeatures::DummyFeatureExample) == 0);
}

//
// Get Backend Type
//
// Make sure IDevice->getBackendType() only returns expected values
//
TEST_F(DeviceTest, GetBackendType) {
  if (iglDev_->getBackendType() == igl::BackendType::Metal) {
    ASSERT_EQ(backend_, util::BACKEND_MTL);
  } else if (iglDev_->getBackendType() == igl::BackendType::OpenGL) {
    ASSERT_EQ(backend_, util::BACKEND_OGL);
  } else if (iglDev_->getBackendType() == igl::BackendType::Vulkan) {
    ASSERT_EQ(backend_, util::BACKEND_VUL);
  } else {
    // Unknow backend. Please add to this test.
    ASSERT_TRUE(0);
  }
}

} // namespace igl::tests
