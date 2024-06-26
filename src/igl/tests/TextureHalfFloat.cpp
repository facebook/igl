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
#include <glm/glm.hpp>
#include <glm/gtc/color_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/Macros.h>
#include <igl/NameHandle.h>
#include <string>

namespace igl::tests {
#if IGL_OPENGL_ES
static const bool kUsesOpenGLES = opengl::DeviceFeatureSet::usesOpenGLES();
#else
// no OpenGLES was linked
static const bool kUsesOpenGLES = false;
#endif

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
constexpr size_t kOffscreenTexWidth = 2;
constexpr size_t kOffscreenTexHeight = 2;

// clang-format off
const glm::vec4 kR = igl::tests::util::convertSRGBToLinear(glm::vec4(0x1F / 255.0, 0x00/255.0, 0x00/255.0, 0x0F/255.0)); // 0x1F00000F
const glm::vec4 kG = igl::tests::util::convertSRGBToLinear(glm::vec4(0x00 / 255.0, 0x2F/255.0, 0x00/255.0, 0x1F/255.0)); // 0x002F001F;
const glm::vec4 kB = igl::tests::util::convertSRGBToLinear(glm::vec4(0x00 / 255.0, 0x00/255.0, 0x3F/255.0, 0x2F/255.0)); // 0x00003F2F;
const glm::vec4 kC = igl::tests::util::convertSRGBToLinear(glm::vec4(0x00 / 255.0, 0x4F/255.0, 0x5F/255.0, 0x3F/255.0)); // 0x004F5F3F;
const glm::vec4 kM = igl::tests::util::convertSRGBToLinear(glm::vec4(0x6F / 255.0, 0x00/255.0, 0x7F/255.0, 0x4F/255.0)); // 0x6F007F4F;
const glm::vec4 kY = igl::tests::util::convertSRGBToLinear(glm::vec4(0x8F / 255.0, 0x9F/255.0, 0x00/255.0, 0x5F/255.0)); // 0x8F9F005F;

using TestColorHalf = glm::vec<4, util::TestHalf, glm::defaultp>;
using TestColor2Half = glm::vec<2, util::TestHalf, glm::defaultp>;
using TestColor3Half = glm::vec<3, util::TestHalf, glm::defaultp>;
const std::array<TestColorHalf, 15> kTextureDataRGBAHalf = {
  kR, kR, kR, kR, // Base Mip, Layer 0
  kG, kG, kG, kG, // Base Mip, Layer 1
  kB, kB, kB, kB, // Base Mip, Layer 2
  kC,             // Mip 1, Layer 0
  kM,             // Mip 1, Layer 1
  kY,             // Mip 1, Layer 2
};

const std::array<TestColor3Half, 15> kTextureDataRGBHalf = {
  glm::vec3(kR), glm::vec3(kR), glm::vec3(kR), glm::vec3(kR), // Base Mip, Layer 0
  glm::vec3(kG), glm::vec3(kG), glm::vec3(kG), glm::vec3(kG), // Base Mip, Layer 1
  glm::vec3(kB), glm::vec3(kB), glm::vec3(kB), glm::vec3(kB), // Base Mip, Layer 2
  glm::vec3(kC),             // Mip 1, Layer 0
  glm::vec3(kM),             // Mip 1, Layer 1
  glm::vec3(kY),             // Mip 1, Layer 2
};

const std::array<TestColor2Half, 15> kTextureDataRGHalf = {
  glm::vec2(kR), glm::vec2(kR), glm::vec2(kR), glm::vec2(kR), // Base Mip, Layer 0
  glm::vec2(kG), glm::vec2(kG), glm::vec2(kG), glm::vec2(kG), // Base Mip, Layer 1
  glm::vec2(kB), glm::vec2(kB), glm::vec2(kB), glm::vec2(kB), // Base Mip, Layer 2
  glm::vec2(kC),             // Mip 1, Layer 0
  glm::vec2(kM),             // Mip 1, Layer 1
  glm::vec2(kY),             // Mip 1, Layer 2
};


const std::array<util::TestHalf, 15> kTextureDataRHalf = {
  util::TestHalf(kR.y), util::TestHalf(kR.y), util::TestHalf(kR.y), util::TestHalf(kR.y), // Base Mip, Layer 0
  util::TestHalf(kG.y), util::TestHalf(kG.y), util::TestHalf(kG.y), util::TestHalf(kG.y), // Base Mip, Layer 1
  util::TestHalf(kB.y), util::TestHalf(kB.y), util::TestHalf(kB.y), util::TestHalf(kB.y), // Base Mip, Layer 2
  util::TestHalf(kC.y),             // Mip 1, Layer 0
  util::TestHalf(kM.y),             // Mip 1, Layer 1
  util::TestHalf(kY.y),             // Mip 1, Layer 2
};
// clang-format on

struct VertexUniforms {
  int layer = 0;
};
//
// TextureHalfFloatTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class TextureHalfFloatTest : public ::testing::Test {
 private:
 public:
  TextureHalfFloatTest() = default;
  ~TextureHalfFloatTest() override = default;

