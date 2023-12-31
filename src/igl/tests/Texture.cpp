/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "data/ShaderData.h"
#include "data/TextureData.h"
#include "data/VertexIndexData.h"
#include "util/Common.h"
#include "util/TestDevice.h"

#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/NameHandle.h>
#include <igl/opengl/Device.h>
#include <string>

namespace igl {
namespace tests {

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
constexpr uint32_t kOffscreenTexWidth = 2u;
constexpr uint32_t kOffscreenTexHeight = 2u;

//
// TextureTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class TextureTest : public ::testing::Test {
 private:
 public:
  TextureTest() = default;
  ~TextureTest() override = default;

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
    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             kOffscreenTexWidth,
                                             kOffscreenTexHeight,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
    texDesc.debugName = "test";

    Result ret;
    offscreenTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
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
    SamplerStateDesc samplerDesc;
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
};

TEST(TextureRangeDesc, Construction) {
  {
    const auto range = TextureRangeDesc::new1D(2, 3, 4, 5);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 0);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 3);
    EXPECT_EQ(range.height, 1);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 4);
    EXPECT_EQ(range.numMipLevels, 5);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::new1DArray(2, 3, 4, 5, 6, 7);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 0);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 3);
    EXPECT_EQ(range.height, 1);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 4);
    EXPECT_EQ(range.numLayers, 5);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::new2DArray(2, 3, 4, 5, 6, 7, 8, 9);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 6);
    EXPECT_EQ(range.numLayers, 7);
    EXPECT_EQ(range.mipLevel, 8);
    EXPECT_EQ(range.numMipLevels, 9);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::new3D(2, 3, 4, 5, 6, 7, 8, 9);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 4);
    EXPECT_EQ(range.width, 5);
    EXPECT_EQ(range.height, 6);
    EXPECT_EQ(range.depth, 7);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 8);
    EXPECT_EQ(range.numMipLevels, 9);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::newCube(2, 3, 4, 5, 7, 8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 7);
    EXPECT_EQ(range.numMipLevels, 8);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 6);
  }
  {
    const auto range = TextureRangeDesc::newCubeFace(2, 3, 4, 5, 1, 7, 8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 7);
    EXPECT_EQ(range.numMipLevels, 8);
    EXPECT_EQ(range.face, 1);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto range = TextureRangeDesc::newCubeFace(2, 3, 4, 5, TextureCubeFace::NegX, 7, 8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 7);
    EXPECT_EQ(range.numMipLevels, 8);
    EXPECT_EQ(range.face, 1);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, AtMipLevel) {
  {
    const auto initialRange = TextureRangeDesc::new3D(0, 2, 5, 2, 10, 16, 0, 2);
    const auto range = initialRange.atMipLevel(0);
    EXPECT_EQ(range.x, 0);
    EXPECT_EQ(range.y, 2);
    EXPECT_EQ(range.z, 5);
    EXPECT_EQ(range.width, 2);
    EXPECT_EQ(range.height, 10);
    EXPECT_EQ(range.depth, 16);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 0);
    EXPECT_EQ(range.numMipLevels, 1);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto initialRange = TextureRangeDesc::new3D(0, 2, 5, 2, 10, 16, 0);
    const auto range = initialRange.atMipLevel(1);
    EXPECT_EQ(range.x, 0);
    EXPECT_EQ(range.y, 1);
    EXPECT_EQ(range.z, 2);
    EXPECT_EQ(range.width, 1);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 8);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 1);
    EXPECT_EQ(range.numMipLevels, 1);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto initialRange = TextureRangeDesc::new2DArray(0, 5, 2, 10, 0, 2, 1);
    const auto range = initialRange.atMipLevel(3);
    EXPECT_EQ(range.x, 0);
    EXPECT_EQ(range.y, 1);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 1);
    EXPECT_EQ(range.height, 2);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 2);
    EXPECT_EQ(range.mipLevel, 3);
    EXPECT_EQ(range.numMipLevels, 1);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, WithNumMipLevels) {
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.withNumMipLevels(8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 8);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, AtLayer) {
  {
    const auto initialRange = TextureRangeDesc::new2DArray(2, 3, 4, 5, 6, 7, 8);
    const auto range = initialRange.atLayer(1);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 1);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 8);
    EXPECT_EQ(range.numMipLevels, 1);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, WithNumLayers) {
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.withNumLayers(8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 8);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, AtFace) {
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.atFace(1);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 1);
    EXPECT_EQ(range.numFaces, 1);
  }
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.atFace(TextureCubeFace::NegX);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 1);
    EXPECT_EQ(range.numFaces, 1);
  }
}

TEST(TextureRangeDesc, WithNumFaces) {
  {
    const auto initialRange = TextureRangeDesc::new2D(2, 3, 4, 5, 6, 7);
    const auto range = initialRange.withNumFaces(8);
    EXPECT_EQ(range.x, 2);
    EXPECT_EQ(range.y, 3);
    EXPECT_EQ(range.z, 0);
    EXPECT_EQ(range.width, 4);
    EXPECT_EQ(range.height, 5);
    EXPECT_EQ(range.depth, 1);
    EXPECT_EQ(range.layer, 0);
    EXPECT_EQ(range.numLayers, 1);
    EXPECT_EQ(range.mipLevel, 6);
    EXPECT_EQ(range.numMipLevels, 7);
    EXPECT_EQ(range.face, 0);
    EXPECT_EQ(range.numFaces, 8);
  }
}

TEST(TextureRangeDesc, Validate) {
  constexpr size_t kMax = std::numeric_limits<uint32_t>::max();
  constexpr size_t kMaxPlus1 = std::numeric_limits<uint32_t>::max() + 1;
  // Overflow validation logic doesn't work on 32bit architectures
  constexpr bool kRunOverflowTests = (sizeof(size_t) > sizeof(uint32_t));

  // 2D
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, 1024, 1024).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, 1024, 1024, 0, 11).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, 1, kMax).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, kMax, 1).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2D(0, 0, 1024, 1024, kMax, 1).validate().isOk());

  if (kRunOverflowTests) {
    EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, kMax, 1024).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, 1024, kMax).validate().isOk());

    EXPECT_FALSE(TextureRangeDesc::new2D(1, 1, 1, kMax).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new2D(1, 1, kMax, 1).validate().isOk());

    EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, 1, kMaxPlus1).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, kMaxPlus1, 1).validate().isOk());
  }

  EXPECT_FALSE(TextureRangeDesc::new2D(0, 0, 1024, 1024, 0, 12).validate().isOk());

  // 2D Array
  EXPECT_TRUE(TextureRangeDesc::new2DArray(0, 0, 1024, 1024, 0, 1024).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new2DArray(0, 0, 1, 1, 0, kMax).validate().isOk());

  if (kRunOverflowTests) {
    EXPECT_FALSE(TextureRangeDesc::new2DArray(0, 0, 1024, 1024, 0, kMax).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new2DArray(0, 0, 1, 1, 1, kMax).validate().isOk());

    EXPECT_FALSE(TextureRangeDesc::new2DArray(0, 0, 1, 1, 0, kMaxPlus1).validate().isOk());
  }

  // 3D
  EXPECT_TRUE(TextureRangeDesc::new3D(0, 0, 0, 1024, 1024, 1024).validate().isOk());
  EXPECT_TRUE(TextureRangeDesc::new3D(0, 0, 0, 1, 1, kMax).validate().isOk());

  if (kRunOverflowTests) {
    EXPECT_FALSE(TextureRangeDesc::new3D(0, 0, 0, 1024, 1024, kMax).validate().isOk());
    EXPECT_FALSE(TextureRangeDesc::new3D(0, 0, 1, 1, 1, kMax).validate().isOk());

    EXPECT_FALSE(TextureRangeDesc::new3D(0, 0, 0, 1, 1, kMaxPlus1).validate().isOk());
  }

  // Cube
  EXPECT_TRUE(TextureRangeDesc::newCube(0, 0, 1024, 1024).validate().isOk());

  EXPECT_FALSE(TextureRangeDesc::newCube(0, 0, 1024, 1024).withNumFaces(7).validate().isOk());
  EXPECT_FALSE(TextureRangeDesc::newCube(0, 0, 1024, 1024).atFace(7).validate().isOk());
}

