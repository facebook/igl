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

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/NameHandle.h>
#include <string>

namespace igl {
namespace tests {

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
#define OFFSCREEN_TEX_WIDTH 2
#define OFFSCREEN_TEX_HEIGHT 2

struct VertexUniforms {
  glm::vec4 viewDirection = glm::vec4(0.0);
};
//
// TextureTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class TextureCubeTest : public ::testing::Test {
 private:
 public:
  TextureCubeTest() = default;
  ~TextureCubeTest() override = default;

  std::shared_ptr<iglu::ManagedUniformBuffer> createVertexUniformBuffer(igl::IDevice& device,
                                                                        igl::Result* /*result*/) {
    std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer = nullptr;

    iglu::ManagedUniformBufferInfo info = {
        .index = 1,
        .length = sizeof(VertexUniforms),
        .uniforms = {{.name = "view",
                      .type = igl::UniformType::Float4,
                      .offset = offsetof(VertexUniforms, viewDirection)}}};

    vertUniformBuffer = std::make_shared<iglu::ManagedUniformBuffer>(device, info);

    IGL_ASSERT(vertUniformBuffer->result.isOk());

    return vertUniformBuffer;
  }

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

    // Initialize render pass descriptor
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    if (iglDev_->getBackendType() == BackendType::OpenGL) {
      util::createShaderStages(iglDev_,
                               igl::tests::data::shader::OGL_SIMPLE_VERT_SHADER_CUBE,
                               igl::tests::data::shader::shaderFunc,
                               igl::tests::data::shader::OGL_SIMPLE_FRAG_SHADER_CUBE,
                               igl::tests::data::shader::shaderFunc,
                               stages);
    } else if (iglDev_->getBackendType() == BackendType::Metal) {
      util::createShaderStages(iglDev_,
                               igl::tests::data::shader::MTL_SIMPLE_SHADER_CUBE,
                               igl::tests::data::shader::simpleVertFunc,
                               igl::tests::data::shader::simpleFragFunc,
                               stages);
    } else if (iglDev_->getBackendType() == BackendType::Vulkan) {
      util::createShaderStages(iglDev_,
                               igl::tests::data::shader::VULKAN_SIMPLE_VERT_SHADER_CUBE,
                               igl::tests::data::shader::shaderFunc,
                               igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_CUBE,
                               igl::tests::data::shader::shaderFunc,
                               stages);
    } else {
      ASSERT_TRUE(false);
    }

    shaderStages_ = std::move(stages);

    // Initialize input to vertex shader
    VertexInputStateDesc inputDesc;

    inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
    inputDesc.attributes[0].offset = 0;
    inputDesc.attributes[0].bufferIndex = data::shader::simplePosIndex;
    inputDesc.attributes[0].name = data::shader::simplePos;
    inputDesc.attributes[0].location = 0;
    inputDesc.inputBindings[0].stride = sizeof(float) * 4;

    // numAttributes has to equal to bindings when using more than 1 buffer
    inputDesc.numAttributes = inputDesc.numInputBindings = 1;

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

  VertexUniforms vertexUniforms_;

  size_t textureUnit_ = 0;
};

uint32_t R = 0x1F00000F;
uint32_t G = 0x002F001F;
uint32_t B = 0x00003F2F;

uint32_t textureArray[6][OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT] = {
    {R, R, R, R},
    {G, G, G, G},
    {B, B, B, B},
    {R | B, R | B, R | B, R | B},
    {R | G, R | G, R | G, R | G},
    {B | G, B | G, B | G, B | G}};

static glm::vec4 viewDirection[] = {{1.0f, 0.0f, 0.0f, 0.0f},
                                    {-1.0f, 0.0f, 0.0f, 0.0f},
                                    {0.0f, 1.0f, 0.0f, 0.0f},
                                    {0.0f, -1.0f, 0.0f, 0.0f},
                                    {0.0f, 0.0f, 1.0f, 0.0f},
                                    {0.0f, 0.0f, -1.0f, 0.0f}};

//
// Texture Passthrough Test
//
// This test uses a simple shader to copy the input texture to a same
// sized output texture (offscreenTexture_)
//
TEST_F(TextureCubeTest, Passthrough) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::newCube(TextureFormat::RGBA_UNorm8,
                                             OFFSCREEN_TEX_WIDTH,
                                             OFFSCREEN_TEX_HEIGHT,
                                             TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, OFFSCREEN_TEX_WIDTH, OFFSCREEN_TEX_HEIGHT);

  ASSERT_TRUE(
      inputTexture_->uploadCube(rangeDesc, igl::TextureCubeFace::PosX, textureArray[0]).isOk());
  ASSERT_TRUE(
      inputTexture_->uploadCube(rangeDesc, igl::TextureCubeFace::NegX, textureArray[1]).isOk());
  ASSERT_TRUE(
      inputTexture_->uploadCube(rangeDesc, igl::TextureCubeFace::PosY, textureArray[2]).isOk());
  ASSERT_TRUE(
      inputTexture_->uploadCube(rangeDesc, igl::TextureCubeFace::NegY, textureArray[3]).isOk());
  ASSERT_TRUE(
      inputTexture_->uploadCube(rangeDesc, igl::TextureCubeFace::PosZ, textureArray[4]).isOk());
  ASSERT_TRUE(
      inputTexture_->uploadCube(rangeDesc, igl::TextureCubeFace::NegZ, textureArray[5]).isOk());
  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  for (int side = 0; side < 6; ++side) {
    //-------
    // Render
    //-------
    cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf_ != nullptr);

    auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
    cmds->bindBuffer(data::shader::simplePosIndex, BindTarget::kVertex, vb_, 0);

    cmds->bindRenderPipelineState(pipelineState);

    cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
    cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

    Result result{};
    auto vertUniformBuffer = createVertexUniformBuffer(*iglDev_.get(), &result);
    ASSERT_TRUE(result.isOk());

    vertexUniforms_.viewDirection = viewDirection[side];

    *static_cast<VertexUniforms*>(vertUniformBuffer->getData()) = vertexUniforms_;
    vertUniformBuffer->bind(*iglDev_.get(), *pipelineState, *cmds.get());

    cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();

    //----------------
    // Validate output
    //----------------
    util::validateFramebufferTexture(
        *iglDev_, *cmdQueue_, *framebuffer_, textureArray[side], "Passthrough");
  }
}

//
// Test ITexture::getEstimatedSizeInBytes
//
TEST_F(TextureCubeTest, GetEstimatedSizeInBytes) {
  auto calcSize =
      [&](size_t width, size_t height, TextureFormat format, size_t numMipLevels) -> size_t {
    Result ret;
    TextureDesc texDesc = TextureDesc::newCube(format,
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
  const uint32_t formatBytes = iglDev_->getBackendType() == BackendType::OpenGL ? 2u : 4u;

  uint32_t bytes;
  bytes = 34u * 34u * formatBytes * 6u;
  ASSERT_EQ(calcSize(34, 34, format, 1), bytes);
  bytes = (16u * 16u + 8u * 8u + 4u * 4u + 2u * 2u + 1u) * formatBytes * 6u;
  ASSERT_EQ(calcSize(16, 16, format, 5), bytes);
}

//
// Test ITexture::getFullRange and ITexture::getCubeFaceRange and
//
TEST_F(TextureCubeTest, GetRange) {
  auto createTexture = [&](size_t width,
                           size_t height,
                           TextureFormat format,
                           size_t numMipLevels) -> std::shared_ptr<ITexture> {
    Result ret;
    TextureDesc texDesc = TextureDesc::newCube(format,
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
  auto getCubeFaceRangeEnum = [&](size_t width,
                                  size_t height,
                                  TextureFormat format,
                                  size_t numMipLevels,
                                  TextureCubeFace face,
                                  size_t rangeMipLevel = 0,
                                  size_t rangeNumMipLevels = 0) -> TextureRangeDesc {
    auto tex = createTexture(width, height, format, numMipLevels);
    return tex ? tex->getCubeFaceRange(
                     face, rangeMipLevel, rangeNumMipLevels ? rangeNumMipLevels : numMipLevels)
               : TextureRangeDesc{};
  };
  auto getCubeFaceRangeNum = [&](size_t width,
                                 size_t height,
                                 TextureFormat format,
                                 size_t numMipLevels,
                                 size_t face,
                                 size_t rangeMipLevel = 0,
                                 size_t rangeNumMipLevels = 0) -> TextureRangeDesc {
    auto tex = createTexture(width, height, format, numMipLevels);
    return tex ? tex->getCubeFaceRange(
                     face, rangeMipLevel, rangeNumMipLevels ? rangeNumMipLevels : numMipLevels)
               : TextureRangeDesc{};
  };
  auto rangesAreEqual = [&](const TextureRangeDesc& a, const TextureRangeDesc& b) -> bool {
    return std::memcmp(&a, &b, sizeof(TextureRangeDesc)) == 0;
  };
  const auto format = iglDev_->getBackendType() == BackendType::OpenGL
                          ? TextureFormat::R5G5B5A1_UNorm
                          : TextureFormat::RGBA_UNorm8;

  TextureRangeDesc range;
  range = TextureRangeDesc::newCube(0, 0, 34, 34, 0, 1);
  ASSERT_TRUE(rangesAreEqual(getFullRange(34, 34, format, 1), range));
  ASSERT_TRUE(rangesAreEqual(getCubeFaceRangeEnum(34, 34, format, 1, TextureCubeFace::NegX),
                             range.atFace(TextureCubeFace::NegX)));
  ASSERT_TRUE(rangesAreEqual(getCubeFaceRangeNum(34, 34, format, 1, 1), range.atFace(1)));

  range = TextureRangeDesc::newCube(0, 0, 16, 16, 0, 5);
  ASSERT_TRUE(rangesAreEqual(getFullRange(16, 16, format, 5), range));
  ASSERT_TRUE(rangesAreEqual(getCubeFaceRangeEnum(16, 16, format, 5, TextureCubeFace::NegX),
                             range.atFace(TextureCubeFace::NegX)));
  ASSERT_TRUE(rangesAreEqual(getCubeFaceRangeNum(16, 16, format, 5, 1), range.atFace(1)));

  // Subset of mip levels
  ASSERT_TRUE(rangesAreEqual(getFullRange(16, 16, format, 5, 1, 1), range.atMipLevel(1)));
  ASSERT_TRUE(rangesAreEqual(getCubeFaceRangeEnum(16, 16, format, 5, TextureCubeFace::NegX, 1, 1),
                             range.atFace(TextureCubeFace::NegX).atMipLevel(1)));
  ASSERT_TRUE(rangesAreEqual(getCubeFaceRangeNum(16, 16, format, 5, 1, 1, 1),
                             range.atFace(1).atMipLevel(1)));
}

} // namespace tests
} // namespace igl
