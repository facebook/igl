/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../data/ShaderData.h"
#include "../data/TextureData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <array>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/NameHandle.h>
#include <igl/opengl/PlatformDevice.h>
#include <string>

// to not use extra curly braces in initializer lists
// This is needed to get tests working on Android
#ifdef __clang__
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

namespace igl::tests {

// Use a 4x4 texture for this test
constexpr size_t OFFSCREEN_TEX_WIDTH = 4;
constexpr size_t OFFSCREEN_TEX_HEIGHT = 4;
#if IGL_OPENGL_ES
#define FLOATING_POINT_TOLERANCE 0.0001
#else
#define FLOATING_POINT_TOLERANCE 0.00001
#endif

// clang-format off
#if !defined(OGL_UNIFORM_BUFFER_FRAG_COMMON)
#define OGL_UNIFORM_BUFFER_FRAG_COMMON \
               PROLOG \
               const float expectedFloat = 0.1; \
               const vec2 expectedVec2 = vec2(0.2, 0.2); \
               const vec3 expectedVec3 = vec3(0.3, 0.3, 0.3); \
               const vec4 expectedVec4 = vec4(0.4, 0.4, 0.4, 0.4); \
               const int expectedInt = 42; \
               const ivec2 expectediVec2 = ivec2(2, 2); \
               const ivec3 expectediVec3 = ivec3(3, 3, 3); \
               const ivec4 expectediVec4 = ivec4(4, 4, 4, 4); \
               const mat2 expectedMat2 = mat2(1.0, 2.0, 3.0, 4.0); \
               const mat3 expectedMat3 = mat3(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0); \
               const mat4 expectedMat4 = mat4(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0); \
               const vec4 failureColor = vec4(0.0, 0.0, 0.0, 1.0); \
               bool isEqual(float refVal, float val) { \
                 return abs(refVal - val) < FLOATING_POINT_TOLERANCE; \
               } \
               bool isEqual(vec2 vecA, vec2 vecB) { \
                 bool equal = true; \
                 for (int i = 0; i < 2; i++) { \
                   if (!isEqual(vecA[i], vecB[i])) { \
                     equal = false; \
                   } \
                 } \
                 return equal; \
               } \
               bool isEqual(vec3 vecA, vec3 vecB) { \
                 bool equal = true; \
                 for (int i = 0; i < 3; i++) { \
                   if (!isEqual(vecA[i], vecB[i])) { \
                     equal = false; \
                   } \
                 } \
                 return equal; \
               } \
               bool isEqual(vec4 vecA, vec4 vecB) { \
                 bool equal = true; \
                 for (int i = 0; i < 4; i++) { \
                   if (!isEqual(vecA[i], vecB[i])) { \
                     equal = false; \
                   } \
                 } \
                 return equal; \
               } \
               bool isEqual(mat3 matA, mat3 matB) { \
                 bool equal = true; \
                 for (int i = 0; i < 3; i++) { \
                    for (int j = 0; j < 3; j++) { \
                      if (!isEqual(matA[i][j], matB[i][j])) { \
                        equal = false; \
                      } \
                   } \
                 } \
                 return equal; \
               } \
               bool isEqual(mat4 matA, mat4 matB) { \
                 bool equal = true; \
                 for (int i = 0; i < 4; i++) { \
                    for (int j = 0; j < 4; j++) { \
                      if (!isEqual(matA[i][j], matB[i][j])) { \
                        equal = false; \
                      } \
                   } \
                 } \
                 return equal; \
               }
#endif

// Uniform Buffer Testing Shader
const char OGL_UNIFORM_BUFFER_FRAG_SHADER[] =
    IGL_TO_STRING(OGL_UNIFORM_BUFFER_FRAG_COMMON

               uniform float testFloat;
               uniform vec2 testVec2;
               uniform vec3 testVec3;
               uniform vec4 testVec4;

               uniform bool testBool;

               uniform int testInt;
               uniform ivec2 testiVec2;
               uniform ivec3 testiVec3;
               uniform ivec4 testiVec4;

               uniform mat2 testMat2;
               uniform mat3 testMat3;
               uniform mat4 testMat4;
               uniform vec4 backgroundColor;

               uniform float unsetFloat;
               uniform bool unsetBool;
               uniform int unsetInt;

               uniform sampler2D inputImage;

               varying vec2 uv;

               void main() {
                 gl_FragColor = texture2D(inputImage, uv);
                 if (uv.y < 0.25) {
                   if (uv.x < 0.25) {
                     if(!isEqual(testFloat, expectedFloat)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.5) {
                     if (!isEqual(testVec2, expectedVec2)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.75) {
                     if (!isEqual(testVec3, expectedVec3)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 1.0) {
                     if (!isEqual(testVec4, expectedVec4)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else {
                     gl_FragColor = backgroundColor;
                   }
                 }
                 else if (uv.y < 0.5) {
                   if (uv.x < 0.25) {
                     if (!testBool) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.5) {
                     if (testInt != expectedInt) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.75) {
                     if (testiVec2 != expectediVec2) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 1.0) {
                     if (testiVec3 != expectediVec3) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else {
                     gl_FragColor = backgroundColor;
                   }
                 }
                 else if (uv.y < 0.75) {
                   if (uv.x < 0.25) {
                     if (testiVec4 != expectediVec4) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.5) {
                     if (testMat2 != expectedMat2) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.75) {
                     if (!isEqual(testMat3, expectedMat3)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 1.0) {
                     if (!isEqual(testMat4, expectedMat4)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else {
                     gl_FragColor = backgroundColor;
                   }
                 }
                 else {
                   if (uv.x < 0.25) {
                     if(!isEqual(unsetFloat, expectedFloat)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.5) {
                     if (!unsetBool) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.75) {
                     if (unsetInt != expectedInt) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else {
                     gl_FragColor = failureColor;
                   }
                 }
               });

// Uniform Array Testing Shader
const char OGL_UNIFORM_ARRAY_FRAG_SHADER[] =
    IGL_TO_STRING(OGL_UNIFORM_BUFFER_FRAG_COMMON

               uniform float testFloat[3];
               uniform vec2 testVec2[3];
               uniform vec3 testVec3[3];
               uniform vec4 testVec4[3];

               uniform bool testBool[3];

               uniform int testInt[3];
               uniform ivec2 testiVec2[3];
               uniform ivec3 testiVec3[3];
               uniform ivec4 testiVec4[3];

               uniform mat2 testMat2[3];
               uniform mat3 testMat3[3];
               uniform mat4 testMat4[3];
               uniform vec4 backgroundColor;

               uniform float unsetFloat[3];
               uniform bool unsetBool[3];
               uniform int unsetInt[3];

               uniform sampler2D inputImage;

               varying vec2 uv;

               void main() {
                 gl_FragColor = texture2D(inputImage, uv);
                 if (uv.y < 0.25) {
                   if (uv.x < 0.25) {
                     if(!isEqual(testFloat[1], expectedFloat)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.5) {
                     if (!isEqual(testVec2[1], expectedVec2)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.75) {
                     if (!isEqual(testVec3[1], expectedVec3)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 1.0) {
                     if (!isEqual(testVec4[1], expectedVec4)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else {
                     gl_FragColor = backgroundColor;
                   }
                 }
                 else if (uv.y < 0.5) {
                   if (uv.x < 0.25) {
                     if (!testBool[1]) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.5) {
                     if (testInt[1] != expectedInt) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.75) {
                     if (testiVec2[1] != expectediVec2) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 1.0) {
                     if (testiVec3[1] != expectediVec3) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else {
                     gl_FragColor = backgroundColor;
                   }
                 }
                 else if (uv.y < 0.75) {
                   if (uv.x < 0.25) {
                     if (testiVec4[1]!= expectediVec4) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.5) {
                     if (testMat2[1] != expectedMat2) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.75) {
                     if (!isEqual(testMat3[1], expectedMat3)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 1.0) {
                     if (!isEqual(testMat4[1], expectedMat4)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else {
                     gl_FragColor = backgroundColor;
                   }
                 }
                 else {
                   if (uv.x < 0.25) {
                     if(!isEqual(unsetFloat[1], expectedFloat)) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.5) {
                     if (!unsetBool[1]) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else if (uv.x < 0.75) {
                     if (unsetInt[1] != expectedInt) {
                       gl_FragColor = failureColor;
                     }
                   }
                   else {
                     gl_FragColor = failureColor;
                   }
                 }
               });

// clang-format on

//
// UniformBufferTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class UniformBufferTest : public ::testing::Test {
 private:
 public:
  UniformBufferTest() = default;
  ~UniformBufferTest() override = default;

  //
  // SetUp()
  //
  // This function sets up a render pass and a render pipeline descriptor
  // so it is ready to render a simple quad with an input texture to an
  // offscreen texture.
  //
  // The actual creation of the render pipeline state object is left
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
    ASSERT_TRUE(ret.isOk());
    ASSERT_TRUE(offscreenTexture_ != nullptr);

    // Create input texture
    texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                 OFFSCREEN_TEX_WIDTH,
                                 OFFSCREEN_TEX_HEIGHT,
                                 TextureDesc::TextureUsageBits::Sampled);
    inputTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_TRUE(ret.isOk());
    ASSERT_TRUE(inputTexture_ != nullptr);

    // Create framebuffer using the offscreen texture
    FramebufferDesc framebufferDesc;

    framebufferDesc.colorAttachments[0].texture = offscreenTexture_;
    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_TRUE(ret.isOk());
    ASSERT_TRUE(framebuffer_ != nullptr);

    // Initialize render pass descriptor
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

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

    // Initialize vertex and sampler buffers
    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = data::vertex_index::QUAD_VERT;
    bufDesc.length = sizeof(data::vertex_index::QUAD_VERT);

    vb_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk());
    ASSERT_TRUE(vb_ != nullptr);

    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = data::vertex_index::QUAD_UV;
    bufDesc.length = sizeof(data::vertex_index::QUAD_UV);

    uv_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_TRUE(ret.isOk());
    ASSERT_TRUE(uv_ != nullptr);

    // Initialize sampler state
    const SamplerStateDesc samplerDesc;
    samp_ = iglDev_->createSamplerState(samplerDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(samp_ != nullptr);

    // Initialize Render Pipeline Descriptor, but leave the creation
    // to the individual tests in case further customization is required
    renderPipelineDesc_.vertexInputState = vertexInputState_;
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
  std::shared_ptr<ITexture> inputTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> vb_, uv_, ib_;
  std::shared_ptr<IBuffer> fragmentParamBuffer_;

  std::shared_ptr<ISamplerState> samp_;

  RenderPipelineDesc renderPipelineDesc_;
  size_t textureUnit_ = 0;
  size_t uniformTypesCount_ = 12;
  size_t failureCasesCount_ = 3;
};

//
// UniformBuffer uniform binding test
//
// This test exercises the uniform buffer binding behavior.
// The custom fragment shader will only show the original input texture when each and all of the
// uniform types are binded properly
//
TEST_F(UniformBufferTest, UniformBufferBinding) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;
  const simd::float4 clearColor = {0.0, 0.0, 1.0, 1.0};
  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, OFFSCREEN_TEX_WIDTH, OFFSCREEN_TEX_HEIGHT);
  struct FragmentParameters {
    simd::float1 testFloat{};
    simd::float2 testVec2{};
    simd::float3 testVec3{};
    simd::float4 testVec4{};

    bool testBool{};

    simd::int1 testInt{};
    simd::int2 testiVec2{};
    simd::int3 testiVec3{};
    simd::int4 testiVec4{};

    simd::float2x2 testMat2;
    simd::float3x3 testMat3;
    simd::float4x4 testMat4;
    simd::float4 backgroundColor{};

    simd::float1 unsetFloat{};
    bool unsetBool{};
    simd::int1 unsetInt{};
  } fragmentParameters_;

  //-------------------------------------
  // Upload the texture
  //-------------------------------------
  inputTexture_->upload(rangeDesc, data::texture::TEX_RGBA_MISC1_4x4);

  //----------------
  // Create Shaders
  //----------------
  // Initialize shader stages
  std::unique_ptr<IShaderStages> stages;
  igl::tests::util::createShaderStages(iglDev_,
                                       data::shader::OGL_SIMPLE_VERT_SHADER,
                                       data::shader::simpleVertFunc,
                                       OGL_UNIFORM_BUFFER_FRAG_SHADER,
                                       data::shader::simpleFragFunc,
                                       stages);
  shaderStages_ = std::move(stages);

  renderPipelineDesc_.shaderStages = shaderStages_;

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(pipelineState != nullptr);

  //-------------------------------------
  // Create Uniforms Buffer
  //-------------------------------------
  // Make sure there are more texture pixels than our test cases
  ASSERT_TRUE(uniformTypesCount_ + failureCasesCount_ <=
              OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT);

  BufferDesc fpDesc;
  fpDesc.type = BufferDesc::BufferTypeBits::Uniform;
  fpDesc.data = &fragmentParameters_;
  fpDesc.length = sizeof(fragmentParameters_);
  fpDesc.storage = ResourceStorage::Shared;

  std::vector<UniformDesc> fragmentUniformDescriptors;

  // "testFloat"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testFloat"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Float;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testFloat);
  fragmentParameters_.testFloat = {0.1f};

  // "testVec2"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testVec2"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Float2;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testVec2);
  fragmentParameters_.testVec2 = {0.2f, 0.2f};

  // "testVec3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testVec3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Float3;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testVec3);
  fragmentParameters_.testVec3 = {0.3f, 0.3f, 0.3f};

  // "testVec4"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testVec4"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Float4;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testVec4);
  fragmentParameters_.testVec4 = {0.4f, 0.4f, 0.4f, 0.4f};

  // "testBool"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testBool"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Boolean;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testBool);
  fragmentParameters_.testBool = true;

  // "testInt"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testInt"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Int;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testInt);
  fragmentParameters_.testInt = {42};

  // "testiVec2"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testiVec2"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Int2;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testiVec2);
  fragmentParameters_.testiVec2 = {2, 2};

  // "testiVec3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testiVec3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Int3;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testiVec3);
  fragmentParameters_.testiVec3 = {3, 3, 3};

  // "testiVec4"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testiVec4"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Int4;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testiVec4);
  fragmentParameters_.testiVec4 = {4, 4, 4, 4};

  // "testMat2"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testMat2"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Mat2x2;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testMat2);
  simd::float2 firstCol2 = {1.0, 2.0}, secondCol2 = {3.0, 4.0};
  fragmentParameters_.testMat2 = {firstCol2, secondCol2};

  // "testMat3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testMat3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Mat3x3;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testMat3);
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::float3x3);
  simd::float3 firstCol3 = {1.0, 2.0, 3.0}, secondCol3 = {4.0, 5.0, 6.0},
               thirdCol3 = {7.0, 8.0, 9.0};
  fragmentParameters_.testMat3 = {firstCol3, secondCol3, thirdCol3};

  // "testMat4"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testMat4"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Mat4x4;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testMat4);
  simd::float4 firstCol4 = {1.0, 2.0, 3.0, 4.0}, secondCol4 = {5.0, 6.0, 7.0, 8.0},
               thirdCol4 = {9.0, 10.0, 11.0, 12.0}, fourthCol4 = {13.0, 14.0, 15.0, 16.0};
  fragmentParameters_.testMat4 = {firstCol4, secondCol4, thirdCol4, fourthCol4};

  // "backgroundColor"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location = pipelineState->getIndexByName(
      igl::genNameHandle("backgroundColor"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Float4;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, backgroundColor);
  fragmentParameters_.backgroundColor = clearColor;

  // "unsetFloat1"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("unsetFloat1"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Float;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, unsetFloat);
  fragmentParameters_.unsetFloat = {0.1f};

  // "unsetBool1"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("unsetBool1"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Boolean;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, unsetBool);
  fragmentParameters_.unsetBool = true;

  // "unsetInt1"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("unsetInt1"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().type = UniformType::Int;
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, unsetInt);
  fragmentParameters_.unsetInt = {42};

  fragmentParamBuffer_ = iglDev_->createBuffer(fpDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(fragmentParamBuffer_ != nullptr);

  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(cmdBuf_ != nullptr);

  std::shared_ptr<igl::IRenderCommandEncoder> cmds =
      cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(6);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  //----------------------
  // Read back framebuffer
  //----------------------
  auto pixels = std::vector<uint32_t>(OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT);

  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);

  //--------------------------------
  // Verify against original texture
  // Pixels are supposed to be 0xFF000000 since the uniform buffer is not bound
  //--------------------------------
  for (size_t i = 0; i < OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT; i++) {
    ASSERT_EQ(pixels[i], 0xFF000000);
  }

  //----------------
  // Bind the uniform buffer
  //----------------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(cmdBuf_ != nullptr);

  cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  // The Uniform Buffer is binded here:
  for (const auto& uniformDesc : fragmentUniformDescriptors) {
    cmds->bindUniform(uniformDesc, &fragmentParameters_);
  }

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(6);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  //----------------------
  // Read back framebuffer
  //----------------------
  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);

  //--------------------------------
  // Verify against original texture
  //--------------------------------
  // If the uniform buffer is broken, the pixels would be 0xFF000000
  for (size_t i = 0; i < uniformTypesCount_; i++) {
    ASSERT_EQ(pixels[i], data::texture::TEX_RGBA_MISC1_4x4[i]);
  }

  // For the unset uniform buffers, make sure the test fails as expected
  for (size_t i = uniformTypesCount_; i < uniformTypesCount_ + failureCasesCount_; i++) {
    ASSERT_EQ(pixels[i], 0xFF000000);
  }
}

//
// UniformBuffer uniform array binding test
//
// This test exercises the uniform array binding behavior.
// The custom fragment shader will only show the original input texture when each and all of the
// uniform types are binded properly
//
TEST_F(UniformBufferTest, UniformArrayBinding) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;
  const simd::float4 clearColor = {0.0, 0.0, 1.0, 1.0};
  const auto rangeDesc = TextureRangeDesc::new2D(0, 0, OFFSCREEN_TEX_WIDTH, OFFSCREEN_TEX_HEIGHT);

  // We are purposely creating an unpacked structure to trigger the
  // manual packing path in UniformBuffer::bindUniformArray.
  // This code has only been tested on MacOS, and we may need to
  // revisit if in the future we are running the test on a different OS
  // or building it with a different compiler.
  struct Float1UnpackedData {
    simd::float1 float1;
    bool padding[3];
  };

  struct Int1UnpackedData {
    simd::int1 int1;
    bool padding[3];
  };

  struct Float2UnpackedData {
    simd::float2 float2;
    bool padding[3];
  };

  struct Int2UnpackedData {
    simd::int2 int2;
    bool padding[3];
  };

  // Interestingly with vectorization, int3, float3, and float3x3 has the same size as their
  // 4 versions (12 bytes vs 16 bytes so the data is not packed.)
  // As a result, to test UniformArrayBinding, a separate test is not needed for data structures
  // of multiples of 3.

  struct Float4UnpackedData {
    simd::float4 float4;
    bool padding[3];
  };

  struct Int4UnpackedData {
    simd::int4 int4;
    bool padding[3];
  };

  struct BooleanUnpackedData {
    bool data;
    bool padding[3];
    simd::float3 morePadding;
  };

  struct FragmentParameters {
    std::array<Float1UnpackedData, 3> testFloat{};
    std::array<Float2UnpackedData, 3> testVec2{};
    std::array<simd::float3, 3> testVec3{};
    std::array<Float4UnpackedData, 3> testVec4{};

    std::array<BooleanUnpackedData, 3> testBool{};

    std::array<Int1UnpackedData, 3> testInt{};
    std::array<Int2UnpackedData, 3> testiVec2{};
    std::array<simd::int3, 3> testiVec3{};
    std::array<Int4UnpackedData, 3> testiVec4{};

    std::array<simd::float2x2, 3> testMat2;
    std::array<simd::float3x3, 3> testMat3;
    std::array<simd::float4x4, 3> testMat4;
    simd::float4 backgroundColor{};

    std::array<simd::float1, 3> unsetFloat{};
    std::array<bool, 3> unsetBool{};
    std::array<simd::int1, 3> unsetInt{};
  } fragmentParameters_;

  //-------------------------------------
  // Upload the texture
  //-------------------------------------
  inputTexture_->upload(rangeDesc, data::texture::TEX_RGBA_MISC1_4x4);

  //----------------
  // Create Shaders
  //----------------
  // Initialize shader stages
  std::unique_ptr<IShaderStages> stages;
  igl::tests::util::createShaderStages(iglDev_,
                                       data::shader::OGL_SIMPLE_VERT_SHADER,
                                       data::shader::simpleVertFunc,
                                       OGL_UNIFORM_ARRAY_FRAG_SHADER,
                                       data::shader::simpleFragFunc,
                                       stages);
  shaderStages_ = std::move(stages);

  renderPipelineDesc_.shaderStages = shaderStages_;

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(pipelineState != nullptr);

  //-------------------------------------
  // Create Uniforms Buffer
  //-------------------------------------
  // Make sure there are more texture pixels than our test cases
  ASSERT_TRUE(uniformTypesCount_ + failureCasesCount_ <=
              OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT);

  BufferDesc fpDesc;
  fpDesc.type = BufferDesc::BufferTypeBits::Uniform;
  fpDesc.data = &fragmentParameters_;
  fpDesc.length = sizeof(fragmentParameters_);
  fpDesc.storage = ResourceStorage::Shared;

  std::vector<UniformDesc> fragmentUniformDescriptors;

  // "testFloat"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Float;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testFloat"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testFloat);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(Float1UnpackedData);
  fragmentParameters_.testFloat[0] = {0.0f, {true, false, true}};
  fragmentParameters_.testFloat[1] = {0.1f, {true, true, true}};
  fragmentParameters_.testFloat[2] = {0.0f, {false, false, false}};

  // "testVec2"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Float2;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testVec2"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testVec2);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(Float2UnpackedData);
  fragmentParameters_.testVec2[0] = {{0.0f, 0.0f}, {true, false, true}};
  fragmentParameters_.testVec2[1] = {{0.2f, 0.2f}, {true, true, true}};
  fragmentParameters_.testVec2[2] = {{0.0f, 0.0f}, {false, false, false}};

  // "testVec3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Float3;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testVec3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testVec3);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::float3);

  fragmentParameters_.testVec3[0] = {0.0f, 0.0f, 0.0f};
  fragmentParameters_.testVec3[1] = {0.3f, 0.3f, 0.3f};
  fragmentParameters_.testVec3[2] = {0.0f, 0.0f, 0.0f};

  // "testVec4"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Float4;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testVec4"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testVec4);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(Float4UnpackedData);
  fragmentParameters_.testVec4[0] = {{0.0f, 0.0f, 0.0f, 0.0f}, {true, false, true}};
  fragmentParameters_.testVec4[1] = {{0.4f, 0.4f, 0.4f, 0.4f}, {true, true, true}};
  fragmentParameters_.testVec4[2] = {{0.0f, 0.0f, 0.0f, 0.0f}, {false, false, false}};

  // "testBool"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Boolean;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testBool"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testBool);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(BooleanUnpackedData);
  fragmentParameters_.testBool[0] = {false, {false, false, true}, {0.0f, 0.1f, 0.2f}};
  fragmentParameters_.testBool[1] = {true, {false, false, true}, {0.3f, 0.4f, 0.5f}};
  fragmentParameters_.testBool[2] = {false, {true, true, true}, {0.6f, 0.7f, 0.8f}};

  // "testInt"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Int;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testInt"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testInt);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(Int1UnpackedData);
  fragmentParameters_.testInt[0] = {0, {true, false, true}};
  fragmentParameters_.testInt[1] = {42, {true, true, true}};
  fragmentParameters_.testInt[2] = {0, {false, false, false}};

  // "testiVec2"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Int2;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testiVec2"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testiVec2);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(Int2UnpackedData);
  fragmentParameters_.testiVec2[0] = {{0, 0}, {true, false, true}};
  fragmentParameters_.testiVec2[1] = {{2, 2}, {true, true, true}};
  fragmentParameters_.testiVec2[2] = {{0, 0}, {false, false, false}};

  // "testiVec3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Int3;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testiVec3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testiVec3);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::int3);
  fragmentParameters_.testiVec3[0] = {0, 0, 0};
  fragmentParameters_.testiVec3[1] = {3, 3, 3};
  fragmentParameters_.testiVec3[2] = {0, 0, 0};

  // "testiVec4"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Int4;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testiVec4"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testiVec4);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(Int4UnpackedData);
  fragmentParameters_.testiVec4[0] = {{0, 0, 0, 0}, {true, false, true}};
  fragmentParameters_.testiVec4[1] = {{4, 4, 4, 4}, {true, true, true}};
  fragmentParameters_.testiVec4[2] = {{0, 0, 0, 0}, {false, false, false}};

  // "testMat2"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Mat2x2;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testMat2"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testMat2);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::float2x2);
  simd::float2 firstCol2 = {1.0, 2.0}, secondCol2 = {3.0, 4.0}, col2_0 = {0.0, 0.0};
  fragmentParameters_.testMat2[0] = {col2_0, col2_0};
  fragmentParameters_.testMat2[1] = {firstCol2, secondCol2};
  fragmentParameters_.testMat2[2] = {col2_0, col2_0};

  // "testMat3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Mat3x3;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testMat3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testMat3);
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::float3x3);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::float3x3);
  simd::float3 firstCol3 = {1.0, 2.0, 3.0}, secondCol3 = {4.0, 5.0, 6.0},
               thirdCol3 = {7.0, 8.0, 9.0}, col3_0 = {0.0, 0.0, 0.0};
  fragmentParameters_.testMat3[0] = {col3_0, col3_0, col3_0};
  fragmentParameters_.testMat3[1] = {firstCol3, secondCol3, thirdCol3};
  fragmentParameters_.testMat3[2] = {col3_0, col3_0, col3_0};

  // "testMat4"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Mat4x4;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("testMat4"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, testMat4);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::float4x4);
  simd::float4 firstCol4 = {1.0, 2.0, 3.0, 4.0}, secondCol4 = {5.0, 6.0, 7.0, 8.0},
               thirdCol4 = {9.0, 10.0, 11.0, 12.0}, fourthCol4 = {13.0, 14.0, 15.0, 16.0},
               col4_0 = {0.0, 0.0, 0.0, 0.0};
  fragmentParameters_.testMat4[0] = {col4_0, col4_0, col4_0, col4_0};
  fragmentParameters_.testMat4[1] = {firstCol4, secondCol4, thirdCol4, fourthCol4};
  fragmentParameters_.testMat4[2] = {col4_0, col4_0, col4_0, col4_0};

  // "backgroundColor"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Float4;
  fragmentUniformDescriptors.back().location = pipelineState->getIndexByName(
      igl::genNameHandle("backgroundColor"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, backgroundColor);
  fragmentParameters_.backgroundColor = clearColor;

  // "unsetFloat3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Float;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("unsetFloat3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, unsetFloat);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::float1);
  fragmentParameters_.unsetFloat = {0.0f, 0.1f, 0.0f};

  // "unsetBool3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Boolean;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("unsetBool3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, unsetBool);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(bool);
  fragmentParameters_.unsetBool[0] = false;
  fragmentParameters_.unsetBool[1] = true;
  fragmentParameters_.unsetBool[2] = false;

  // "unsetInt3"
  fragmentUniformDescriptors.emplace_back();
  fragmentUniformDescriptors.back().type = UniformType::Int;
  fragmentUniformDescriptors.back().location =
      pipelineState->getIndexByName(igl::genNameHandle("unsetInt3"), igl::ShaderStage::Fragment);
  fragmentUniformDescriptors.back().offset = offsetof(FragmentParameters, unsetInt);
  fragmentUniformDescriptors.back().numElements = 3;
  fragmentUniformDescriptors.back().elementStride = sizeof(simd::int1);
  fragmentParameters_.unsetInt[0] = {0};
  fragmentParameters_.unsetInt[1] = {42};
  fragmentParameters_.unsetInt[2] = {0};

  fragmentParamBuffer_ = iglDev_->createBuffer(fpDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(fragmentParamBuffer_ != nullptr);

  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(cmdBuf_ != nullptr);

  std::shared_ptr<igl::IRenderCommandEncoder> cmds =
      cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(6);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  //----------------------
  // Read back framebuffer
  //----------------------
  auto pixels = std::vector<uint32_t>(OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT);

  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);

  //--------------------------------
  // Verify against original texture
  // Pixels are supposed to be 0xFF000000 since the uniform buffer is not bound
  //--------------------------------
  for (size_t i = 0; i < OFFSCREEN_TEX_WIDTH * OFFSCREEN_TEX_HEIGHT; i++) {
    ASSERT_EQ(pixels[i], 0xFF000000);
  }

  //----------------
  // Bind the uniform buffer
  //----------------
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_TRUE(cmdBuf_ != nullptr);

  cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, framebuffer_);
  cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
  cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

  cmds->bindRenderPipelineState(pipelineState);

  // The Uniform Buffer is binded here:
  for (const auto& uniformDesc : fragmentUniformDescriptors) {
    cmds->bindUniform(uniformDesc, &fragmentParameters_);
  }

  cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
  cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

  cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
  cmds->drawIndexed(6);
  cmds->endEncoding();

  cmdQueue_->submit(*cmdBuf_);

  //----------------------
  // Read back framebuffer
  //----------------------
  framebuffer_->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), rangeDesc);

  //--------------------------------
  // Verify against original texture
  //--------------------------------
  // If the uniform buffer is broken, the pixels would be 0xFF000000
  for (size_t i = 0; i < uniformTypesCount_; i++) {
    ASSERT_EQ(pixels[i], data::texture::TEX_RGBA_MISC1_4x4[i]);
  }

  // For the unset uniform buffers, make sure the test fails as expected
  for (size_t i = uniformTypesCount_; i < uniformTypesCount_ + failureCasesCount_; i++) {
    ASSERT_EQ(pixels[i], 0xFF000000);
  }
}

} // namespace igl::tests