namespace {
TextureRangeDesc createTestRange() {
  TextureRangeDesc range;
  range.x = 1;
  range.y = 2;
  range.z = 3;
  range.width = 4;
  range.height = 5;
  range.depth = 6;
  range.layer = 7;
  range.numLayers = 8;
  range.mipLevel = 9;
  range.numMipLevels = 10;
  range.face = 11;
  range.numFaces = 12;

  return range;
}
} // namespace

TEST(TextureRangeDesc, Equality) {
  const auto rangeA = createTestRange();
  {
    const auto rangeB = createTestRange();
    EXPECT_TRUE(rangeA == rangeB);
    EXPECT_FALSE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.x = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.y = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.z = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.width = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.height = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.depth = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.layer = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.numLayers = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.mipLevel = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.numMipLevels = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.face = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
  {
    auto rangeB = createTestRange();
    rangeB.numFaces = 0;
    EXPECT_FALSE(rangeA == rangeB);
    EXPECT_TRUE(rangeA != rangeB);
  }
}

TEST(TextureFormatProperties, Construction) {
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(std::string(props.name), std::string("RGBA_UNorm8"));
    EXPECT_EQ(props.format, TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(props.componentsPerPixel, 4);
    EXPECT_EQ(props.bytesPerBlock, 4);
    EXPECT_EQ(props.blockWidth, 1);
    EXPECT_EQ(props.blockHeight, 1);
    EXPECT_EQ(props.blockDepth, 1);
    EXPECT_EQ(props.minBlocksX, 1);
    EXPECT_EQ(props.minBlocksY, 1);
    EXPECT_EQ(props.minBlocksZ, 1);
  }
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    EXPECT_EQ(std::string(props.name), std::string("RGB_PVRTC_2BPPV1"));
    EXPECT_EQ(props.format, TextureFormat::RGB_PVRTC_2BPPV1);
    EXPECT_EQ(props.componentsPerPixel, 3);
    EXPECT_EQ(props.bytesPerBlock, 8);
    EXPECT_EQ(props.blockWidth, 8);
    EXPECT_EQ(props.blockHeight, 4);
    EXPECT_EQ(props.blockDepth, 1);
    EXPECT_EQ(props.minBlocksX, 2);
    EXPECT_EQ(props.minBlocksY, 2);
    EXPECT_EQ(props.minBlocksZ, 1);
  }
}

TEST(TextureFormatProperties, GetRows) {
  {
    const auto range = TextureRangeDesc::new2D(0, 0, 2, 2);
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(props.getRows(range), 2);
    EXPECT_EQ(props.getRows(range.withNumMipLevels(2)), 3);
  }
  {
    const auto range = TextureRangeDesc::new2D(0, 0, 2, 2);
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    // MinBlocksY = 2
    EXPECT_EQ(props.getRows(range), 2);
    EXPECT_EQ(props.getRows(range.withNumMipLevels(2)), 4);
  }
  {
    const auto range = TextureRangeDesc::new3D(0, 0, 0, 2, 2, 2);
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(props.getRows(range), 4);
    EXPECT_EQ(props.getRows(range.withNumMipLevels(2)), 5);
  }
  {
    const auto range = TextureRangeDesc::new3D(0, 0, 0, 2, 2, 2);
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    // MinBlocksY = 2
    EXPECT_EQ(props.getRows(range), 4);
    EXPECT_EQ(props.getRows(range.withNumMipLevels(2)), 6);
  }
  {
    const auto range = TextureRangeDesc::new2DArray(0, 0, 2, 2, 0, 2);
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(props.getRows(range), 4);
    EXPECT_EQ(props.getRows(range.withNumMipLevels(2)), 6);
  }
  {
    const auto range = TextureRangeDesc::new2DArray(0, 0, 2, 2, 0, 2);
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    // MinBlocksY = 2
    EXPECT_EQ(props.getRows(range), 4);
    EXPECT_EQ(props.getRows(range.withNumMipLevels(2)), 8);
  }
  {
    const auto range = TextureRangeDesc::newCube(0, 0, 2, 2);
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(props.getRows(range), 12);
    EXPECT_EQ(props.getRows(range.atFace(TextureCubeFace::NegX)), 2);
    EXPECT_EQ(props.getRows(range.withNumMipLevels(2)), 18);
  }
  {
    const auto range = TextureRangeDesc::newCube(0, 0, 2, 2);
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    // MinBlocksY = 2
    EXPECT_EQ(props.getRows(range), 12);
    EXPECT_EQ(props.getRows(range.atFace(TextureCubeFace::NegX)), 2);
    EXPECT_EQ(props.getRows(range.withNumMipLevels(2)), 24);
  }
}

TEST(TextureFormatProperties, GetBytesPerRow) {
  const auto range = TextureRangeDesc::new2D(0, 0, 2, 2);
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(props.getBytesPerRow(range), 2 * 4);
  }
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    // minBlocksX = 2
    EXPECT_EQ(props.getBytesPerRow(range), 2 * 1 * 8);
  }
}

TEST(TextureFormatProperties, getBytesPerLayer) {
  const auto range = TextureRangeDesc::new2D(0, 0, 10, 10);
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(props.getBytesPerLayer(range), 10 * 10 * 4);
    EXPECT_EQ(props.getBytesPerLayer(range, 50), 10 * 50);

    EXPECT_EQ(props.getBytesPerLayer(10, 10, 1), 10 * 10 * 4);
    EXPECT_EQ(props.getBytesPerLayer(10, 10, 1, 50), 10 * 50);
  }
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    // 2 blocks x 3 blocks
    EXPECT_EQ(props.getBytesPerLayer(range), 2 * 3 * 8);
    EXPECT_EQ(props.getBytesPerLayer(10, 10, 1), 2 * 3 * 8);
  }
}

