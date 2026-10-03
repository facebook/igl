/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/StateSanitizer.h>

#include <cmath>
#include <memory>
#include <vector>
#include <igl/tests/util/device/webgpu/TestDevice.h>

namespace igl::tests {

namespace {

VertexInputStateDesc makeVertexInput(size_t stride, VertexAttributeFormat format, size_t offset) {
  return {
      .numAttributes = 1,
      .attributes = {{.bufferIndex = 0, .format = format, .offset = offset, .location = 0}},
      .numInputBindings = 1,
      .inputBindings = {{.stride = stride}},
  };
}

WGPULimits defaultLimits() {
  WGPULimits limits = WGPU_LIMITS_INIT;
  limits.maxVertexAttributes = 16;
  limits.maxVertexBuffers = 8;
  limits.maxVertexBufferArrayStride = 2048;
  return limits;
}

} // namespace

TEST(WebGPUStateSanitizerTest, MinMaxBlendForcesOneFactors) {
  WGPUBlendComponent component = {};
  ASSERT_TRUE(webgpu::makeBlendComponent(
                  BlendOp::Max, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha, component)
                  .isOk());
  EXPECT_EQ(component.operation, WGPUBlendOperation_Max);
  EXPECT_EQ(component.srcFactor, WGPUBlendFactor_One);
  EXPECT_EQ(component.dstFactor, WGPUBlendFactor_One);

  ASSERT_TRUE(webgpu::makeBlendComponent(
                  BlendOp::Add, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha, component)
                  .isOk());
  EXPECT_EQ(component.srcFactor, WGPUBlendFactor_SrcAlpha);
  EXPECT_EQ(component.dstFactor, WGPUBlendFactor_OneMinusSrcAlpha);
}

TEST(WebGPUStateSanitizerTest, BlendAlphaFactorIsUnsupported) {
  WGPUBlendComponent component = {};
  EXPECT_EQ(webgpu::makeBlendComponent(
                BlendOp::Add, BlendFactor::BlendAlpha, BlendFactor::Zero, component)
                .code,
            Result::Code::Unsupported);
}

TEST(WebGPUStateSanitizerTest, AlphaToCoverageNeedsSamples) {
  WGPUMultisampleState state = {};
  ASSERT_TRUE(webgpu::makeMultisampleState(1, true, state).isOk());
  EXPECT_EQ(state.alphaToCoverageEnabled, 0u);
  ASSERT_TRUE(webgpu::makeMultisampleState(4, true, state).isOk());
  EXPECT_EQ(state.alphaToCoverageEnabled, 1u);
  EXPECT_EQ(state.count, 4u);
  EXPECT_EQ(webgpu::makeMultisampleState(2, false, state).code, Result::Code::Unsupported);
  EXPECT_EQ(webgpu::makeMultisampleState(8, false, state).code, Result::Code::Unsupported);
}

TEST(WebGPUStateSanitizerTest, MissingAspectsGetDefaults) {
  const DepthStencilStateDesc desc = {
      .compareFunction = CompareFunction::Less,
      .isDepthWriteEnabled = true,
      .backFaceStencil = {.depthStencilPassOperation = StencilOperation::Replace,
                          .stencilCompareFunction = CompareFunction::Equal},
      .frontFaceStencil = {.depthStencilPassOperation = StencilOperation::Replace,
                           .stencilCompareFunction = CompareFunction::Equal},
  };
  WGPUDepthStencilState state = {};
  ASSERT_TRUE(webgpu::makeDepthStencilState(desc, WGPUTextureFormat_Depth32Float, state).isOk());
  EXPECT_EQ(state.depthCompare, WGPUCompareFunction_Less);
  EXPECT_EQ(state.depthWriteEnabled, WGPUOptionalBool_True);
  EXPECT_EQ(state.stencilFront.passOp, WGPUStencilOperation_Keep);
  EXPECT_EQ(state.stencilBack.compare, WGPUCompareFunction_Always);

  ASSERT_TRUE(webgpu::makeDepthStencilState(desc, WGPUTextureFormat_Stencil8, state).isOk());
  EXPECT_EQ(state.depthCompare, WGPUCompareFunction_Always);
  EXPECT_EQ(state.depthWriteEnabled, WGPUOptionalBool_False);
  EXPECT_EQ(state.stencilFront.passOp, WGPUStencilOperation_Replace);
  EXPECT_EQ(state.stencilFront.compare, WGPUCompareFunction_Equal);

  EXPECT_EQ(webgpu::makeDepthStencilState(desc, WGPUTextureFormat_RGBA8Unorm, state).code,
            Result::Code::ArgumentInvalid);
}

TEST(WebGPUStateSanitizerTest, StencilMasksComeFromTheFrontFace) {
  const DepthStencilStateDesc desc = {
      .backFaceStencil = {.readMask = 0x0f, .writeMask = 0x0e},
      .frontFaceStencil = {.readMask = 0xf0, .writeMask = 0xe0},
  };
  WGPUDepthStencilState state = {};
  ASSERT_TRUE(
      webgpu::makeDepthStencilState(desc, WGPUTextureFormat_Depth24PlusStencil8, state).isOk());
  EXPECT_EQ(state.stencilReadMask, 0xf0u);
  EXPECT_EQ(state.stencilWriteMask, 0xe0u);
}

TEST(WebGPUStateSanitizerTest, DepthBiasOnlyForTriangles) {
  WGPUDepthStencilState state = WGPU_DEPTH_STENCIL_STATE_INIT;
  webgpu::applyDepthBias(PrimitiveType::Triangle, 2.4f, 1.5f, 0.25f, state);
  EXPECT_EQ(state.depthBias, 2);
  EXPECT_EQ(state.depthBiasSlopeScale, 1.5f);
  EXPECT_EQ(state.depthBiasClamp, 0.25f);
  for (const PrimitiveType topology :
       {PrimitiveType::Point, PrimitiveType::Line, PrimitiveType::LineStrip}) {
    webgpu::applyDepthBias(topology, 2.0f, 1.5f, 0.25f, state);
    EXPECT_EQ(state.depthBias, 0);
    EXPECT_EQ(state.depthBiasSlopeScale, 0.0f);
    EXPECT_EQ(state.depthBiasClamp, 0.0f);
  }
}

TEST(WebGPUStateSanitizerTest, DepthBiasIsClampedToInt32) {
  WGPUDepthStencilState state = WGPU_DEPTH_STENCIL_STATE_INIT;
  webgpu::applyDepthBias(PrimitiveType::Triangle, 1e20f, 0.0f, 0.0f, state);
  EXPECT_EQ(state.depthBias, 2147483520);
  webgpu::applyDepthBias(PrimitiveType::Triangle, -1e20f, 0.0f, 0.0f, state);
  EXPECT_EQ(state.depthBias, -2147483520);
  webgpu::applyDepthBias(PrimitiveType::Triangle, std::nanf(""), 0.0f, 0.0f, state);
  EXPECT_EQ(state.depthBias, 0);
}

TEST(WebGPUStateSanitizerTest, PrimitiveState) {
  WGPUPrimitiveState state = {};
  RenderPipelineDesc desc;
  desc.topology = PrimitiveType::Triangle;
  ASSERT_TRUE(webgpu::makePrimitiveState(
                  desc, CullMode::Back, WindingMode::Clockwise, WGPUIndexFormat_Uint16, state)
                  .isOk());
  EXPECT_EQ(state.stripIndexFormat, WGPUIndexFormat_Undefined);
  EXPECT_EQ(state.cullMode, WGPUCullMode_Back);
  EXPECT_EQ(state.frontFace, WGPUFrontFace_CW);

  desc.topology = PrimitiveType::TriangleStrip;
  ASSERT_TRUE(
      webgpu::makePrimitiveState(
          desc, CullMode::Disabled, WindingMode::CounterClockwise, WGPUIndexFormat_Uint16, state)
          .isOk());
  EXPECT_EQ(state.stripIndexFormat, WGPUIndexFormat_Uint16);

  desc.polygonFillMode = PolygonFillMode::Line;
  EXPECT_EQ(
      webgpu::makePrimitiveState(
          desc, CullMode::Disabled, WindingMode::CounterClockwise, WGPUIndexFormat_Undefined, state)
          .code,
      Result::Code::Unsupported);
}

TEST(WebGPUStateSanitizerTest, VertexInputRules) {
  const WGPULimits limits = defaultLimits();
  EXPECT_TRUE(webgpu::validateVertexInputState(
                  makeVertexInput(16, VertexAttributeFormat::Float4, 0), limits)
                  .isOk());
  // Stride not a multiple of 4.
  EXPECT_EQ(webgpu::validateVertexInputState(
                makeVertexInput(6, VertexAttributeFormat::UShort2Norm, 0), limits)
                .code,
            Result::Code::Unsupported);
  // No 3-component 8/16-bit formats.
  EXPECT_EQ(webgpu::validateVertexInputState(
                makeVertexInput(4, VertexAttributeFormat::UByte3Norm, 0), limits)
                .code,
            Result::Code::Unsupported);
  // Offset must be a multiple of min(4, size).
  EXPECT_EQ(webgpu::validateVertexInputState(makeVertexInput(16, VertexAttributeFormat::Float2, 2),
                                             limits)
                .code,
            Result::Code::Unsupported);
  EXPECT_EQ(
      webgpu::validateVertexInputState(makeVertexInput(8, VertexAttributeFormat::Float4, 0), limits)
          .code,
      Result::Code::ArgumentOutOfRange);

  // Bindings are selected by buffer index, beyond numInputBindings too.
  VertexInputStateDesc desc = makeVertexInput(16, VertexAttributeFormat::Float4, 0);
  desc.attributes[0].bufferIndex = 1;
  desc.inputBindings[1].stride = 16;
  EXPECT_TRUE(webgpu::validateVertexInputState(desc, limits).isOk());
  desc.attributes[0].bufferIndex = limits.maxVertexBuffers;
  EXPECT_EQ(webgpu::validateVertexInputState(desc, limits).code, Result::Code::ArgumentOutOfRange);
  desc = makeVertexInput(16, VertexAttributeFormat::Float4, 0);
  desc.numInputBindings = IGL_BUFFER_BINDINGS_MAX + 1;
  EXPECT_EQ(webgpu::validateVertexInputState(desc, limits).code, Result::Code::ArgumentOutOfRange);
  desc = makeVertexInput(16, VertexAttributeFormat::Float4, 0);
  desc.inputBindings[0].sampleFunction = VertexSampleFunction::Constant;
  EXPECT_EQ(webgpu::validateVertexInputState(desc, limits).code, Result::Code::Unsupported);

  // A negative location falls back to the attribute index, which may collide with an explicit one.
  desc = makeVertexInput(16, VertexAttributeFormat::Float2, 0);
  desc.numAttributes = 2;
  desc.attributes[0].location = 1;
  desc.attributes[1] = {
      .bufferIndex = 0, .format = VertexAttributeFormat::Float2, .offset = 8, .location = -1};
  EXPECT_EQ(webgpu::validateVertexInputState(desc, limits).code, Result::Code::ArgumentInvalid);
  desc.attributes[0].location = 0;
  EXPECT_TRUE(webgpu::validateVertexInputState(desc, limits).isOk());
  desc.attributes[1].location = static_cast<int>(limits.maxVertexAttributes);
  EXPECT_EQ(webgpu::validateVertexInputState(desc, limits).code, Result::Code::ArgumentOutOfRange);
}

TEST(WebGPUStateSanitizerTest, VertexBufferLayouts) {
  const VertexInputStateDesc desc = {
      .numAttributes = 3,
      .attributes = {{.bufferIndex = 2, .format = VertexAttributeFormat::Float2, .location = 1},
                     {.bufferIndex = 0, .format = VertexAttributeFormat::Float4, .location = 0},
                     {.bufferIndex = 2,
                      .format = VertexAttributeFormat::UByte4Norm,
                      .offset = 8,
                      .location = -1}},
      .numInputBindings = 3,
      .inputBindings = {{.stride = 16},
                        {.stride = 4},
                        {.stride = 12, .sampleFunction = VertexSampleFunction::Instance}},
  };
  std::vector<WGPUVertexBufferLayout> layouts;
  std::vector<WGPUVertexAttribute> attributes;
  webgpu::makeVertexBufferLayouts(desc, layouts, attributes);
  ASSERT_EQ(layouts.size(), 3u);
  ASSERT_EQ(attributes.size(), 3u);

  EXPECT_EQ(layouts[0].stepMode, WGPUVertexStepMode_Vertex);
  EXPECT_EQ(layouts[0].arrayStride, 16u);
  ASSERT_EQ(layouts[0].attributeCount, 1u);
  EXPECT_EQ(layouts[0].attributes[0].format, WGPUVertexFormat_Float32x4);

  // An input binding without attributes is an unused slot.
  EXPECT_EQ(layouts[1].stepMode, WGPUVertexStepMode_Undefined);
  EXPECT_EQ(layouts[1].arrayStride, 0u);
  EXPECT_EQ(layouts[1].attributeCount, 0u);

  EXPECT_EQ(layouts[2].stepMode, WGPUVertexStepMode_Instance);
  ASSERT_EQ(layouts[2].attributeCount, 2u);
  EXPECT_EQ(layouts[2].attributes[0].shaderLocation, 1u);
  EXPECT_EQ(layouts[2].attributes[1].offset, 8u);
  EXPECT_EQ(layouts[2].attributes[1].shaderLocation, 2u);
}

TEST(WebGPUStateSanitizerTest, VertexBufferLayoutsSkipOutOfRangeBuffers) {
  const VertexInputStateDesc desc = {
      .numAttributes = 2,
      .attributes = {{.bufferIndex = 1, .format = VertexAttributeFormat::Float4, .location = 0},
                     {.bufferIndex = IGL_BUFFER_BINDINGS_MAX,
                      .format = VertexAttributeFormat::Float4,
                      .location = 1}},
      .numInputBindings = 1,
      .inputBindings = {{.stride = 4}, {.stride = 16}},
  };
  std::vector<WGPUVertexBufferLayout> layouts;
  std::vector<WGPUVertexAttribute> attributes;
  webgpu::makeVertexBufferLayouts(desc, layouts, attributes);
  ASSERT_EQ(layouts.size(), 2u);
  ASSERT_EQ(attributes.size(), 1u);
  EXPECT_EQ(layouts[0].stepMode, WGPUVertexStepMode_Undefined);
  EXPECT_EQ(layouts[0].arrayStride, 0u);
  EXPECT_EQ(layouts[1].stepMode, WGPUVertexStepMode_Vertex);
  EXPECT_EQ(layouts[1].arrayStride, 16u);
  ASSERT_EQ(layouts[1].attributeCount, 1u);
  EXPECT_EQ(layouts[1].attributes[0].shaderLocation, 0u);
}

TEST(WebGPUStateSanitizerTest, AnisotropyNeedsLinearFilters) {
  EXPECT_EQ(webgpu::getMaxAnisotropy({.maxAnisotropic = 8}), 1u);
  EXPECT_EQ(webgpu::getMaxAnisotropy({.minFilter = SamplerMinMagFilter::Linear,
                                      .magFilter = SamplerMinMagFilter::Linear,
                                      .mipFilter = SamplerMipFilter::Linear,
                                      .maxAnisotropic = 8}),
            8u);
  EXPECT_EQ(webgpu::getMaxAnisotropy({.minFilter = SamplerMinMagFilter::Linear,
                                      .magFilter = SamplerMinMagFilter::Linear,
                                      .mipFilter = SamplerMipFilter::Linear,
                                      .maxAnisotropic = 64}),
            16u);
}

TEST(WebGPUStateSanitizerTest, OffsetAndCopyAlignment) {
  EXPECT_TRUE(webgpu::isDynamicOffsetAligned(0));
  EXPECT_TRUE(webgpu::isDynamicOffsetAligned(512));
  EXPECT_FALSE(webgpu::isDynamicOffsetAligned(16));
  EXPECT_TRUE(webgpu::validateBufferCopy(4, 8, 12).isOk());
  EXPECT_EQ(webgpu::validateBufferCopy(2, 0, 4).code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(webgpu::validateBufferCopy(0, 0, 6).code, Result::Code::ArgumentInvalid);
}

class WebGPUStateSanitizerDeviceTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
  }

