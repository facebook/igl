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

#include <cstddef>
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
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.withNumMipLevels(1).atLayer(1), 50), 500);
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
      EXPECT_EQ(props.getSubRangeByteOffset(range, range.withNumMipLevels(1).atFace(1), 50), 500);
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

} // namespace igl::tests