TEST(TextureFormatProperties, getBytesPerRange) {
  auto range = TextureRangeDesc::new2D(0, 0, 10, 10, 0, 3);
  auto cubeRange = TextureRangeDesc::newCube(0, 0, 10, 10, 0, 3);
  auto cubeFaceRange = TextureRangeDesc::newCubeFace(0, 0, 10, 10, TextureCubeFace::PosZ, 0, 3);
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    // Level 0: 10 pixels x 10 pixels
    // Level 1:  5 pixels x  5 pixels
    // Level 2:  2 pixels x  2 pixels
    const auto bytes = ((10 * 10) + (5 * 5) + (2 * 2)) * 4;
    EXPECT_EQ(props.getBytesPerRange(range), bytes);
    EXPECT_EQ(props.getBytesPerRange(cubeRange), bytes * 6);
    EXPECT_EQ(props.getBytesPerRange(cubeFaceRange), bytes);
  }
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    // Level 0: 10 pixels x 10 pixels
    const auto bytes = 10 * 50;
    EXPECT_EQ(props.getBytesPerRange(range.atMipLevel(0), 50), bytes);
    EXPECT_EQ(props.getBytesPerRange(cubeRange.atMipLevel(0), 50), bytes * 6);
    EXPECT_EQ(props.getBytesPerRange(cubeFaceRange.atMipLevel(0), 50), bytes);
  }
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    // Level 0: 2 blocks x 3 blocks
    // Level 1: 2 blocks x 2 blocks
    // Level 2: 2 blocks x 2 blocks
    const auto bytes = ((2 * 3) + (2 * 2) + (2 * 2)) * 8;
    EXPECT_EQ(props.getBytesPerRange(range), bytes);
    EXPECT_EQ(props.getBytesPerRange(cubeRange), bytes * 6);
    EXPECT_EQ(props.getBytesPerRange(cubeFaceRange), bytes);
  }
}

TEST(TextureFormatProperties, getSubRangeByteOffset) {
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    {
      // 2D Range
      const auto range = TextureRangeDesc::new2D(0, 0, 10, 10, 0, 3);
      // Level 0: 10 pixels x 10 pixels = 400 bytes
      // Level 1:  5 pixels x  5 pixels = 100 bytes
      // Level 2:  2 pixels x  2 pixels =  16 bytes

      EXPECT_EQ(props.getSubRangeByteOffset(range, range), 0);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1)), 400);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(2)), 500);
    }

    {
      // 2D Aray Range
      const auto range = TextureRangeDesc::new2DArray(0, 0, 10, 10, 0, 2, 0, 3);
      // Level 0: 10 pixels x 10 pixels x 2 layers = 800 bytes
      // Level 1:  5 pixels x  5 pixels x 2 layers = 200 bytes
      // Level 2:  2 pixels x  2 pixels x 2 layers =  32 bytes

      EXPECT_EQ(props.getSubRangeByteOffset(range, range), 0);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atLayer(1)), 400);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1)), 800);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1).atLayer(1)), 900);

      // Custom row length
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atLayer(1), 50), 500);
    }

    {
      // 3D Range
      const auto range = TextureRangeDesc::new3D(0, 0, 0, 10, 10, 10, 0, 3);
      // Level 0: 10 pixels x 10 pixels x 10 pixels = 4000 bytes
      // Level 1:  5 pixels x  5 pixels x  5 pixels =  500 bytes
      // Level 2:  2 pixels x  2 pixels x  2 pixels =   32 bytes

      EXPECT_EQ(props.getSubRangeByteOffset(range, range), 0);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1)), 4000);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(2)), 4500);
    }

    {
      // Cube Range
      const auto range = TextureRangeDesc::newCube(0, 0, 10, 10, 0, 3);
      // Level 0: 10 pixels x 10 pixels x 6 faces = 2400 bytes
      // Level 1:  5 pixels x  5 pixels x 6 faces =  600 bytes
      // Level 2:  2 pixels x  2 pixels x 6 faces =   96 bytes

      EXPECT_EQ(props.getSubRangeByteOffset(range, range), 0);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atFace(1)), 400);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1)), 2400);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1).atFace(1)), 2500);

      // Custom row length
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atFace(1), 50), 500);
    }
  }
  {
    const auto props = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB_PVRTC_2BPPV1);
    {
      // 2D Range
      const auto range = TextureRangeDesc::new2D(0, 0, 10, 10, 0, 3);
      // Level 0: 2 blocks x 3 blocks x 8 bytes = 48 bytes
      // Level 1: 2 blocks x 2 blocks x 8 bytes = 32 bytes
      // Level 2: 2 blocks x 2 blocks x 8 bytes = 32 bytes

      EXPECT_EQ(props.getSubRangeByteOffset(range, range), 0);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1)), 48);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(2)), 80);
    }

    {
      // 2D Aray Range
      const auto range = TextureRangeDesc::new2DArray(0, 0, 10, 10, 0, 2, 0, 3);
      // Level 0: 2 blocks x 3 blocks x 2 layers x 8 bytes = 96 bytes
      // Level 1: 2 blocks x 2 blocks x 2 layers x 8 bytes = 64 bytes
      // Level 2: 2 blocks x 2 blocks x 2 layers x 8 bytes = 64 bytes

      EXPECT_EQ(props.getSubRangeByteOffset(range, range), 0);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atLayer(1)), 48);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1)), 96);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1).atLayer(1)), 128);
    }

    {
      // 3D Range
      const auto range = TextureRangeDesc::new3D(0, 0, 0, 10, 10, 10, 0, 3);
      // Level 0: 2 blocks x 3 blocks x 10 pixels x 8 bytes = 480 bytes
      // Level 1: 2 blocks x 2 blocks x  5 pixels x 8 bytes = 160 bytes
      // Level 2: 2 blocks x 2 blocks x  2 pixels x 8 bytes =  64 bytes

      EXPECT_EQ(props.getSubRangeByteOffset(range, range), 0);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1)), 480);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(2)), 640);
    }

    {
      // Cube Range
      const auto range = TextureRangeDesc::newCube(0, 0, 10, 10, 0, 3);
      // Level 0: 2 blocks x 3 blocks x 6 faces x 8 bytes = 288 bytes
      // Level 1: 2 blocks x 2 blocks x 6 faces x 8 bytes = 192 bytes
      // Level 2: 2 blocks x 2 blocks x 6 faces x 8 bytes = 192 bytes

      EXPECT_EQ(props.getSubRangeByteOffset(range, range), 0);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atFace(1)), 48);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1)), 288);
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.atMipLevel(1).atFace(1)), 320);
    }
  }
}

TEST_F(TextureTest, Upload) {
  Result ret;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 kOffscreenTexWidth,
                                                 kOffscreenTexHeight,
                                                 TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

  inputTexture_->upload(rangeDesc, data::texture::TEX_RGBA_2x2);

  //----------------
  // Validate data
  //----------------
  util::validateUploadedTexture(
      *iglDev_, *cmdQueue_, inputTexture_, data::texture::TEX_RGBA_2x2, "Passthrough");
}

