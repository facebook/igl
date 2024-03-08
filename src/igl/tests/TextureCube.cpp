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
#include "util/TextureValidationHelpers.h"

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/NameHandle.h>
#include <string>

namespace igl::tests {

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
constexpr size_t kOffscreenTexWidth = 2;
constexpr size_t kOffscreenTexHeight = 2;

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

    TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             kOffscreenTexWidth,
                                             kOffscreenTexHeight,
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

    // Initialize UV data and sampler buffer
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

constexpr uint32_t kR = 0x1F00001F;
constexpr uint32_t kG = 0x002F002F;
constexpr uint32_t kB = 0x00003F4F;
constexpr uint32_t kC = 0x004F5F3F;
constexpr uint32_t kM = 0x6F007F4F;
constexpr uint32_t kY = 0x8F9F005F;

// clang-format off
constexpr std::array<uint32_t, 30> kTextureData = {
  kR, kR, kR, kR,                     // Base Mip, Face 0
  kG, kG, kG, kG,                     // Base Mip, Face 1
  kB, kB, kB, kB,                     // Base Mip, Face 2
  kR | kB, kR | kB, kR | kB, kR | kB, // Base Mip, Face 3
  kR | kG, kR | kG, kR | kG, kR | kG, // Base Mip, Face 4
  kB | kG, kB | kG, kB | kG, kB | kG, // Base Mip, Face 5
  kC,                                 // Mip 1, Face 0
  kM,                                 // Mip 1, Face 1
  kY,                                 // Mip 1, Face 2
  kC | kM,                            // Mip 1, Face 3
  kC | kY,                            // Mip 1, Face 4
  kM | kY,                            // Mip 1, Face 5
};
// clang-format on

constexpr std::array<const uint32_t*, 6> kBaseMipTextureFaceData{
    kTextureData.data() + 0,
    kTextureData.data() + 4,
    kTextureData.data() + 8,
    kTextureData.data() + 12,
    kTextureData.data() + 16,
    kTextureData.data() + 20,
};

constexpr std::array<const uint32_t*, 6> kMip1TextureFaceData{
    kTextureData.data() + 24,
    kTextureData.data() + 25,
    kTextureData.data() + 26,
    kTextureData.data() + 27,
    kTextureData.data() + 28,
    kTextureData.data() + 29,
};

static const std::array<glm::vec4, 6> kViewDirection = {glm::vec4{1.0f, 0.0f, 0.0f, 0.0f},
                                                        glm::vec4{-1.0f, 0.0f, 0.0f, 0.0f},
                                                        glm::vec4{0.0f, 1.0f, 0.0f, 0.0f},
                                                        glm::vec4{0.0f, -1.0f, 0.0f, 0.0f},
                                                        glm::vec4{0.0f, 0.0f, 1.0f, 0.0f},
                                                        glm::vec4{0.0f, 0.0f, -1.0f, 0.0f}};

//
// Test uploading cube maps
//
// Create a cube map texture and upload different solid color into each face. Then verify the color
// of each face.
//
namespace {
void runUploadTest(IDevice& device, ICommandQueue& cmdQueue, bool singleUpload) {
  Result ret;

  //--------------------
  // Create cube texture
  //--------------------
  const TextureDesc texDesc = TextureDesc::newCube(TextureFormat::RGBA_UNorm8,
                                                   kOffscreenTexWidth,
                                                   kOffscreenTexWidth,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment);
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  ASSERT_TRUE(tex != nullptr);

  //---------------------------------------------------------------------
  // Upload pixel data and validate faces
  //---------------------------------------------------------------------
  if (singleUpload) {
    for (size_t face = 0; face < 6; ++face) {
      ASSERT_TRUE(tex->upload(tex->getFullRange(0), kTextureData.data()).isOk());
    }
  } else {
    for (size_t face = 0; face < 6; ++face) {
      ASSERT_TRUE(
          tex->upload(tex->getCubeFaceRange(face, 0), kBaseMipTextureFaceData[face]).isOk());
    }
  }

  for (size_t face = 0; face < 6; ++face) {
    const auto faceStr = "Face " + std::to_string(face);
    util::validateUploadedTextureRange(device,
                                       cmdQueue,
                                       tex,
                                       tex->getCubeFaceRange(face),
                                       kBaseMipTextureFaceData[face],
                                       faceStr.c_str());
  }
}
} // namespace

TEST_F(TextureCubeTest, Upload_SingleUpload) {
  runUploadTest(*iglDev_, *cmdQueue_, true);
}

TEST_F(TextureCubeTest, Upload_FaceByFace) {
  runUploadTest(*iglDev_, *cmdQueue_, false);
}

//
// Test uploading cube maps including mipmaps
//
namespace {
void runUploadToMipTest(IDevice& device, ICommandQueue& cmdQueue, bool singleUpload) {
  Result ret;

  //------------------------------------
  // Create cube texture with mip levels
  //------------------------------------
  TextureDesc texDesc = TextureDesc::newCube(TextureFormat::RGBA_UNorm8,
                                             kOffscreenTexWidth,
                                             kOffscreenTexWidth,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = 2;
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  ASSERT_TRUE(tex != nullptr);

  //---------------------------------------------------------------------
  // Upload pixel data and validate faces
  //---------------------------------------------------------------------
  if (singleUpload) {
    ASSERT_TRUE(tex->upload(tex->getFullRange(0, 2), kTextureData.data()).isOk());
  } else {
    ASSERT_TRUE(tex->upload(tex->getFullRange(0, 1), kTextureData.data()).isOk());
    ASSERT_TRUE(tex->upload(tex->getFullRange(1, 1), kMip1TextureFaceData[0]).isOk());
  }

  for (size_t mipLevel = 0; mipLevel < 2; ++mipLevel) {
    for (size_t face = 0; face < 6; ++face) {
      const auto faceStr = "MipLevel " + std::to_string(mipLevel) + ";Face " + std::to_string(face);
      util::validateUploadedTextureRange(device,
                                         cmdQueue,
                                         tex,
                                         tex->getCubeFaceRange(face, mipLevel),
                                         mipLevel == 0 ? kBaseMipTextureFaceData[face]
                                                       : kMip1TextureFaceData[face],
                                         faceStr.c_str());
    }
  }
}
} // namespace

TEST_F(TextureCubeTest, UploadToMip_SingleUpload) {
  runUploadToMipTest(*iglDev_, *cmdQueue_, true);
}

TEST_F(TextureCubeTest, UploadToMip_LevelByLevel) {
  runUploadToMipTest(*iglDev_, *cmdQueue_, false);
}

//
// Texture Passthrough Test - Sample From Cube
//
// This test uses a simple shader to copy a face of the input cube texture to an
// a output texture that matches the size of the input texture face
//
TEST_F(TextureCubeTest, Passthrough_SampleFromCube) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::newCube(TextureFormat::RGBA_UNorm8,
                                             kOffscreenTexWidth,
                                             kOffscreenTexHeight,
                                             TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);