 protected:
  std::unique_ptr<webgpu::Device> device_;
};

TEST_F(WebGPUStateSanitizerDeviceTest, ColorTargetBlendability) {
  const auto& features = device_->getDeviceFeatureSet();
  WGPUColorTargetState target = {};
  WGPUBlendState blend = {};
  ASSERT_TRUE(webgpu::makeColorTargetState({.textureFormat = TextureFormat::RGBA_UNorm8,
                                            .colorWriteMask = kColorWriteBitsRed,
                                            .blendEnabled = true},
                                           features,
                                           target,
                                           blend)
                  .isOk());
  EXPECT_EQ(target.format, WGPUTextureFormat_RGBA8Unorm);
  EXPECT_EQ(target.writeMask, WGPUColorWriteMask_Red);
  EXPECT_EQ(target.blend, &blend);

  ASSERT_TRUE(webgpu::makeColorTargetState(
                  {.textureFormat = TextureFormat::R_UInt32}, features, target, blend)
                  .isOk());
  EXPECT_EQ(target.blend, nullptr);
  EXPECT_EQ(
      webgpu::makeColorTargetState(
          {.textureFormat = TextureFormat::R_UInt32, .blendEnabled = true}, features, target, blend)
          .code,
      Result::Code::Unsupported);

  const Result float32 = webgpu::makeColorTargetState(
      {.textureFormat = TextureFormat::RGBA_F32, .blendEnabled = true}, features, target, blend);
  EXPECT_EQ(float32.isOk(), features.hasWGPUFeature(WGPUFeatureName_Float32Blendable));

  EXPECT_EQ(webgpu::makeColorTargetState({.textureFormat = TextureFormat::RGBA_UNorm8,
                                          .blendEnabled = true,
                                          .srcRGBBlendFactor = BlendFactor::Src1Color},
                                         features,
                                         target,
                                         blend)
                .code,
            Result::Code::Unsupported);
  EXPECT_EQ(webgpu::makeColorTargetState(
                {.textureFormat = TextureFormat::RGBA_BC7_UNORM_4x4}, features, target, blend)
                .code,
            Result::Code::Unsupported);
}

TEST_F(WebGPUStateSanitizerDeviceTest, StateObjects) {
  Result ret;
  EXPECT_NE(device_->createDepthStencilState({.compareFunction = CompareFunction::Less}, &ret),
            nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  EXPECT_NE(
      device_->createVertexInputState(makeVertexInput(16, VertexAttributeFormat::Float4, 0), &ret),
      nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(device_->createVertexInputState(
                makeVertexInput(6, VertexAttributeFormat::UShort2Norm, 0), &ret),
            nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

} // namespace igl::tests