//
// Texture Passthrough Test
//
// This test uses a simple shader to copy the input texture to a same
// sized output texture (offscreenTexture_)
//
TEST_F(TextureTest, Passthrough) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOffscreenTexWidth,
                                           kOffscreenTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

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
  cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);
  cmds->bindBuffer(data::shader::simpleUvIndex, BindTarget::kVertex, uv_, 0);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------
  // Validate output
  //----------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *framebuffer_, data::texture::TEX_RGBA_2x2, "Passthrough");
}

//
// This test uses a simple shader to copy the input texture with a
// texture to a same sized output texture (offscreenTexture_)
// The difference between this test and PassthroughTexture is that
// a section of the original input texture is updated. This is meant
// to exercise the sub-texture upload path.
//
TEST_F(TextureTest, PassthroughSubTexture) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //------------------------------------------------------
  // Create input texture and sub-texture, and upload data
  //------------------------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOffscreenTexWidth,
                                           kOffscreenTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

  inputTexture_->upload(rangeDesc, data::texture::TEX_RGBA_2x2);

  // Upload right lower corner as a single-pixel sub-texture.
  auto singlePixelDesc =
      TextureRangeDesc::new2D(kOffscreenTexWidth - 1, kOffscreenTexHeight - 1, 1, 1);
  int32_t singlePixelColor = 0x44332211;

  inputTexture_->upload(singlePixelDesc, &singlePixelColor);

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
  cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);
  cmds->bindBuffer(data::shader::simpleUvIndex, BindTarget::kVertex, uv_, 0);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------
  // Validate output
  //----------------
  util::validateFramebufferTexture(*iglDev_,
                                   *cmdQueue_,
                                   *framebuffer_,
                                   data::texture::TEX_RGBA_2x2_MODIFIED,
                                   "PassthroughSubTexture");
}

//
// Framebuffer to Texture Copy Test
//
// This test will exercise the copy functionality via the following steps:
//   1. clear FB to (0.5, 0.5, 0.5, 0.5)
//   2. Copy content to a texture
//   3. clear FB to (0, 0, 0, 0) and verify it is cleared
//   4. Copy texture content to FB
//   5. Verify that the FB is back to (0.5, 0.5, 0.5, 0.5)
//
TEST_F(TextureTest, FBCopy) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;
  std::shared_ptr<ITexture> dstTexture;

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

  //--------------------------------
  // Create copy destination texture
  //--------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOffscreenTexWidth,
                                           kOffscreenTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  texDesc.debugName = "Texture: TextureTest::FBCopy::dstTexture";
  dstTexture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(dstTexture != nullptr);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(pipelineState != nullptr);

  //---------------------------------
  // Clear FB to {0.5, 0.5, 0.5, 0.5}
  //---------------------------------
  renderPass_.colorAttachments[0].clearColor = {0.501f, 0.501f, 0.501f, 0.501f};

  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);
  cmds->bindBuffer(data::shader::simpleUvIndex, BindTarget::kVertex, uv_, 0);
  cmds->bindRenderPipelineState(pipelineState);

  // draw 0 indices here just to clear the FB
  cmds->drawIndexed(PrimitiveType::Triangle, 0, IndexFormat::UInt16, *ib_, 0);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------------------------------------------------------------
  // Validate framebuffer texture
  //----------------------------------------------------------------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *framebuffer_, data::texture::TEX_RGBA_GRAY_2x2, "After Initial Clear");

  //------------------------
  // Copy content to texture
  //------------------------
  framebuffer_->copyTextureColorAttachment(*cmdQueue_, 0, dstTexture, rangeDesc);

  //-------------------------
  // Clear FB to {0, 0, 0, 0}
  //-------------------------
  renderPass_.colorAttachments[0].clearColor = {0, 0, 0, 0};

  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());

  cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);
  cmds->bindBuffer(data::shader::simpleUvIndex, BindTarget::kVertex, uv_, 0);
  cmds->bindRenderPipelineState(pipelineState);
  cmds->drawIndexed(PrimitiveType::Triangle, 0, IndexFormat::UInt16, *ib_, 0);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //-----------------------------------
  // Validate framebuffer texture again
  //-----------------------------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *framebuffer_, data::texture::TEX_RGBA_CLEAR_2x2, "After Second Clear");

  //---------------------------------------------
  // Copy dstTexture to FB so we can read it back
  //---------------------------------------------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());

  cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);
  cmds->bindBuffer(data::shader::simpleUvIndex, BindTarget::kVertex, uv_, 0);

  cmds->bindRenderPipelineState(pipelineState);

  // Using dstTexture as input here
  cmds->bindTexture(textureUnit_, BindTarget::kFragment, dstTexture.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //------------------------------------------------------
  // Read back framebuffer. Should be {0.5, 0.5, 0.5, 0.5}
  //------------------------------------------------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *framebuffer_, data::texture::TEX_RGBA_GRAY_2x2, "After Copy");
}

constexpr uint32_t kAlignedPixelsWidth = 3u;
constexpr uint32_t kAlignedPixelsHeight = 2u;

constexpr std::array<uint32_t, 6> kPixelsAligned12 = {1u, 2u, 3u, 4u, 5u, 6u};

// clang-format off
constexpr std::array<uint8_t, 28> kPixelsAligned14 = {
    1, 0, 0, 0,
    2, 0, 0, 0,
    3, 0, 0, 0,
    0, 0, // Expected to be skipped
    4, 0, 0, 0,
    5, 0, 0, 0,
    6, 0, 0, 0,
    0, 0, // Expected to be skipped
};
// clang-format on
constexpr std::array<uint32_t, 8> kPixelsAligned16 = {1u,
                                                      2u,
                                                      3u,
                                                      0x00000000u, // Expected to be skipped
                                                      4u,
                                                      5u,
                                                      6u,
                                                      0x00000000u}; // Expected to be skipped
constexpr std::array<uint32_t, 10> kPixelsAligned20 = {1u,
                                                       2u,
                                                       3u,
                                                       0x00000000u, // Expected to be skipped
                                                       0x00000000u, // Expected to be skipped
                                                       4u,
                                                       5u,
                                                       6u,
                                                       0x00000000u, // Expected to be skipped
                                                       0x00000000u}; // Expected to be skipped

constexpr std::array<std::pair<const void*, uint32_t>, 4> kPixelAlignments = {
    // 12 byte row will triggers 4 byte alignment.
    // No padding required since the width equals number of input pixels per row.
    std::make_pair(kPixelsAligned12.data(), kAlignedPixelsWidth * 4u),
    // 14 byte row will trigger 2 byte alignment since texture width is set to 3.
    // Padding of 0.5 pixels used per row of width 3.
    std::make_pair(kPixelsAligned14.data(), kAlignedPixelsWidth * 4u + 2u),
    // 16 byte row will trigger 8 byte alignment since texture width is set to 3.
    // Padding of 1 pixel used per row of width 3.
    std::make_pair(kPixelsAligned16.data(), (kAlignedPixelsWidth + 1u) * 4u),
    // 20 byte row is neither 8, 4, 2, nor 1 byte aligned.
    // Padding of 2 pixels used per row of width 3.
    std::make_pair(kPixelsAligned20.data(), (kAlignedPixelsWidth + 2u) * 4u),
};

