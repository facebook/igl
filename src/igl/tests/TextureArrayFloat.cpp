/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// obscure compiler error where glm::vec would delete it's construct_at
// since Half didnt have a trivial default ctor despite specifying it.
#if defined(IGL_CMAKE_BUILD)
#define GLM_FORCE_XYZW_ONLY 1
#endif

#include "data/ShaderData.h"
#include "data/TextureData.h"
#include "data/VertexIndexData.h"
#include "util/Color.h"
#include "util/Common.h"
#include "util/Half.h"
#include "util/TestDevice.h"
#include "util/TextureValidationHelpers.h"

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>
#include <array>
#include <cstring>
#include <glm/gtc/color_space.hpp>
#include <gtest/gtest.h>
#include <igl/NameHandle.h>
#include <string>

namespace igl::tests {

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
constexpr size_t kOffscreenTexWidth = 2;
constexpr size_t kOffscreenTexHeight = 2;
constexpr size_t kOffscreenSubTexWidth = 1;
constexpr size_t kOffscreenSubTexHeight = 1;

constexpr igl::TextureFormat kFloatTextureFormat = TextureFormat::RGBA_F16;
using TestColorType = glm::vec<4, util::TestHalf, glm::defaultp>; // control
// using TestColorType = glm::vec<4, float, glm::defaultp>;

struct VertexUniforms {
  int layer = 0;
};
//
// TextureFloatTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class TextureArrayFloatTest : public ::testing::Test {
 private:
 public:
  TextureArrayFloatTest() = default;
  ~TextureArrayFloatTest() override = default;