  std::shared_ptr<iglu::ManagedUniformBuffer> createVertexUniformBuffer(igl::IDevice& device,
                                                                        igl::Result* /*result*/) {
    std::shared_ptr<iglu::ManagedUniformBuffer> vertUniformBuffer = nullptr;

    const iglu::ManagedUniformBufferInfo vertInfo = {
        .index = 2,
        .length = sizeof(VertexUniforms),
        .uniforms = {
            igl::UniformDesc{
                .name = "layer",
                .type = igl::UniformType::Int,
                .offset = offsetof(VertexUniforms, layer),
            },
        }};

    vertUniformBuffer = std::make_shared<iglu::ManagedUniformBuffer>(device, vertInfo);

    IGL_ASSERT(vertUniformBuffer->result.isOk());
    return vertUniformBuffer;
  }

  void createPassthroughFrameBuffer(igl::TextureFormat format) {
    // Create an offscreen texture to render to
    TextureDesc const texDesc = TextureDesc::new2D(
        format, kOffscreenTexWidth, kOffscreenTexHeight, TextureDesc::TextureUsageBits::Attachment);

    Result ret;
    offscreenTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok)
        << "RetCode: " << static_cast<int>(ret.code) << " Message: " << ret.message;
    ASSERT_TRUE(offscreenTexture_ != nullptr);
    ASSERT_TRUE(offscreenTexture_->getFormat() == format);

    // Create framebuffer using the offscreen texture
    FramebufferDesc framebufferDesc;

    framebufferDesc.colorAttachments[0].texture = offscreenTexture_;
    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(framebuffer_ != nullptr);
    ASSERT_TRUE(offscreenTexture_->getFormat() == format);
  }

  void createShaderStages() {
    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    igl::tests::util::createSimpleShaderStages(iglDev_, stages, offscreenTexture_->getFormat());

    ASSERT_TRUE(stages != nullptr);

    shaderStages_ = std::move(stages);
  }

  void initializeRenderPipeline() {
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

    if (!iglDev_->hasFeature(DeviceFeatures::TextureHalfFloat)) {
      GTEST_SKIP() << "2D half float texture is unsupported for this platform.";
      return;
    }

    Result ret;
    createPassthroughFrameBuffer(igl::TextureFormat::RGBA_F16);

    // Initialize render pass descriptor
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

    createShaderStages();

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
    SamplerStateDesc const samplerDesc;
    samp_ = iglDev_->createSamplerState(samplerDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(samp_ != nullptr);

    initializeRenderPipeline();
  }

  void TearDown() override {}
  template<typename ColorType>
  void runPassthroughFormat(igl::TextureFormat format, const ColorType* data) {
    std::shared_ptr<IRenderPipelineState> pipelineState;
    createPassthroughFrameBuffer(format);
    ASSERT_TRUE(offscreenTexture_ != nullptr);
    ASSERT_TRUE(offscreenTexture_->getFormat() == format);
    createShaderStages();
    initializeRenderPipeline();

    Result ret;
    //-------------------------------------
    // Create input texture and upload data
    //-------------------------------------
    const TextureDesc texDesc = TextureDesc::new2D(
        format, kOffscreenTexWidth, kOffscreenTexHeight, TextureDesc::TextureUsageBits::Sampled);
    inputTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(inputTexture_ != nullptr);

    const auto rangeDesc = TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight);
    const size_t bytesPerRow = kOffscreenTexWidth * inputTexture_->getProperties().bytesPerBlock;

    //
    // upload and redownload to make sure that we've uploaded successfully.
    //
    ASSERT_TRUE(inputTexture_->upload(rangeDesc, data, bytesPerRow).isOk());

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

    Result result{};
    auto vertUniformBuffer = createVertexUniformBuffer(*iglDev_, &result);
    ASSERT_TRUE(result.isOk());

    vertexUniforms_.layer = static_cast<int>(1);

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
    const auto* layerStr = "Layer 0";
    util::validateFramebufferTexture(*iglDev_, *cmdQueue_, *framebuffer_, data, layerStr);
  }

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