//
// Test ITexture::repackData
//
TEST_F(TextureTest, RepackData) {
  const auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
  const auto range = TextureRangeDesc::new2D(0, 0, kAlignedPixelsWidth, kAlignedPixelsHeight);

  for (const auto& [data, bytesPerRow] : kPixelAlignments) {
    const size_t alignedSize =
        static_cast<size_t>(kAlignedPixelsWidth) * static_cast<size_t>(kAlignedPixelsHeight);
    const size_t unalignedSize =
        static_cast<size_t>(kAlignedPixelsHeight) * static_cast<size_t>(bytesPerRow);
    {
      //------------------
      // Test packing data
      //------------------

      std::vector<uint32_t> packedData(alignedSize);
      ITexture::repackData(properties,
                           range,
                           static_cast<const uint8_t*>(data),
                           bytesPerRow,
                           reinterpret_cast<uint8_t*>(packedData.data()),
                           0);

      for (size_t i = 0; i < packedData.size(); ++i) {
        EXPECT_EQ(packedData[i], kPixelsAligned12[i]);
      }
    }

    {
      //-----------------------------
      // Test packing + flipping data
      //-----------------------------

      std::vector<uint32_t> packedFlippedData(alignedSize);
      ITexture::repackData(properties,
                           range,
                           static_cast<const uint8_t*>(data),
                           bytesPerRow,
                           reinterpret_cast<uint8_t*>(packedFlippedData.data()),
                           0,
                           true);

      for (size_t i = 0; i < kAlignedPixelsWidth; ++i) {
        EXPECT_EQ(packedFlippedData[i], kPixelsAligned12[i + kAlignedPixelsWidth]);
        EXPECT_EQ(packedFlippedData[i + kAlignedPixelsWidth], kPixelsAligned12[i]);
      }
    }

    {
      //--------------------
      // Test unpacking data
      //--------------------

      std::vector<uint8_t> unpackedData(unalignedSize);
      ITexture::repackData(properties,
                           range,
                           reinterpret_cast<const uint8_t*>(kPixelsAligned12.data()),
                           0,
                           unpackedData.data(),
                           bytesPerRow);

      for (size_t i = 0; i < unpackedData.size(); ++i) {
        EXPECT_EQ(unpackedData[i], reinterpret_cast<const uint8_t*>(data)[i]);
      }
    }

    {
      //-------------------------------
      // Test unpacking + flipping data
      //-------------------------------

      std::vector<uint8_t> unpackedFlippedData(unalignedSize);
      ITexture::repackData(properties,
                           range,
                           reinterpret_cast<const uint8_t*>(kPixelsAligned12.data()),
                           0,
                           unpackedFlippedData.data(),
                           bytesPerRow,
                           true);

      const auto width = unpackedFlippedData.size() / 2;
      for (size_t i = 0; i < width; ++i) {
        EXPECT_EQ(unpackedFlippedData[i + width], reinterpret_cast<const uint8_t*>(data)[i]);
        EXPECT_EQ(unpackedFlippedData[i], reinterpret_cast<const uint8_t*>(data)[i + width]);
      }
    }
  }
}

//
// Pixel upload alignment test
//
// In openGL, when writing to a gpu texture from cpu memory the cpu memory pixel rows can be
// packed a couple of different ways 1, 2, 4 or 8 byte aligned. This test ensures bytesPerRow gets
// converted to the correct byte alignment in openGL and works as expected in metal
//
// If a row has 3 RGBA pixels but is 8 byte aligned the row will be 16 bytes with the last 4 bytes
// being ignored. If it was instead 1, 2 or 4 byte aligned the row would be 12 bytes as 12 is
// divisible by a single pixels byte size.
//
// Expected output: Pixels read out are correct even when different bytes per pixel are used
// during upload.
//
// Note: This test only covers 4 and 8 byte alignment because copyBytesColorAttachment does not
// support reading non 4 byte formats
//
TEST_F(TextureTest, UploadAlignment) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create new frame buffer with a width and height that can cause different alignments
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kAlignedPixelsWidth,
                                           kAlignedPixelsHeight,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  auto customOffscreenTexture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(customOffscreenTexture != nullptr);

  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = customOffscreenTexture;
  auto customFramebuffer = iglDev_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(customFramebuffer != nullptr);

  for (const auto& [data, bytesPerRow] : kPixelAlignments) {
    //-------------------------------------
    // Create input texture and upload data
    //-------------------------------------
    texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                 kAlignedPixelsWidth,
                                 kAlignedPixelsHeight,
                                 TextureDesc::TextureUsageBits::Sampled);
    inputTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(inputTexture_ != nullptr);

    const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kAlignedPixelsWidth, kAlignedPixelsHeight);

    inputTexture_->upload(rangeDesc, data, static_cast<size_t>(bytesPerRow));

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

    auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, customFramebuffer);
    cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);
    cmds->bindBuffer(data::shader::simpleUvIndex, BindTarget::kVertex, uv_, 0);

    cmds->bindRenderPipelineState(pipelineState);

    cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
    cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

    cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();

    //----------------
    // Validate output
    //----------------
    const std::string alignmentStr = "UploadAlignment: " + std::to_string(bytesPerRow);
    util::validateFramebufferTexture(
        *iglDev_, *cmdQueue_, *customFramebuffer, kPixelsAligned12.data(), alignmentStr.c_str());
  }
}

//
// Texture Resize Test
//
// This test uses a simple shader to copy the input texture to a different
// sized output texture (offscreenTexture_)
//
TEST_F(TextureTest, Resize) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  constexpr uint32_t kInputTexWidth = 10u;
  constexpr uint32_t kInputTexHeight = 40u;
  constexpr uint32_t kOutputTexWidth = 5u;
  constexpr uint32_t kOutputTexHeight = 5u;
  constexpr size_t kTextureSize =
      static_cast<size_t>(kInputTexWidth) * static_cast<size_t>(kInputTexHeight);

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kInputTexWidth,
                                           kInputTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  auto rangeDesc = TextureRangeDesc::new2D(0, 0, kInputTexWidth, kInputTexHeight);

  // Allocate input texture and set color to 0x80808080
  std::vector<uint32_t> inputTexData(kTextureSize, 0x80808080);
  inputTexture_->upload(rangeDesc, inputTexData.data());

  //------------------------------------------------------------------------
  // Create a different sized output texture, and attach it to a framebuffer
  //------------------------------------------------------------------------
  texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                               kOutputTexWidth,
                               kOutputTexHeight,
                               TextureDesc::TextureUsageBits::Sampled |
                                   TextureDesc::TextureUsageBits::Attachment);

  auto outputTex = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(outputTex != nullptr);

  // Create framebuffer using the output texture
  FramebufferDesc framebufferDesc;

  framebufferDesc.colorAttachments[0].texture = outputTex;
  auto fb = iglDev_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(fb != nullptr);

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

  auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, fb);
  cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);
  cmds->bindBuffer(data::shader::simpleUvIndex, BindTarget::kVertex, uv_, 0);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  cmdBuf_->waitUntilCompleted();

  //----------------
  // Validate output
  //----------------
  util::validateFramebufferTexture(
      *iglDev_, *cmdQueue_, *fb, data::texture::TEX_RGBA_GRAY_5x5, "Resize");
}