  std::shared_ptr<iglu::ManagedUniformBuffer> createVertexUniformBuffer(IDevice& device,
                                                                        Result* /*result*/) {
    std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer = nullptr;

    const iglu::ManagedUniformBufferInfo vertInfo = {
        .index = 2,
        .length = sizeof(VertexUniforms),
        .uniforms = {
            UniformDesc{
                .name = "layer",
                .type = igl::UniformType::Int,
                .offset = offsetof(VertexUniforms, layer),
            },
        }};

    vertUniformBuffer = std::make_shared<iglu::ManagedUniformBuffer>(device, vertInfo);

    IGL_DEBUG_ASSERT(vertUniformBuffer->result.isOk());
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

    if (!iglDev_->hasFeature(DeviceFeatures::TextureHalfFloat) ||
        !iglDev_->hasFeature(DeviceFeatures::Texture2DArray)) {
      GTEST_SKIP() << "2D float texture array is unsupported for this platform.";
      return;
    }

    // Create an offscreen texture to render to
    const TextureDesc texDesc = TextureDesc::new2D(kFloatTextureFormat,
                                                   kOffscreenTexWidth,
                                                   kOffscreenTexHeight,
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
        } else {
          GTEST_SKIP() << "Texture array is unsupported for this platform.";
          return;
        }
      }
#if defined(IGL_PLATFORM_LINUX) && IGL_PLATFORM_LINUX
      GTEST_SKIP() << "Temporarily disabled.";
#endif
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

// clang-format off
const TestColorType kR = igl::tests::util::convertSRGBToLinear(glm::vec4(0x1F / 255.0, 0x00/255.0, 0x00/255.0, 0x0F/255.0)); // 0x1F00000F
const TestColorType kG = igl::tests::util::convertSRGBToLinear(glm::vec4(0x00 / 255.0, 0x2F/255.0, 0x00/255.0, 0x1F/255.0)); // 0x002F001F;
const TestColorType kB = igl::tests::util::convertSRGBToLinear(glm::vec4(0x00 / 255.0, 0x00/255.0, 0x3F/255.0, 0x2F/255.0)); // 0x00003F2F;
const TestColorType kC = igl::tests::util::convertSRGBToLinear(glm::vec4(0x00 / 255.0, 0x4F/255.0, 0x5F/255.0, 0x3F/255.0)); // 0x004F5F3F;
const TestColorType kM = igl::tests::util::convertSRGBToLinear(glm::vec4(0x6F / 255.0, 0x00/255.0, 0x7F/255.0, 0x4F/255.0)); // 0x6F007F4F;
const TestColorType kY = igl::tests::util::convertSRGBToLinear(glm::vec4(0x8F / 255.0, 0x9F/255.0, 0x00/255.0, 0x5F/255.0)); // 0x8F9F005F;

constexpr size_t kNumLayers = 3;

const std::array<TestColorType, 15> kTextureData = {
  kR, kR, kR, kR, // Base Mip, Layer 0
  kG, kG, kG, kG, // Base Mip, Layer 1
  kB, kB, kB, kB, // Base Mip, Layer 2
  kC,             // Mip 1, Layer 0
  kM,             // Mip 1, Layer 1
  kY,             // Mip 1, Layer 2
};

const std::array<TestColorType, 3> kSubTextureData = {
  kC,             // Layer 0
  kM,             // Layer 1
  kY,             // Layer 2
};

const std::array<TestColorType, 12> kModifiedTextureData = {
  kR, kR, kR, kC, // Layer 0
  kG, kG, kG, kM, // Layer 1
  kB, kB, kB, kY, // Layer 2
};
// clang-format on

const std::array<const TestColorType*, kNumLayers> kTextureLayerData = {
    kTextureData.data() + 0,
    kTextureData.data() + 4,
    kTextureData.data() + 8,
};

const std::array<const TestColorType*, kNumLayers> kSubTextureLayerData = {
    kSubTextureData.data() + 0,
    kSubTextureData.data() + 1,
    kSubTextureData.data() + 2,
};

const std::array<const TestColorType*, kNumLayers> kModifiedTextureLayerData = {
    kModifiedTextureData.data() + 0,
    kModifiedTextureData.data() + 4,
    kModifiedTextureData.data() + 8,
};

//
// Texture Upload Test
//
// This test uploads data to an array texture and then downloads it again to validate it
//
namespace {
void runUploadTest(IDevice& device,
                   ICommandQueue& cmdQueue,
                   bool singleUpload,
                   bool modifyTexture) {
  Result ret;
  const std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  const TextureDesc texDesc = TextureDesc::new2DArray(
      kFloatTextureFormat,
      kOffscreenTexWidth,
      kOffscreenTexHeight,
      kNumLayers,
      TextureDesc::TextureUsageBits::Sampled | TextureDesc::TextureUsageBits::Attachment);
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(tex != nullptr);

  //
  // upload and redownload to make sure that we've uploaded successfully.
  //
  if (singleUpload) {
    const auto uploadRange =
        TextureRangeDesc::new2DArray(0, 0, kOffscreenTexWidth, kOffscreenTexHeight, 0, kNumLayers);
    ASSERT_TRUE(tex->upload(uploadRange, kTextureData.data()).isOk());
  } else {
    for (size_t layer = 0; layer < kNumLayers; ++layer) {
      const auto uploadRange =
          TextureRangeDesc::new2DArray(0, 0, kOffscreenTexWidth, kOffscreenTexHeight, layer, 1);
      ASSERT_TRUE(tex->upload(uploadRange, kTextureLayerData[layer]).isOk());
    }
  }

  if (modifyTexture) {
    if (singleUpload) {
      const auto uploadRange =
          TextureRangeDesc::new2DArray(kOffscreenTexWidth - kOffscreenSubTexWidth,
                                       kOffscreenTexHeight - kOffscreenSubTexHeight,
                                       kOffscreenSubTexWidth,
                                       kOffscreenSubTexHeight,
                                       0,
                                       kNumLayers);
      ASSERT_TRUE(tex->upload(uploadRange, kSubTextureData.data()).isOk());
    } else {
      for (size_t layer = 0; layer < kNumLayers; ++layer) {
        const auto uploadRange =
            TextureRangeDesc::new2DArray(kOffscreenTexWidth - kOffscreenSubTexWidth,
                                         kOffscreenTexHeight - kOffscreenSubTexHeight,
                                         kOffscreenSubTexWidth,
                                         kOffscreenSubTexHeight,
                                         layer,
                                         1);
        ASSERT_TRUE(tex->upload(uploadRange, kSubTextureLayerData[layer]).isOk());
      }
    }
  }

  for (size_t layer = 0; layer < kNumLayers; ++layer) {
    //--------------------------------
    // Verify against original texture
    //--------------------------------
    const auto layerStr = "Layer " + std::to_string(layer);
    const auto* data = modifyTexture ? kModifiedTextureLayerData[layer] : kTextureLayerData[layer];
    util::validateUploadedTextureRange(
        device, cmdQueue, tex, tex->getLayerRange(layer), data, layerStr.c_str());
  }
}
} // namespace

TEST_F(TextureArrayFloatTest, Upload_SingleUpload) {
  runUploadTest(*iglDev_, *cmdQueue_, true, false);
}

TEST_F(TextureArrayFloatTest, Upload_LayerByLayer) {
  runUploadTest(*iglDev_, *cmdQueue_, false, false);
}

TEST_F(TextureArrayFloatTest, Upload_SingleUpload_ModifySubTexture) {
  runUploadTest(*iglDev_, *cmdQueue_, true, true);
}

TEST_F(TextureArrayFloatTest, Upload_LayerByLayer_ModifySubTexture) {
  runUploadTest(*iglDev_, *cmdQueue_, false, true);
}

namespace {
void runUploadToMipTest(IDevice& device, ICommandQueue& cmdQueue, bool singleUpload) {
  Result ret;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc texDesc = TextureDesc::new2DArray(kFloatTextureFormat,
                                                kOffscreenTexWidth,
                                                kOffscreenTexHeight,
                                                kNumLayers,
                                                TextureDesc::TextureUsageBits::Sampled |
                                                    TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = 2;
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(tex != nullptr);

  //
  // upload and redownload to make sure that we've uploaded successfully.
  //
  if (singleUpload) {
    const auto uploadRange = TextureRangeDesc::new2DArray(
        0, 0, kOffscreenTexWidth, kOffscreenTexHeight, 0, kNumLayers, 0, 2);
    ASSERT_TRUE(tex->upload(uploadRange, kTextureData.data()).isOk());
  } else {
    for (size_t mipLevel = 0; mipLevel < 2; ++mipLevel) {
      for (size_t layer = 0; layer < kNumLayers; ++layer) {
        const auto uploadRange =
            TextureRangeDesc::new2DArray(0, 0, kOffscreenTexWidth, kOffscreenTexHeight, layer, 1)
                .atMipLevel(mipLevel);
        if (mipLevel == 0) {
          ASSERT_TRUE(tex->upload(uploadRange, kTextureLayerData[layer]).isOk());
        } else {
          ASSERT_TRUE(tex->upload(uploadRange, kSubTextureLayerData[layer]).isOk());
        }
      }
    }
  }

  for (size_t mipLevel = 0; mipLevel < 2; ++mipLevel) {
    for (size_t layer = 0; layer < kNumLayers; ++layer) {
      //--------------------------------
      // Verify against original texture
      //--------------------------------
      const auto layerStr =
          "Mip Level " + std::to_string(mipLevel) + "; Layer " + std::to_string(layer);
      const auto* data = mipLevel == 0 ? kTextureLayerData[layer] : kSubTextureLayerData[layer];
      util::validateUploadedTextureRange(
          device, cmdQueue, tex, tex->getLayerRange(layer, mipLevel), data, layerStr.c_str());
    }
  }
}
} // namespace

TEST_F(TextureArrayFloatTest, UploadToMip_SingleUpload) {
  runUploadToMipTest(*iglDev_, *cmdQueue_, true);
}

TEST_F(TextureArrayFloatTest, UploadToMip_LayerByLayer) {
  runUploadToMipTest(*iglDev_, *cmdQueue_, false);
}

//
// Texture Passthrough Test - Sample From Array
//
// This test uses a simple shader to copy a layer of the input array texture to an
// a output texture that matches the size of the input texture layer
//
TEST_F(TextureArrayFloatTest, Passthrough_SampleFromArray) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  const TextureDesc texDesc = TextureDesc::new2DArray(kFloatTextureFormat,
                                                      kOffscreenTexWidth,
                                                      kOffscreenTexHeight,
                                                      kNumLayers,
                                                      TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);
  const size_t bytesPerRow = kOffscreenTexWidth * sizeof(TestColorType);