//
// Texture Upload Test
//
// This test uploads data to an array texture and then downloads it again to validate it
//
namespace {
template<typename ColorType>
void runUploadTest(IDevice& device,
                   ICommandQueue& cmdQueue,
                   igl::TextureFormat format,
                   const ColorType* data) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> const pipelineState;

  //-------------------------------------
  // Create input texture and upload data
  //-------------------------------------
  TextureDesc const texDesc = TextureDesc::new2D(
      format, kOffscreenTexWidth, kOffscreenTexHeight, TextureDesc::TextureUsageBits::Sampled);
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(tex != nullptr);

  //
  // upload and redownload to make sure that we've uploaded successfully.
  //
  const auto uploadRange =
      TextureRangeDesc::new2D(0, 0, kOffscreenTexWidth, kOffscreenTexHeight, 0);
  ASSERT_TRUE(tex->upload(uploadRange, data).isOk());

  //--------------------------------
  // Verify against original texture
  //--------------------------------
  const auto* layerStr = "Layer 0";
  util::validateUploadedTextureRange(device, cmdQueue, tex, tex->getLayerRange(0), data, layerStr);
}
} // namespace
TEST_F(TextureHalfFloatTest, Upload_RGBA16) {
  runUploadTest(*iglDev_, *cmdQueue_, igl::TextureFormat::RGBA_F16, kTextureDataRGBAHalf.data());
}

TEST_F(TextureHalfFloatTest, Upload_RGB16) {
  if (iglDev_->getBackendType() == BackendType::Vulkan ||
      iglDev_->getBackendType() == BackendType::Metal || kUsesOpenGLES) {
    GTEST_SKIP() << "Skip due to lack of support for RGB";
  }
  runUploadTest(*iglDev_, *cmdQueue_, igl::TextureFormat::RGB_F16, kTextureDataRGBHalf.data());
}

TEST_F(TextureHalfFloatTest, Upload_RG16) {
  runUploadTest(*iglDev_, *cmdQueue_, igl::TextureFormat::RG_F16, kTextureDataRGHalf.data());
}

TEST_F(TextureHalfFloatTest, Upload_R16) {
  runUploadTest(*iglDev_, *cmdQueue_, igl::TextureFormat::R_F16, kTextureDataRHalf.data());
}

//
// Texture Passthrough Test - Sample From Array
//
// This test uses a simple shader to copy a layer of the input array texture to an
// a output texture that matches the size of the input texture layer
//

TEST_F(TextureHalfFloatTest, Passthrough_SampleRGBA16) {
  runPassthroughFormat(igl::TextureFormat::RGBA_F16, kTextureDataRGBAHalf.data());
}

TEST_F(TextureHalfFloatTest, Passthrough_SampleRGB16) {
#if IGL_PLATFORM_WIN && !IGL_ANGLE
  GTEST_SKIP() << "Skipping due to known issue on Windows without angle";
#endif

  if (iglDev_->getBackendType() == BackendType::Vulkan ||
      iglDev_->getBackendType() == BackendType::Metal ||
#if IGL_PLATFORM_MACOS
      iglDev_->getBackendType() == BackendType::OpenGL ||
#endif
      kUsesOpenGLES) {
    GTEST_SKIP() << "Skip due to lack of support for RGB";
  }
  runPassthroughFormat(igl::TextureFormat::RGB_F16, kTextureDataRGBHalf.data());
}

TEST_F(TextureHalfFloatTest, Passthrough_SampleRG16) {
#if IGL_PLATFORM_MACOS
  if (iglDev_->getBackendType() == BackendType::OpenGL) {
    GTEST_SKIP() << "Skip due to lack of support for RG on mac OpenGL";
  }
#endif
  runPassthroughFormat(igl::TextureFormat::RG_F16, kTextureDataRGHalf.data());
}

TEST_F(TextureHalfFloatTest, Passthrough_SampleR16) {
#if IGL_PLATFORM_MACOS
  if (iglDev_->getBackendType() == BackendType::OpenGL) {
    GTEST_SKIP() << "Skip due to lack of support for RG on mac OpenGL";
  }
#endif
  runPassthroughFormat(igl::TextureFormat::R_F16, kTextureDataRHalf.data());
}

} // namespace igl::tests
