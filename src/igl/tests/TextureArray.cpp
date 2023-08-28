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
#include <igl/opengl/Device.h>
#include <string>

namespace igl {
namespace tests {

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
constexpr size_t OFFSCREEN_TEX_WIDTH = 2;
constexpr size_t OFFSCREEN_TEX_HEIGHT = 2;
constexpr size_t OFFSCREEN_SUBTEX_WIDTH = 1;
constexpr size_t OFFSCREEN_SUBTEX_HEIGHT = 1;

struct VertexUniforms {
  int layer = 0;
};
//
// TextureArrayTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class TextureArrayTest : public ::testing::Test {
 private:
 public:
  TextureArrayTest() = default;
  ~TextureArrayTest() override = default;

  std::shared_ptr<iglu::ManagedUniformBuffer> createVertexUniformBuffer(igl::IDevice& device,
                                                                        igl::Result* /*result*/) {
    std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer = nullptr;

    iglu::ManagedUniformBufferInfo vertInfo = {
        .index = 2,
        .length = sizeof(VertexUniforms),
        .uniforms = {{.name = "layer",
                      .type = igl::UniformType::Int,
                      .offset = offsetof(VertexUniforms, layer)}}};

    vertUniformBuffer = std::make_shared<iglu::ManagedUniformBuffer>(device, vertInfo);

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
#if IGL_PLATFORM_LINUX && !IGL_PLATFORM_LINUX_USE_EGL
    GTEST_SKIP() << "Fix these tests on Linux";
#endif

    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    if (!iglDev_->hasFeature(DeviceFeatures::Texture2DArray)) {
      GTEST_SKIP() << "2D array texture is unsupported for this platform.";
      return;
    }

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
    if (iglDev_->hasFeature(DeviceFeatures::Texture2DArray)) {
      if (iglDev_->getBackendType() == BackendType::OpenGL) {
#if IGL_BACKEND_OPENGL
        if (opengl::DeviceFeatureSet::usesOpenGLES()) {
          util::createShaderStages(iglDev_,
                                   igl::tests::data::shader::OGL_SIMPLE_VERT_SHADER_TEXARRAY_ES3,
                                   igl::tests::data::shader::shaderFunc,
                                   igl::tests::data::shader::OGL_SIMPLE_FRAG_SHADER_TEXARRAY_ES3,
                                   igl::tests::data::shader::shaderFunc,
                                   stages);
        } else {
          if (!iglDev_->hasRequirement(DeviceRequirement::TextureArrayExtReq)) {
            util::createShaderStages(iglDev_,
                                     igl::tests::data::shader::OGL_SIMPLE_VERT_SHADER_TEXARRAY,
                                     igl::tests::data::shader::shaderFunc,
                                     igl::tests::data::shader::OGL_SIMPLE_FRAG_SHADER_TEXARRAY,
                                     igl::tests::data::shader::shaderFunc,
                                     stages);
          } else if (iglDev_->hasFeature(DeviceFeatures::TextureArrayExt)) {
            util::createShaderStages(iglDev_,
                                     igl::tests::data::shader::OGL_SIMPLE_VERT_SHADER_TEXARRAY_EXT,
                                     igl::tests::data::shader::shaderFunc,
                                     igl::tests::data::shader::OGL_SIMPLE_FRAG_SHADER_TEXARRAY_EXT,
                                     igl::tests::data::shader::shaderFunc,
                                     stages);
          }
        }
#endif // IGL_BACKEND_OPENGL
      } else if (iglDev_->getBackendType() == BackendType::Vulkan) {
        util::createShaderStages(iglDev_,
                                 igl::tests::data::shader::VULKAN_SIMPLE_VERT_SHADER_TEX_2DARRAY,
                                 igl::tests::data::shader::shaderFunc,
                                 igl::tests::data::shader::VULKAN_SIMPLE_FRAG_SHADER_TEX_2DARRAY,
                                 igl::tests::data::shader::shaderFunc,
                                 stages);
      } else if (iglDev_->getBackendType() == BackendType::Metal) {
        util::createShaderStages(iglDev_,
                                 igl::tests::data::shader::MTL_SIMPLE_SHADER_TXT_2D_ARRAY,
                                 igl::tests::data::shader::simpleVertFunc,
                                 igl::tests::data::shader::simpleFragFunc,
                                 stages);
      }
    }

    ASSERT_TRUE(stages != nullptr);

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

  void runPassthroughTest(bool uploadFullArray, bool modifyTexture);

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

static uint32_t R = 0x1F00000F;
static uint32_t G = 0x002F001F;
static uint32_t B = 0x00003F2F;
static uint32_t C = 0x004F5F3F;
static uint32_t M = 0x6F007F4F;
static uint32_t Y = 0x8F9F005F;

constexpr size_t kNumLayers = 3;

static uint32_t textureArray2D[kNumLayers][OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT] = {
    {R, R, R, R},
    {G, G, G, G},
    {B, B, B, B}};
static uint32_t textureSubArray2D[kNumLayers][OFFSCREEN_SUBTEX_WIDTH * OFFSCREEN_SUBTEX_HEIGHT] = {
    {C},
    {M},
    {Y}};
static uint32_t modifiedTextureArray2D[kNumLayers][OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT] = {
    {R, R, R, C},
    {G, G, G, M},
    {B, B, B, Y}};

//
// Texture Passthrough Test
//
// This test uses a simple shader to copy the input texture to a same
// sized output texture (offscreenTexture_)
//
void TextureArrayTest::runPassthroughTest(bool uploadFullArray, bool modifyTexture) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::new2DArray(TextureFormat::RGBA_UNorm8,
                                                OFFSCREEN_TEX_WIDTH,
                                                OFFSCREEN_TEX_HEIGHT,
                                                kNumLayers,
                                                TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  //
  // upload and redownload to make sure that we've uploaded successfully.
  //
  if (uploadFullArray) {
    const auto uploadRange = TextureRangeDesc::new2DArray(
        0, 0, OFFSCREEN_TEX_WIDTH, OFFSCREEN_TEX_HEIGHT, 0, kNumLayers);
    ASSERT_TRUE(inputTexture_->upload(uploadRange, textureArray2D[0]).isOk());
  } else {
    for (int j = 0; j < kNumLayers; ++j) {
      const auto uploadRange =
          TextureRangeDesc::new2DArray(0, 0, OFFSCREEN_TEX_WIDTH, OFFSCREEN_TEX_HEIGHT, j, 1);
      ASSERT_TRUE(inputTexture_->upload(uploadRange, textureArray2D[j]).isOk());
    }
  }

  if (modifyTexture) {
    if (uploadFullArray) {
      const auto uploadRange =
          TextureRangeDesc::new2DArray(OFFSCREEN_TEX_WIDTH - OFFSCREEN_SUBTEX_WIDTH,
                                       OFFSCREEN_TEX_HEIGHT - OFFSCREEN_SUBTEX_HEIGHT,
                                       OFFSCREEN_SUBTEX_WIDTH,
                                       OFFSCREEN_SUBTEX_HEIGHT,
                                       0,
                                       kNumLayers);
      ASSERT_TRUE(inputTexture_->upload(uploadRange, textureSubArray2D[0]).isOk());
    } else {
      for (int j = 0; j < kNumLayers; ++j) {
        const auto uploadRange =
            TextureRangeDesc::new2DArray(OFFSCREEN_TEX_WIDTH - OFFSCREEN_SUBTEX_WIDTH,
                                         OFFSCREEN_TEX_HEIGHT - OFFSCREEN_SUBTEX_HEIGHT,
                                         OFFSCREEN_SUBTEX_WIDTH,
                                         OFFSCREEN_SUBTEX_HEIGHT,
                                         j,
                                         1);
        ASSERT_TRUE(inputTexture_->upload(uploadRange, textureSubArray2D[j]).isOk());
      }
    }
  }
  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  for (int i = 0; i < kNumLayers; ++i) {
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

    vertexUniforms_.layer = i;

    *static_cast<VertexUniforms*>(vertUniformBuffer->getData()) = vertexUniforms_;
    vertUniformBuffer->bind(*iglDev_.get(), *pipelineState, *cmds.get());

    cmds->drawIndexed(PrimitiveType::Triangle, 6, IndexFormat::UInt16, *ib_, 0);

    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();

    //----------------------
    // Read back framebuffer
    //----------------------
    auto pixels = std::vector<uint32_t>(OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT);

    const auto downloadRange =
        TextureRangeDesc::new2D(0, 0, OFFSCREEN_TEX_WIDTH, OFFSCREEN_TEX_HEIGHT);
    framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), downloadRange);

    //--------------------------------
    // Verify against original texture
    //--------------------------------
    for (size_t j = 0; j < OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT; j++) {
      if (modifyTexture) {
        ASSERT_EQ(pixels[j], modifiedTextureArray2D[i][j]);
      } else {
        ASSERT_EQ(pixels[j], textureArray2D[i][j]);
      }
    }
  }
}

TEST_F(TextureArrayTest, Passthrough_FullRange) {
  runPassthroughTest(true, false);
}

TEST_F(TextureArrayTest, Passthrough_LayerByLayer) {
  runPassthroughTest(false, false);
}

TEST_F(TextureArrayTest, Passthrough_FullRange_ModifySubTexture) {
  runPassthroughTest(true, true);
}

TEST_F(TextureArrayTest, Passthrough_LayerByLayer_ModifySubTexture) {
  runPassthroughTest(false, true);
}

TEST_F(TextureArrayTest, ValidateRange2DArray) {
  if (!iglDev_->hasFeature(DeviceFeatures::Texture2DArray)) {
    GTEST_SKIP() << "2D array textures not supported. Skipping.";
  }

  Result ret;
  bool fullRange;
  auto texDesc = TextureDesc::new2DArray(
      TextureFormat::RGBA_UNorm8, 8, 8, 2, TextureDesc::TextureUsageBits::Sampled);
  auto tex = iglDev_->createTexture(texDesc, &ret);

  std::tie(ret, fullRange) = tex->validateRange(TextureRangeDesc::new2DArray(0, 0, 8, 8, 0, 2));
  EXPECT_TRUE(ret.isOk());
  EXPECT_TRUE(fullRange);

  std::tie(ret, fullRange) = tex->validateRange(TextureRangeDesc::new2DArray(4, 4, 4, 4, 1, 1));
  EXPECT_TRUE(ret.isOk());
  EXPECT_FALSE(fullRange);

  std::tie(ret, fullRange) = tex->validateRange(TextureRangeDesc::new2DArray(0, 0, 4, 4, 0, 2, 1));
  EXPECT_FALSE(ret.isOk());

  std::tie(ret, fullRange) = tex->validateRange(TextureRangeDesc::new2DArray(0, 0, 12, 12, 0, 3));
  EXPECT_FALSE(ret.isOk());

  std::tie(ret, fullRange) = tex->validateRange(TextureRangeDesc::new2DArray(0, 0, 0, 0, 0, 0));
  EXPECT_FALSE(ret.isOk());
}
} // namespace tests
} // namespace igl