  //
  // upload and redownload to make sure that we've uploaded successfully.
  //
  for (size_t layer = 0; layer < kNumLayers; ++layer) {
    ASSERT_TRUE(
        inputTexture_->upload(rangeDesc.atLayer(layer), kTextureLayerData[layer], bytesPerRow)
            .isOk());
  }

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  for (size_t layer = 0; layer < kNumLayers; ++layer) {
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

    Result result{};
    auto vertUniformBuffer = createVertexUniformBuffer(*iglDev_, &result);
    ASSERT_TRUE(result.isOk());

    vertexUniforms_.layer = static_cast<int>(layer);

    *static_cast<VertexUniforms*>(vertUniformBuffer->getData()) = vertexUniforms_;
    vertUniformBuffer->bind(*iglDev_, *pipelineState, *cmds);

    cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
    cmds->drawIndexed(6);

    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();

    //----------------
    // Validate output
    //----------------
    const auto layerStr = "Layer " + std::to_string(layer);
    const auto* data = kTextureLayerData[layer];
    util::validateFramebufferTexture(*iglDev_, *cmdQueue_, *framebuffer_, data, layerStr.c_str());
  }
}

//
// Texture Passthrough Test - Render To Array
//
// This test uses a simple shader to copy a non-array input texture to an
// a single layer of the array output texture. The size of the input texture matches the size of a
// single layer in the output texture.
//
TEST_F(TextureArrayFloatTest, Passthrough_RenderToArray) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  //---------------------------------
  // Create input and output textures
  //---------------------------------
  TextureDesc texDesc = TextureDesc::new2D(kFloatTextureFormat,
                                           kOffscreenTexWidth,
                                           kOffscreenTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(inputTexture_ != nullptr);

  texDesc = TextureDesc::new2DArray(kFloatTextureFormat,
                                    kOffscreenTexWidth,
                                    kOffscreenTexHeight,
                                    kNumLayers,
                                    TextureDesc::TextureUsageBits::Attachment);
  auto customOffscreenTexture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(customOffscreenTexture != nullptr);

  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);
  const size_t bytesPerRow = kOffscreenTexWidth * sizeof(TestColorType);

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