//
// Texture Validate Range 2D
//
// This test validates some of the logic in validateRange for 2D textures.
//
TEST_F(TextureTest, ValidateRange2D) {
  Result ret;
  auto texDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 8, 8, TextureDesc::TextureUsageBits::Sampled);
  auto tex = iglDev_->createTexture(texDesc, &ret);

  ret = tex->validateRange(TextureRangeDesc::new2D(0, 0, 8, 8));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2D(4, 4, 4, 4));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2D(0, 0, 4, 4, 1));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2D(0, 0, 12, 12));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2D(0, 0, 0, 0));
  EXPECT_FALSE(ret.isOk());
}

//
// Texture Validate Range Cube
//
// This test validates some of the logic in validateRange for Cube textures.
//
TEST_F(TextureTest, ValidateRangeCube) {
  Result ret;
  auto texDesc = TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, 8, 8, TextureDesc::TextureUsageBits::Sampled);
  auto tex = iglDev_->createTexture(texDesc, &ret);

  ret = tex->validateRange(TextureRangeDesc::newCube(0, 0, 8, 8));
  EXPECT_TRUE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 8, 8, 1));
  EXPECT_TRUE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 8, 8, TextureCubeFace::NegX));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::newCube(4, 4, 4, 4));
  EXPECT_TRUE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(4, 4, 4, 4, 1));
  EXPECT_TRUE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(4, 4, 4, 4, TextureCubeFace::NegX));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::newCube(0, 0, 4, 4, 1));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 4, 4, 1, 1));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 4, 4, TextureCubeFace::NegX, 1));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::newCube(0, 0, 12, 12));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 12, 12, 1));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 12, 12, TextureCubeFace::NegX));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::newCube(0, 0, 0, 0));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 0, 0, 1));
  EXPECT_FALSE(ret.isOk());
  ret = tex->validateRange(TextureRangeDesc::newCubeFace(0, 0, 0, 0, TextureCubeFace::NegX));
  EXPECT_FALSE(ret.isOk());
}

//
// Texture Validate Range 3D
//
// This test validates some of the logic in validateRange for 3D textures.
//
TEST_F(TextureTest, ValidateRange3D) {
  if (!iglDev_->hasFeature(DeviceFeatures::Texture3D)) {
    GTEST_SKIP() << "3D textures not supported. Skipping.";
  }

  Result ret;
  auto texDesc = TextureDesc::new3D(
      TextureFormat::RGBA_UNorm8, 8, 8, 8, TextureDesc::TextureUsageBits::Sampled);
  auto tex = iglDev_->createTexture(texDesc, &ret);

  ret = tex->validateRange(TextureRangeDesc::new3D(0, 0, 0, 8, 8, 8));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new3D(4, 4, 4, 4, 4, 4));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new3D(0, 0, 0, 4, 4, 4, 1));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new3D(0, 0, 0, 12, 12, 12));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new3D(0, 0, 0, 0, 0, 0));
  EXPECT_FALSE(ret.isOk());
}

//
// Test render to mip
//
// Create a square output texture with a mip chain and render several different colors into each
// mip level. Read back individual mips to make sure they were written to correctly.
//

TEST_F(TextureTest, RenderToMip) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  // Use a square output texture with mips
  constexpr uint32_t kNumMipLevels = 4;
  constexpr uint32_t kOutputTexWidth = 8u;
  constexpr uint32_t kOutputTexHeight = 8u;
  static_assert(kOutputTexWidth > 1);
  static_assert(1 << (kNumMipLevels - 1) == kOutputTexWidth);
  static_assert(kOutputTexWidth == kOutputTexHeight);

  static constexpr std::array<uint32_t, kNumMipLevels> kColors = {
      0xdeadbeef, 0x8badf00d, 0xc00010ff, 0xbaaaaaad};

  std::vector<std::vector<uint32_t>> inputTexData{
      std::vector(64 /* 8 * 8 */, kColors[0]),
      std::vector(16 /* 4 * 4 */, kColors[1]),
      std::vector(4 /* 2 * 2 */, kColors[2]),
      std::vector(1 /* 1 * 1 */, kColors[3]),
  };

  //---------------------------------------------------------------------
  // Create output texture with mip levels and attach it to a framebuffer
  //---------------------------------------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOutputTexWidth,
                                           kOutputTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = kNumMipLevels;

  auto outputTex = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(outputTex != nullptr);

  // Create framebuffer using the output texture
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = outputTex;
  auto fb = iglDev_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(fb != nullptr);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  //-------------------------
  // Render to each mip level
  //-------------------------
  for (auto mipLevel = 0; mipLevel < kNumMipLevels; mipLevel++) {
    //---------------------
    // Create input texture
    //---------------------
    const int inTexWidth = kOutputTexWidth >> mipLevel;
    texDesc = TextureDesc::new2D(
        TextureFormat::RGBA_UNorm8, inTexWidth, inTexWidth, TextureDesc::TextureUsageBits::Sampled);
    inputTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(inputTexture_ != nullptr);

    // Initialize the input texture's color
    auto rangeDesc = TextureRangeDesc::new2D(0, 0, inTexWidth, inTexWidth);
    inputTexture_->upload(rangeDesc, inputTexData[mipLevel].data());

    cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf_ != nullptr);

    // Modify render pass to only draw to nth mip level
    renderPass_.colorAttachments[0].mipLevel = mipLevel;

    auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, fb);
    cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);
    cmds->bindBuffer(data::shader::simpleUvIndex, BindTarget::kVertex, uv_, 0);

    cmds->bindRenderPipelineState(pipelineState);

    cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
    cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

    cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();
  }

  // Do readback in a separate loop to ensure all mip levels have been rendered.
  for (size_t mipLevel = 0; mipLevel < kNumMipLevels; mipLevel++) {
    //----------------
    // Validate output
    //----------------
    util::validateFramebufferTextureRange(*iglDev_,
                                          *cmdQueue_,
                                          *fb,
                                          outputTex->getFullRange(mipLevel),
                                          inputTexData[mipLevel].data(),
                                          (std::string("Mip ") + std::to_string(mipLevel)).c_str());
  }
}