  ASSERT_TRUE(inputTexture_
                  ->upload(rangeDesc.atFace(igl::TextureCubeFace::PosX), kBaseMipTextureFaceData[0])
                  .isOk());
  ASSERT_TRUE(inputTexture_
                  ->upload(rangeDesc.atFace(igl::TextureCubeFace::NegX), kBaseMipTextureFaceData[1])
                  .isOk());
  ASSERT_TRUE(inputTexture_
                  ->upload(rangeDesc.atFace(igl::TextureCubeFace::PosY), kBaseMipTextureFaceData[2])
                  .isOk());
  ASSERT_TRUE(inputTexture_
                  ->upload(rangeDesc.atFace(igl::TextureCubeFace::NegY), kBaseMipTextureFaceData[3])
                  .isOk());
  ASSERT_TRUE(inputTexture_
                  ->upload(rangeDesc.atFace(igl::TextureCubeFace::PosZ), kBaseMipTextureFaceData[4])
                  .isOk());
  ASSERT_TRUE(inputTexture_
                  ->upload(rangeDesc.atFace(igl::TextureCubeFace::NegZ), kBaseMipTextureFaceData[5])
                  .isOk());
  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  for (size_t face = 0; face < 6; ++face) {
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

    Result result{};
    auto vertUniformBuffer = createVertexUniformBuffer(*iglDev_.get(), &result);
    ASSERT_TRUE(result.isOk());

    vertexUniforms_.viewDirection = kViewDirection[face];

    *static_cast<VertexUniforms*>(vertUniformBuffer->getData()) = vertexUniforms_;
    vertUniformBuffer->bind(*iglDev_.get(), *pipelineState, *cmds.get());

    cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();

    //----------------
    // Validate output
    //----------------
    const auto faceStr = std::string("Face ") + std::to_string(face);
    util::validateFramebufferTexture(
        *iglDev_, *cmdQueue_, *framebuffer_, kBaseMipTextureFaceData[face], faceStr.c_str());
  }
}

//
// Texture Passthrough Test - Render To Cube
//
// This test uses a simple shader to copy a non-cube input texture to an
// a single face of the cube output texture. The size of the input texture matches the size of a
// single face in the output texture.
//
TEST_F(TextureCubeTest, Passthrough_RenderToCube) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //---------------------------------
  // Create input and output textures
  //---------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOffscreenTexWidth,
                                           kOffscreenTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  texDesc = TextureDesc::newCube(TextureFormat::RGBA_UNorm8,
                                 kOffscreenTexWidth,
                                 kOffscreenTexHeight,
                                 TextureDesc::TextureUsageBits::Sampled |
                                     TextureDesc::TextureUsageBits::Attachment);
  auto customOffscreenTexture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(customOffscreenTexture != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);
  const size_t bytesPerRow = kOffscreenTexWidth * 4;