  for (size_t layer = 0; layer < kNumLayers; ++layer) {
    //------------------
    // Upload layer data
    //------------------
    ASSERT_TRUE(inputTexture_->upload(rangeDesc, kTextureLayerData[layer], bytesPerRow).isOk());

    //-------
    // Render
    //-------
    cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf_ != nullptr);

    renderPass_.colorAttachments[0].layer = layer;
    auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, customFramebuffer);
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
  }

  // Validate in a separate loop to ensure all layers are already written
  for (size_t layer = 0; layer < kNumLayers; ++layer) {
    //----------------
    // Validate output
    //----------------
    const auto layerStr = "Layer " + std::to_string(layer);
    const auto* data = kTextureLayerData[layer];
    util::validateFramebufferTextureRange(*iglDev_,
                                          *cmdQueue_,
                                          *customFramebuffer,
                                          customOffscreenTexture->getLayerRange(layer),
                                          data,
                                          layerStr.c_str());
  }
}

TEST_F(TextureArrayFloatTest, ValidateRange2DArray) {
  Result ret;
  auto texDesc =
      TextureDesc::new2DArray(kFloatTextureFormat, 8, 8, 2, TextureDesc::TextureUsageBits::Sampled);
  auto tex = iglDev_->createTexture(texDesc, &ret);

  ret = tex->validateRange(TextureRangeDesc::new2DArray(0, 0, 8, 8, 0, 2));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2DArray(4, 4, 4, 4, 1, 1));
  EXPECT_TRUE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2DArray(0, 0, 4, 4, 0, 2, 1));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2DArray(0, 0, 12, 12, 0, 3));
  EXPECT_FALSE(ret.isOk());

  ret = tex->validateRange(TextureRangeDesc::new2DArray(0, 0, 0, 0, 0, 0));
  EXPECT_FALSE(ret.isOk());
}

//
// Test ITexture::getEstimatedSizeInBytes
//
TEST_F(TextureArrayFloatTest, GetEstimatedSizeInBytes) {
  auto calcSize =
      [&](size_t width, size_t height, TextureFormat format, size_t numMipLevels) -> size_t {
    Result ret;
    TextureDesc texDesc =
        TextureDesc::new2DArray(format, width, height, 2, TextureDesc::TextureUsageBits::Sampled);
    texDesc.numMipLevels = numMipLevels;
    auto texture = iglDev_->createTexture(texDesc, &ret);
    if (ret.code != Result::Code::Ok || texture == nullptr) {
      return 0;
    }
    return texture->getEstimatedSizeInBytes();
  };

  const auto format = kFloatTextureFormat;
  const uint32_t formatBytes = (kFloatTextureFormat == TextureFormat::RGBA_F16) ? 8u : 16u;

  uint32_t bytes = 0;
  bytes = 12u * 34u * formatBytes * 2u;
  ASSERT_EQ(calcSize(12, 34, format, 1), bytes);
  bytes = (16u + 8u + 4u + 2u + 1u) * formatBytes * 2u;
  ASSERT_EQ(calcSize(16, 1, format, 5), bytes);

  if (iglDev_->hasFeature(DeviceFeatures::TextureNotPot)) {
    if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
      // ES 2.0 generates maximum mip levels
      bytes = (128u * 333u + 64u * 166u + 32u * 83u + 16u * 41u + 8u * 20u + 4u * 10u + 2u * 5u +
               1u * 2u + 1u * 1u) *
              formatBytes * 2u;
      ASSERT_EQ(calcSize(128, 333, format, 9), bytes);
    } else {
      bytes = (128u * 333u + 64u * 166u) * formatBytes * 2u;
      ASSERT_EQ(calcSize(128, 333, format, 2), bytes);
    }

    if (iglDev_->hasFeature(DeviceFeatures::TextureFormatRG)) {
      constexpr size_t rBytes = 1u;
      constexpr size_t rgBytes = 2u;
      bytes = (16 + 8 + 4 + 2 + 1) * rBytes * 2u;
      ASSERT_EQ(calcSize(16, 1, TextureFormat::R_UNorm8, 5), bytes);
      if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
        // ES 2.0 generates maximum mip levels
        bytes = (128u * 333u + 64u * 166u + 32u * 83u + 16u * 41u + 8u * 20u + 4u * 10u + 2u * 5u +
                 1u * 2u + 1u * 1u) *
                rgBytes * 2u;
        ASSERT_EQ(calcSize(128, 333, TextureFormat::RG_UNorm8, 9), bytes);
      } else {
        bytes = (128u * 333u + 64u * 166u) * rgBytes * 2u;
        ASSERT_EQ(calcSize(128, 333, TextureFormat::RG_UNorm8, 2), bytes);
      }
    }
  }
}