namespace {
void testUploadToMip(IDevice& device, ICommandQueue& cmdQueue, bool singleUpload) {
  Result ret;

  // Use a square output texture with mips
  constexpr uint32_t kNumMipLevels = 2;
  constexpr uint32_t kTexWidth = 2u;
  constexpr uint32_t kTexHeight = 2u;
  static_assert(kTexWidth > 1);
  static_assert(1 << (kNumMipLevels - 1) == kTexWidth);
  static_assert(kTexWidth == kTexHeight);

  constexpr uint32_t kBaseMipColor = 0xdeadbeef;
  constexpr uint32_t kMip1Color = 0x8badf00d;

  static constexpr std::array<uint32_t, 5> kMipTextureData = {
      kBaseMipColor, // Base Mip
      kBaseMipColor, // Base Mip
      kBaseMipColor, // Base Mip
      kBaseMipColor, // Base Mip
      kMip1Color, // Mip 1
  };
  static constexpr const uint32_t* kBaseMipData = kMipTextureData.data();
  static constexpr const uint32_t* kMip1Data = kMipTextureData.data() + 4;

  //---------------------------------------------------------------------
  // Create texture with mip levels
  //---------------------------------------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kTexWidth,
                                           kTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = kNumMipLevels;
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  ASSERT_TRUE(tex != nullptr);

  //---------------------------------------------------------------------
  // Validate initial state, upload pixel data, and generate mipmaps
  //---------------------------------------------------------------------
  if (singleUpload) {
    ret = tex->upload(tex->getFullRange(0, 2), kMipTextureData.data());
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  } else {
    ret = tex->upload(tex->getFullRange(0), kBaseMipData);
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;

    ret = tex->upload(tex->getFullRange(1), kMip1Data);
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  }

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(0), kBaseMipData, "Base Level");

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(1), kMip1Data, "Mip 1");
}
} // namespace

TEST_F(TextureTest, UploadToMip_LevelByLevel) {
  testUploadToMip(*iglDev_, *cmdQueue_, false);
}

TEST_F(TextureTest, UploadToMip_SingleUpload) {
  testUploadToMip(*iglDev_, *cmdQueue_, true);
}

namespace {
void testGenerateMipmap(IDevice& device, ICommandQueue& cmdQueue, bool withCommandQueue) {
  Result ret;

  // Use a square output texture with mips
  constexpr uint32_t kNumMipLevels = 2;
  constexpr uint32_t kTexWidth = 2u;
  constexpr uint32_t kTexHeight = 2u;
  static_assert(kTexWidth > 1);
  static_assert(1 << (kNumMipLevels - 1) == kTexWidth);
  static_assert(kTexWidth == kTexHeight);

  constexpr uint32_t kColor = 0xdeadbeef;
  constexpr std::array<uint32_t, 4> kBaseMipData = {kColor, kColor, kColor, kColor};
  constexpr std::array<uint32_t, 1> kInitialMip1Data = {0};
  constexpr std::array<uint32_t, 1> kGeneratedMip1Data = {kColor};

  //---------------------------------------------------------------------
  // Create texture with mip levels
  //---------------------------------------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kTexWidth,
                                           kTexWidth,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = kNumMipLevels;
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  ASSERT_TRUE(tex != nullptr);

  //---------------------------------------------------------------------
  // Validate initial state, upload pixel data, and generate mipmaps
  //---------------------------------------------------------------------
  ret = tex->upload(tex->getFullRange(0), kBaseMipData.data());
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;

  ret = tex->upload(tex->getFullRange(1), kInitialMip1Data.data());
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(0), kBaseMipData.data(), "Initial (level 0)");

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(1), kInitialMip1Data.data(), "Initial (level 1)");

  if (withCommandQueue) {
    tex->generateMipmap(cmdQueue);

    // Dummy command buffer to wait for completion.
    auto cmdBuf = cmdQueue.createCommandBuffer({}, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf != nullptr);
    cmdQueue.submit(*cmdBuf);
    cmdBuf->waitUntilCompleted();
  } else {
    auto cmdBuffer = cmdQueue.createCommandBuffer({}, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
    tex->generateMipmap(*cmdBuffer);
    cmdQueue.submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
  }

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(0), kBaseMipData.data(), "Final (level 0)");

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(1), kGeneratedMip1Data.data(), "Final (level 1)");
}
} // namespace

//
// Test generating mipmaps
//
// Create a texture and upload a solid color into the base mip level, verify the base and 1st mip
// level colors. Then generate mipmaps and verify again.
//
TEST_F(TextureTest, GenerateMipmapWithCommandQueue) {
  testGenerateMipmap(*iglDev_, *cmdQueue_, true);
}

TEST_F(TextureTest, GenerateMipmapWithCommandBuffer) {
  testGenerateMipmap(*iglDev_, *cmdQueue_, false);
}

TEST_F(TextureTest, GetTextureBytesPerRow) {
  const auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
  const auto range = TextureRangeDesc::new2D(0, 0, 10, 10);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(0)), 40);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(1)), 20);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(2)), 8);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(3)), 4);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(4)), 4);
}

TEST_F(TextureTest, GetTextureBytesPerLayer) {
  const auto range = TextureRangeDesc::new2D(0, 0, 10, 10);
  {
    // Uncompressed
    const auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(0)), 400);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(1)), 100);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(2)), 16);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(3)), 4);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(4)), 4);
  }
  {
    // Compressed
    // 16 bytes per 5x5 block
    const auto properties =
        TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_ASTC_5x5);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(0)), 64);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(1)), 16);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(2)), 16);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(3)), 16);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(4)), 16);
  }
  {
    // Compressed
    // 8 bytes per 4x4 block
    const auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB8_ETC2);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(0)), 72);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(1)), 32);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(2)), 8);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(3)), 8);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(4)), 8);
  }
}