  //--------------------------
  // Create custom framebuffer
  //--------------------------
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = customOffscreenTexture;
  auto customFramebuffer = iglDev_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(customFramebuffer != nullptr);

  //----------------------------
  // Create custom shader stages
  //----------------------------
  std::unique_ptr<IShaderStages> customStages;
  igl::tests::util::createSimpleShaderStages(iglDev_, customStages);
  renderPipelineDesc_.shaderStages = std::move(customStages);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  for (size_t face = 0; face < 6; ++face) {
    //------------------
    // Upload layer data
    //------------------
    ASSERT_TRUE(
        inputTexture_->upload(rangeDesc, kBaseMipTextureFaceData[face], bytesPerRow).isOk());

    //-------
    // Render
    //-------
    cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf_ != nullptr);

    renderPass_.colorAttachments[0].face = face;
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
  }

  // Validate in a separate loop to ensure all faces are already written
  for (size_t face = 0; face < 6; ++face) {
    //----------------
    // Validate output
    //----------------
    const auto faceStr = "Face " + std::to_string(face);
    util::validateFramebufferTextureRange(*iglDev_,
                                          *cmdQueue_,
                                          *customFramebuffer,
                                          customOffscreenTexture->getCubeFaceRange(face),
                                          kBaseMipTextureFaceData[face],
                                          faceStr.c_str());
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
// Test ITexture::getFullRange, ITexture::getFullMipRange, and ITexture::getCubeFaceRange and
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
  auto getFullMipRange = [&](size_t width,
                             size_t height,
                             TextureFormat format,
                             // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                             size_t numMipLevels) -> TextureRangeDesc {
    auto tex = createTexture(width, height, format, numMipLevels);
    return tex ? tex->getFullMipRange() : TextureRangeDesc{};
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
    return memcmp(&a, &b, sizeof(TextureRangeDesc)) == 0;
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

  // All mip levels
  ASSERT_TRUE(rangesAreEqual(getFullMipRange(16, 16, format, 5), range.withNumMipLevels(5)));
}

} // namespace igl::tests