//
// Test ITexture::getFullRange ITexture::getFullMipRange, and ITexture::getLayerRange
//
TEST_F(TextureArrayFloatTest, GetRange) {
  auto createTexture = [&](size_t width,
                           size_t height,
                           TextureFormat format,
                           size_t numMipLevels) -> std::shared_ptr<ITexture> {
    Result ret;
    TextureDesc texDesc =
        TextureDesc::new2DArray(format, width, height, 2, TextureDesc::TextureUsageBits::Sampled);
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
  auto getLayerRange = [&](size_t width,
                           size_t height,
                           TextureFormat format,
                           // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                           size_t numMipLevels,
                           size_t layer,
                           size_t rangeMipLevel = 0,
                           size_t rangeNumMipLevels = 0) -> TextureRangeDesc {
    auto tex = createTexture(width, height, format, numMipLevels);
    return tex ? tex->getLayerRange(
                     layer, rangeMipLevel, rangeNumMipLevels ? rangeNumMipLevels : numMipLevels)
               : TextureRangeDesc{};
  };
  auto rangesAreEqual = [&](const TextureRangeDesc& a, const TextureRangeDesc& b) -> bool {
    return std::memcmp(&a, &b, sizeof(TextureRangeDesc)) == 0;
  };
  const auto format = kFloatTextureFormat;

  TextureRangeDesc range;
  range = TextureRangeDesc::new2DArray(0, 0, 12, 34, 0, 2, 0, 1);
  ASSERT_TRUE(rangesAreEqual(getFullRange(12, 34, format, 1), range));
  ASSERT_TRUE(rangesAreEqual(getLayerRange(12, 34, format, 1, 1), range.atLayer(1)));
  range = TextureRangeDesc::new2DArray(0, 0, 16, 1, 0, 2, 0, 4);
  ASSERT_TRUE(rangesAreEqual(getFullRange(16, 1, format, 4), range));
  ASSERT_TRUE(rangesAreEqual(getLayerRange(16, 1, format, 4, 1), range.atLayer(1)));

  // Test subset of mip levels
  ASSERT_TRUE(rangesAreEqual(getFullRange(16, 1, format, 4, 1, 1), range.atMipLevel(1)));
  ASSERT_TRUE(
      rangesAreEqual(getLayerRange(16, 1, format, 4, 1, 1, 1), range.atMipLevel(1).atLayer(1)));

  // Test all mip levels
  ASSERT_TRUE(rangesAreEqual(getFullMipRange(16, 1, format, 4), range.withNumMipLevels(4)));

  if (iglDev_->hasFeature(DeviceFeatures::TextureNotPot)) {
    if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
      // ES 2.0 generates maximum mip levels
      range = TextureRangeDesc::new2DArray(0, 0, 128, 333, 0, 2, 0, 9);
      ASSERT_TRUE(rangesAreEqual(getFullRange(128, 333, format, 9), range));
      ASSERT_TRUE(rangesAreEqual(getLayerRange(128, 333, format, 9, 1), range.atLayer(1)));

      // Test all mip levels
      ASSERT_TRUE(rangesAreEqual(getFullMipRange(128, 333, format, 9), range.withNumMipLevels(9)));
    } else {
      range = TextureRangeDesc::new2DArray(0, 0, 128, 333, 0, 2, 0, 2);
      ASSERT_TRUE(rangesAreEqual(getFullRange(128, 333, format, 2), range));
      ASSERT_TRUE(rangesAreEqual(getLayerRange(128, 333, format, 2, 1), range.atLayer(1)));

      // Test all mip levels
      ASSERT_TRUE(rangesAreEqual(getFullMipRange(128, 333, format, 2), range.withNumMipLevels(2)));
    }
  }
}

} // namespace igl::tests