//
// Test ITexture::getEstimatedSizeInBytes
//
TEST_F(TextureTest, GetEstimatedSizeInBytes) {
  auto calcSize =
      [&](size_t width, size_t height, TextureFormat format, size_t numMipLevels) -> size_t {
    Result ret;
    TextureDesc texDesc = TextureDesc::new2D(format,
                                             width,
                                             height,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
    texDesc.numMipLevels = numMipLevels;
    auto texture = iglDev_->createTexture(texDesc, &ret);
    if (ret.code != Result::Code::Ok || texture == nullptr) {
      return 0;
    }
    return texture->getEstimatedSizeInBytes();
  };

  const auto format = iglDev_->getBackendType() == BackendType::OpenGL
                          ? TextureFormat::R5G5B5A1_UNorm
                          : TextureFormat::RGBA_UNorm8;
  const size_t formatBytes = iglDev_->getBackendType() == BackendType::OpenGL ? 2 : 4;

  size_t bytes;
  bytes = static_cast<size_t>(12 * 34 * formatBytes);
  ASSERT_EQ(calcSize(12, 34, format, 1), bytes);
  bytes = static_cast<size_t>((16 + 8 + 4 + 2 + 1) * formatBytes);
  ASSERT_EQ(calcSize(16, 1, format, 5), bytes);
  if (iglDev_->hasFeature(DeviceFeatures::TextureNotPot)) {
    if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
      // ES 2.0 generates maximum mip levels
      bytes = static_cast<size_t>(
          (128 * 333 + 64 * 166 + 32 * 83 + 16 * 41 + 8 * 20 + 4 * 10 + 2 * 5 + 1 * 2 + 1 * 1) *
          formatBytes);
      ASSERT_EQ(calcSize(128, 333, format, 9), bytes);
    } else {
      bytes = static_cast<size_t>((128 * 333 + 64 * 166) * formatBytes);
      ASSERT_EQ(calcSize(128, 333, format, 2), bytes);
    }

    if (iglDev_->hasFeature(DeviceFeatures::TextureFormatRG)) {
      const auto rBytes = 1;
      const auto rgBytes = 2;
      bytes = static_cast<size_t>((16 + 8 + 4 + 2 + 1) * rBytes);
      ASSERT_EQ(calcSize(16, 1, TextureFormat::R_UNorm8, 5), bytes);
      if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
        // ES 2.0 generates maximum mip levels
        bytes = static_cast<size_t>(
            (128 * 333 + 64 * 166 + 32 * 83 + 16 * 41 + 8 * 20 + 4 * 10 + 2 * 5 + 1 * 2 + 1 * 1) *
            rgBytes);
        ASSERT_EQ(calcSize(128, 333, TextureFormat::RG_UNorm8, 9), bytes);
      } else {
        bytes = static_cast<size_t>((128 * 333 + 64 * 166) * rgBytes);
        ASSERT_EQ(calcSize(128, 333, TextureFormat::RG_UNorm8, 2), bytes);
      }
    }
  }
}

//
// Test ITexture::getFullRange and ITexture::getFullMipRange
//
TEST_F(TextureTest, GetRange) {
  auto createTexture = [&](size_t width,
                           size_t height,
                           TextureFormat format,
                           // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                           size_t numMipLevels) -> std::shared_ptr<ITexture> {
    Result ret;
    TextureDesc texDesc = TextureDesc::new2D(format,
                                             width,
                                             height,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
    texDesc.numMipLevels = numMipLevels;
    auto texture = iglDev_->createTexture(texDesc, &ret);
    if (ret.code != Result::Code::Ok || texture == nullptr) {
      return {};
    }
    return texture;
  };
  auto getFullRange = [&](size_t width,
                          size_t height,
                          TextureFormat format,
                          // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                          size_t numMipLevels,
                          size_t rangeMipLevel = 0,
                          size_t rangeNumMipLevels = 0) -> TextureRangeDesc {
    auto tex = createTexture(width, height, format, numMipLevels);
    return tex ? tex->getFullRange(rangeMipLevel,
                                   rangeNumMipLevels ? rangeNumMipLevels : numMipLevels)
               : TextureRangeDesc{};
  };
  auto getFullMipRange = [&](size_t width,
                             size_t height,
                             TextureFormat format,
                             // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                             size_t numMipLevels) -> TextureRangeDesc {
    auto tex = createTexture(width, height, format, numMipLevels);
    return tex ? tex->getFullMipRange() : TextureRangeDesc{};
  };
  auto rangesAreEqual = [&](const TextureRangeDesc& a, const TextureRangeDesc& b) -> bool {
    return std::memcmp(&a, &b, sizeof(TextureRangeDesc)) == 0;
  };
  const auto format = iglDev_->getBackendType() == BackendType::OpenGL
                          ? TextureFormat::R5G5B5A1_UNorm
                          : TextureFormat::RGBA_UNorm8;

  TextureRangeDesc range;
  range = TextureRangeDesc::new2D(0, 0, 12, 34, 0, 1);
  ASSERT_TRUE(rangesAreEqual(getFullRange(12, 34, format, 1), range));
  range = TextureRangeDesc::new2D(0, 0, 16, 1, 0, 4);
  ASSERT_TRUE(rangesAreEqual(getFullRange(16, 1, format, 4), range));

  // Test subset of mip levels
  ASSERT_TRUE(rangesAreEqual(getFullRange(16, 1, format, 4, 1, 1), range.atMipLevel(1)));

  // Test all mip levels
  ASSERT_TRUE(rangesAreEqual(getFullMipRange(16, 1, format, 4), range.withNumMipLevels(4)));

  if (iglDev_->hasFeature(DeviceFeatures::TextureNotPot)) {
    if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
      // ES 2.0 generates maximum mip levels
      range = TextureRangeDesc::new2D(0, 0, 128, 333, 0, 9);
      ASSERT_TRUE(rangesAreEqual(getFullRange(128, 333, format, 9), range));

      // Test all mip levels
      ASSERT_TRUE(rangesAreEqual(getFullMipRange(128, 333, format, 9), range.withNumMipLevels(9)));
    } else {
      range = TextureRangeDesc::new2D(0, 0, 128, 333, 0, 2);
      ASSERT_TRUE(rangesAreEqual(getFullRange(128, 333, format, 2), range));

      // Test all mip levels
      ASSERT_TRUE(rangesAreEqual(getFullMipRange(128, 333, format, 2), range.withNumMipLevels(2)));
    }
  }
}

//
// Test the functionality of TextureDesc::calcMipmapLevelCount
//
TEST(TextureDescStaticTest, CalcMipmapLevelCount) {
  ASSERT_EQ(TextureDesc::calcNumMipLevels(1, 1), 1);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(4, 8), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(8, 4), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(10, 10), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(10, 10, 10), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(8, 4, 4), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(4, 8, 4), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(4, 4, 8), 4);
}

//
// Test TextureFormatProperties::getNumMipLevels
//
TEST_F(TextureTest, GetNumMipLevels) {
  {
    auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(properties.getNumMipLevels(1, 1, 4), 1);
    EXPECT_EQ(properties.getNumMipLevels(2, 2, 4 * 4 + 4), 2);
    EXPECT_EQ(properties.getNumMipLevels(5, 5, 25 * 4 + 4 * 4 + 4), 3);

    auto range = TextureRangeDesc::new2D(0, 0, 100, 50, 0);
    range.numMipLevels = 5;
    EXPECT_EQ(properties.getNumMipLevels(100, 50, properties.getBytesPerRange(range)), 5);
  }

  {
    // Compressed
    // 16 bytes per 5x5 block
    auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_ASTC_5x5);
    EXPECT_EQ(properties.getNumMipLevels(1, 1, 16), 1);
    EXPECT_EQ(properties.getNumMipLevels(2, 2, 16 + 16), 2);
    EXPECT_EQ(properties.getNumMipLevels(5, 5, 16 + 16 + 16), 3);

    auto range = TextureRangeDesc::new2D(0, 0, 100, 50, 0);
    range.numMipLevels = 5;
    EXPECT_EQ(properties.getNumMipLevels(100, 50, properties.getBytesPerRange(range)), 5);
  }
}

} // namespace tests
} // namespace igl
