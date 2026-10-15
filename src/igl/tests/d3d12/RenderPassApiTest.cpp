/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if defined(IGL_D3D12_TEST)

#include <gtest/gtest.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/ComputePipelineState.h>
#include <igl/NameHandle.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/ShaderCreator.h>
#include <igl/VertexInputState.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/RenderPassMapping.h>
#include <igl/d3d12/Texture.h>
#include <igl/tests/util/device/d3d12/TestDevice.h>

// Declares ID3D12InfoQueue; needs the windows.h that the D3D12 headers above pull in.
#include <d3d12sdklayers.h>

namespace igl::tests {

namespace {

constexpr uint32_t kTargetSize = 8;
constexpr uint32_t kTextureSize = 4;
constexpr uint32_t kRed = 0xFF0000FFu;
constexpr uint32_t kGreen = 0xFF00FF00u;
constexpr uint32_t kBlue = 0xFFFF0000u;

constexpr Color kRedColor = {1.0f, 0.0f, 0.0f, 1.0f};
constexpr Color kBlueColor = {0.0f, 0.0f, 1.0f, 1.0f};

RenderPassDesc makeColorPass(LoadAction loadAction, StoreAction storeAction, const Color& color) {
  RenderPassDesc renderPass;
  renderPass.colorAttachments.push_back(
      {.loadAction = loadAction, .storeAction = storeAction, .clearColor = color});
  return renderPass;
}

// Positions are float4 and texture coordinates float2, for the simple texturing shader. Every
// vertex samples the center of the 4x4 solid color textures.
std::vector<float> makeQuadPositions(float left, float right) {
  // clang-format off
  return {
      left,  -1.0f, 0.0f, 1.0f,
      right, -1.0f, 0.0f, 1.0f,
      right,  1.0f, 0.0f, 1.0f,
      left,  -1.0f, 0.0f, 1.0f,
      right,  1.0f, 0.0f, 1.0f,
      left,   1.0f, 0.0f, 1.0f,
  };
  // clang-format on
}

std::vector<float> makeQuadUvs() {
  return std::vector<float>(12, 0.5f);
}

} // namespace

TEST(RenderPassMappingTest, LoadActionsMapToBeginningAccess) {
  EXPECT_EQ(d3d12::toBeginningAccessType(LoadAction::Clear),
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR);
  EXPECT_EQ(d3d12::toBeginningAccessType(LoadAction::Load),
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE);
  EXPECT_EQ(d3d12::toBeginningAccessType(LoadAction::DontCare),
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD);
}

TEST(RenderPassMappingTest, StoreActionsMapToEndingAccess) {
  for (const bool hasResolveTexture : {false, true}) {
    EXPECT_EQ(d3d12::toEndingAccessType(StoreAction::Store, hasResolveTexture),
              D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
    EXPECT_EQ(d3d12::toEndingAccessType(StoreAction::DontCare, hasResolveTexture),
              D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD);
  }
  EXPECT_EQ(d3d12::toEndingAccessType(StoreAction::MsaaResolve, true),
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE);
}

TEST(RenderPassMappingTest, MsaaResolveWithoutResolveTextureDiscards) {
  EXPECT_EQ(d3d12::toEndingAccessType(StoreAction::MsaaResolve, false),
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD);
}

TEST(RenderPassMappingTest, ColorTargetCoversEveryActionCombination) {
  const D3D12_CPU_DESCRIPTOR_HANDLE rtv = {.ptr = 0x1000};
  for (const LoadAction loadAction : {LoadAction::DontCare, LoadAction::Load, LoadAction::Clear}) {
    for (const StoreAction storeAction :
         {StoreAction::DontCare, StoreAction::Store, StoreAction::MsaaResolve}) {
      for (const bool hasResolveTexture : {false, true}) {
        const RenderPassDesc::AttachmentDesc desc = {.loadAction = loadAction,
                                                     .storeAction = storeAction,
                                                     .clearColor = {0.25f, 0.5f, 0.75f, 1.0f}};
        const auto target = d3d12::makeColorRenderTargetDesc(
            rtv, &desc, DXGI_FORMAT_R8G8B8A8_UNORM, hasResolveTexture);

        EXPECT_EQ(target.cpuDescriptor.ptr, rtv.ptr);
        EXPECT_EQ(target.BeginningAccess.Type, d3d12::toBeginningAccessType(loadAction));
        EXPECT_EQ(target.EndingAccess.Type,
                  d3d12::toEndingAccessType(storeAction, hasResolveTexture));
      }
    }
  }
}

TEST(RenderPassMappingTest, ColorTargetClearCarriesColorAndFormat) {
  const RenderPassDesc::AttachmentDesc desc = {.loadAction = LoadAction::Clear,
                                               .storeAction = StoreAction::Store,
                                               .clearColor = {0.25f, 0.5f, 0.75f, 1.0f}};
  const auto target =
      d3d12::makeColorRenderTargetDesc({}, &desc, DXGI_FORMAT_B8G8R8A8_UNORM, false);

  ASSERT_EQ(target.BeginningAccess.Type, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR);
  const D3D12_CLEAR_VALUE& clearValue = target.BeginningAccess.Clear.ClearValue;
  EXPECT_EQ(clearValue.Format, DXGI_FORMAT_B8G8R8A8_UNORM);
  EXPECT_EQ(clearValue.Color[0], 0.25f);
  EXPECT_EQ(clearValue.Color[1], 0.5f);
  EXPECT_EQ(clearValue.Color[2], 0.75f);
  EXPECT_EQ(clearValue.Color[3], 1.0f);
}

TEST(RenderPassMappingTest, ColorTargetWithoutDescPreservesContents) {
  const auto target =
      d3d12::makeColorRenderTargetDesc({}, nullptr, DXGI_FORMAT_R8G8B8A8_UNORM, true);

  EXPECT_EQ(target.BeginningAccess.Type, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE);
  EXPECT_EQ(target.EndingAccess.Type, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
}

TEST(RenderPassMappingTest, DepthOnlyFormatHasNoStencilAccess) {
  for (const TextureFormat format : {TextureFormat::Z_UNorm16, TextureFormat::Z_UNorm32}) {
    const RenderPassDesc::AttachmentDesc depth = {
        .loadAction = LoadAction::Clear, .storeAction = StoreAction::Store, .clearDepth = 0.5f};
    const RenderPassDesc::AttachmentDesc stencil = {
        .loadAction = LoadAction::Clear, .storeAction = StoreAction::Store, .clearStencil = 7};
    const auto target = d3d12::makeDepthStencilDesc(
        {}, depth, stencil, format, d3d12::textureFormatToDXGIFormat(format), false);

    EXPECT_EQ(target.DepthBeginningAccess.Type, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR);
    EXPECT_EQ(target.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth, 0.5f);
    EXPECT_EQ(target.DepthEndingAccess.Type, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
    EXPECT_EQ(target.StencilBeginningAccess.Type,
              D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS);
    EXPECT_EQ(target.StencilEndingAccess.Type, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
  }
}

TEST(RenderPassMappingTest, DepthStencilFormatMapsBothPlanes) {
  const RenderPassDesc::AttachmentDesc depth = {.loadAction = LoadAction::Load,
                                                .storeAction = StoreAction::DontCare};
  const RenderPassDesc::AttachmentDesc stencil = {
      .loadAction = LoadAction::Clear, .storeAction = StoreAction::Store, .clearStencil = 7};
  const TextureFormat format = TextureFormat::S8_UInt_Z32_UNorm;
  const auto target = d3d12::makeDepthStencilDesc(
      {}, depth, stencil, format, d3d12::textureFormatToDXGIFormat(format), false);

  EXPECT_EQ(target.DepthBeginningAccess.Type, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE);
  EXPECT_EQ(target.DepthEndingAccess.Type, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD);
  ASSERT_EQ(target.StencilBeginningAccess.Type, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR);
  EXPECT_EQ(target.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil, 7);
  EXPECT_EQ(target.StencilBeginningAccess.Clear.ClearValue.Format,
            DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
  EXPECT_EQ(target.StencilEndingAccess.Type, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
}

TEST(RenderPassMappingTest, DepthResolvesAfterThePass) {
  const RenderPassDesc::AttachmentDesc depth = {.loadAction = LoadAction::Clear,
                                                .storeAction = StoreAction::MsaaResolve};
  const TextureFormat format = TextureFormat::Z_UNorm32;
  const DXGI_FORMAT dxgiFormat = d3d12::textureFormatToDXGIFormat(format);

  // The contents have to survive the pass for the resolve that follows it.
  EXPECT_EQ(d3d12::makeDepthStencilDesc({}, depth, depth, format, dxgiFormat, true)
                .DepthEndingAccess.Type,
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
  EXPECT_EQ(d3d12::makeDepthStencilDesc({}, depth, depth, format, dxgiFormat, false)
                .DepthEndingAccess.Type,
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD);
}

TEST(RenderPassMappingTest, ContinuationPreservesAndKeepsEndingAccess) {
  const RenderPassDesc::AttachmentDesc desc = {.loadAction = LoadAction::Clear,
                                               .storeAction = StoreAction::DontCare};
  const auto target =
      d3d12::makeColorRenderTargetDesc({.ptr = 0x1000}, &desc, DXGI_FORMAT_R8G8B8A8_UNORM, false);
  const auto resumed = d3d12::asContinuation(target);
  EXPECT_EQ(resumed.cpuDescriptor.ptr, target.cpuDescriptor.ptr);
  EXPECT_EQ(resumed.BeginningAccess.Type, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE);
  EXPECT_EQ(resumed.EndingAccess.Type, target.EndingAccess.Type);

  const TextureFormat format = TextureFormat::Z_UNorm32;
  const auto depthStencil = d3d12::makeDepthStencilDesc(
      {}, desc, desc, format, d3d12::textureFormatToDXGIFormat(format), false);
  const auto resumedDepthStencil = d3d12::asContinuation(depthStencil);
  EXPECT_EQ(resumedDepthStencil.DepthBeginningAccess.Type,
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE);
  // No stencil plane: still no access.
  EXPECT_EQ(resumedDepthStencil.StencilBeginningAccess.Type,
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS);
  EXPECT_EQ(resumedDepthStencil.DepthEndingAccess.Type, depthStencil.DepthEndingAccess.Type);
}

TEST(RenderPassMappingTest, IntegerFormatsDoNotResolveInThePass) {
  EXPECT_TRUE(d3d12::canResolveInRenderPass(TextureFormat::RGBA_UNorm8));
  EXPECT_TRUE(d3d12::canResolveInRenderPass(TextureFormat::RGBA_SRGB));
  EXPECT_TRUE(d3d12::canResolveInRenderPass(TextureFormat::RGBA_F16));
  EXPECT_FALSE(d3d12::canResolveInRenderPass(TextureFormat::R_UInt32));
  EXPECT_FALSE(d3d12::canResolveInRenderPass(TextureFormat::RGBA_UInt32));
  EXPECT_FALSE(d3d12::canResolveInRenderPass(TextureFormat::Z_UNorm32));
}

TEST(RenderPassMappingTest, AutoModeNeedsTileBasedRendererWithValidRenderPasses) {
  using d3d12::RenderPassMode;
  using d3d12::shouldUseRenderPassApi;
  constexpr auto kTier0 = D3D12_RENDER_PASS_TIER_0;
  constexpr auto kTier1 = D3D12_RENDER_PASS_TIER_1;
  constexpr auto kTier2 = D3D12_RENDER_PASS_TIER_2;

  EXPECT_TRUE(shouldUseRenderPassApi(RenderPassMode::Auto, kTier1, true, true, true));
  EXPECT_TRUE(shouldUseRenderPassApi(RenderPassMode::Auto, kTier2, true, true, true));
  EXPECT_FALSE(shouldUseRenderPassApi(RenderPassMode::Auto, kTier0, true, true, true));
  EXPECT_FALSE(shouldUseRenderPassApi(RenderPassMode::Auto, kTier1, false, true, true));
  EXPECT_FALSE(shouldUseRenderPassApi(RenderPassMode::Auto, kTier1, true, true, false));
  // An OPTIONS18 query that failed leaves RenderPassesValid unknown, which keeps Auto off.
  EXPECT_FALSE(shouldUseRenderPassApi(RenderPassMode::Auto, kTier1, true, false, false));
  EXPECT_FALSE(shouldUseRenderPassApi(RenderPassMode::Auto, kTier1, true, false, true));
}

TEST(RenderPassMappingTest, ForcedModesIgnoreCapabilities) {
  using d3d12::RenderPassMode;
  using d3d12::shouldUseRenderPassApi;

  EXPECT_TRUE(shouldUseRenderPassApi(
      RenderPassMode::ForceOn, D3D12_RENDER_PASS_TIER_0, false, true, false));
  EXPECT_FALSE(
      shouldUseRenderPassApi(RenderPassMode::ForceOff, D3D12_RENDER_PASS_TIER_2, true, true, true));
}

TEST(RenderPassMappingTest, ParsesRenderPassModeNames) {
  using d3d12::parseRenderPassMode;
  using d3d12::RenderPassMode;

  EXPECT_EQ(parseRenderPassMode("auto", RenderPassMode::ForceOn), RenderPassMode::Auto);
  EXPECT_EQ(parseRenderPassMode("on", RenderPassMode::Auto), RenderPassMode::ForceOn);
  EXPECT_EQ(parseRenderPassMode("ON", RenderPassMode::Auto), RenderPassMode::ForceOn);
  EXPECT_EQ(parseRenderPassMode("Off", RenderPassMode::Auto), RenderPassMode::ForceOff);
  // Anything else keeps the configured mode.
  EXPECT_EQ(parseRenderPassMode("", RenderPassMode::ForceOn), RenderPassMode::ForceOn);
  EXPECT_EQ(parseRenderPassMode("maybe", RenderPassMode::ForceOff), RenderPassMode::ForceOff);
  EXPECT_EQ(parseRenderPassMode(nullptr, RenderPassMode::ForceOff), RenderPassMode::ForceOff);
}

TEST(RenderPassMappingTest, ContextConfigDefaultsToAutoMode) {
  EXPECT_EQ(d3d12::D3D12ContextConfig{}.renderPassMode, d3d12::RenderPassMode::Auto);
}

TEST(RenderPassTextureTest, HasReadStateFollowsTheTrackedState) {
  setDebugBreakEnabled(false);
  const auto device = util::device::d3d12::createTestDevice(d3d12::D3D12ContextConfig{});
  ASSERT_NE(device, nullptr);

  Result ret;
  const auto texture =
      device->createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                               kTextureSize,
                                               kTextureSize,
                                               TextureDesc::TextureUsageBits::Sampled |
                                                   TextureDesc::TextureUsageBits::Attachment),
                            &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(texture, nullptr);
  auto* d3dTexture = static_cast<d3d12::Texture*>(texture.get());

  // A new sampled texture is in D3D12_RESOURCE_STATE_COMMON, which no shader stage reads, unless
  // the render pass API is on: a bind inside a pass cannot record the barrier that leaves COMMON,
  // so there it starts readable by every stage. An upload leaves it readable by pixel shaders only.
  const bool startsReadable = device->isRenderPassApiEnabled();
  EXPECT_EQ(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE), startsReadable);
  EXPECT_EQ(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE), startsReadable);
  EXPECT_TRUE(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_COMMON));
  const std::vector<uint32_t> pixels(kTextureSize * kTextureSize, kRed);
  ASSERT_TRUE(
      texture->upload(TextureRangeDesc::new2D(0, 0, kTextureSize, kTextureSize), pixels.data())
          .isOk());
  EXPECT_TRUE(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
  EXPECT_FALSE(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
  EXPECT_FALSE(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));

  // A state that combines both shader stages includes each of them, and a copy destination none.
  const auto queue = device->createCommandQueue({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(queue, nullptr);
  const auto commandBuffer = queue->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(commandBuffer, nullptr);
  auto& d3dCommandBuffer = static_cast<d3d12::CommandBuffer&>(*commandBuffer);
  ASSERT_TRUE(d3dCommandBuffer.begin().isOk());
  ID3D12GraphicsCommandList* commandList = d3dCommandBuffer.getCommandList();
  d3dTexture->transitionAll(commandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
  EXPECT_TRUE(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
  EXPECT_TRUE(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
  EXPECT_TRUE(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE));
  d3dTexture->transitionAll(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
  EXPECT_FALSE(d3dTexture->hasReadState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
  queue->submit(*commandBuffer);
  commandBuffer->waitUntilCompleted();

  // A new depth-stencil texture is never readable as depth. It follows the same rule as a color
  // texture for the shader stages.
  const auto depthTexture =
      device->createTexture(TextureDesc::new2D(TextureFormat::S8_UInt_Z32_UNorm,
                                               kTextureSize,
                                               kTextureSize,
                                               TextureDesc::TextureUsageBits::Sampled |
                                                   TextureDesc::TextureUsageBits::Attachment),
                            &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(depthTexture, nullptr);
  const auto* d3dDepthTexture = static_cast<const d3d12::Texture*>(depthTexture.get());
  EXPECT_FALSE(d3dDepthTexture->hasReadState(D3D12_RESOURCE_STATE_DEPTH_READ));
  EXPECT_EQ(d3dDepthTexture->hasReadState(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE),
            startsReadable);
  EXPECT_TRUE(d3dDepthTexture->hasReadState(D3D12_RESOURCE_STATE_COMMON));
}

// Runs once with the render pass API forced off and once with it forced on. Both must draw the
// same pixels.
class RenderPassApiTest : public ::testing::TestWithParam<d3d12::RenderPassMode> {
 protected:
  void SetUp() override {
    setDebugBreakEnabled(false);

    // The environment variable overrides the configured mode, so it rules out the cases it
    // contradicts. Every other case has to get the mode it asks for.
    if (d3d12::parseRenderPassMode(std::getenv("IGL_D3D12_RENDER_PASS"), GetParam()) !=
        GetParam()) {
      GTEST_SKIP() << "IGL_D3D12_RENDER_PASS overrides the render pass mode of this test";
    }

    d3d12::D3D12ContextConfig config;
    config.renderPassMode = GetParam();
    device_ = util::device::d3d12::createTestDevice(config);
    ASSERT_NE(device_, nullptr);

    Result ret;
    queue_ = device_->createCommandQueue({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(queue_, nullptr);

    createPipeline();
    createGeometry();
    sampler_ = device_->createSamplerState({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(sampler_, nullptr);

    redTexture_ = createSolidTexture(kRed);
    greenTexture_ = createSolidTexture(kGreen);
    ASSERT_NE(redTexture_, nullptr);
    ASSERT_NE(greenTexture_, nullptr);

    colorTexture_ =
        device_->createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                  kTargetSize,
                                                  kTargetSize,
                                                  TextureDesc::TextureUsageBits::Sampled |
                                                      TextureDesc::TextureUsageBits::Attachment),
                               &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(colorTexture_, nullptr);
    framebuffer_ =
        device_->createFramebuffer({.colorAttachments = {{.texture = colorTexture_}}}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(framebuffer_, nullptr);

    // Validation messages of the setup do not belong to the test.
    auto* nativeDevice = static_cast<ID3D12Device*>(device_->getNativeDevice());
    if (nativeDevice &&
        SUCCEEDED(nativeDevice->QueryInterface(IID_PPV_ARGS(infoQueue_.GetAddressOf())))) {
      infoQueue_->ClearStoredMessages();
    }
  }

  // Fails the test on validation errors that the debug layer reported since the last check. The
  // info queue only exists when the debug layer is on.
  void expectNoValidationErrors() {
    if (!infoQueue_) {
      return;
    }
    const UINT64 messageCount = infoQueue_->GetNumStoredMessages();
    for (UINT64 i = 0; i < messageCount; ++i) {
      SIZE_T length = 0;
      if (FAILED(infoQueue_->GetMessage(i, nullptr, &length)) || length == 0) {
        continue;
      }
      std::vector<uint8_t> storage(length);
      auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
      if (FAILED(infoQueue_->GetMessage(i, message, &length))) {
        continue;
      }
      if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
          message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) {
        ADD_FAILURE() << "D3D12 validation error " << static_cast<int>(message->ID) << ": "
                      << message->pDescription;
      }
    }
    infoQueue_->ClearStoredMessages();
  }

  [[nodiscard]] bool forcedOn() const {
    return GetParam() == d3d12::RenderPassMode::ForceOn;
  }

  // Counters only move when the render pass API records the passes.
  [[nodiscard]] size_t whenForcedOn(size_t count) const {
    return forcedOn() ? count : 0;
  }

  void createPipeline() {
    Result ret;
    std::unique_ptr<IShaderStages> stages;
    util::createSimpleShaderStages(device_, stages);
    ASSERT_NE(stages, nullptr);

    const VertexInputStateDesc inputDesc{
        .numAttributes = 2,
        .attributes = {{.bufferIndex = data::shader::kSimplePosIndex,
                        .format = VertexAttributeFormat::Float4,
                        .offset = 0,
                        .name = std::string(data::shader::kSimplePos),
                        .location = 0},
                       {.bufferIndex = data::shader::kSimpleUvIndex,
                        .format = VertexAttributeFormat::Float2,
                        .offset = 0,
                        .name = std::string(data::shader::kSimpleUv),
                        .location = 1}},
        .numInputBindings = 2,
        .inputBindings = {{.stride = sizeof(float) * 4}, {.stride = sizeof(float) * 2}},
    };
    const auto vertexInputState = device_->createVertexInputState(inputDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(vertexInputState, nullptr);

    const RenderPipelineDesc pipelineDesc = {
        .vertexInputState = vertexInputState,
        .shaderStages = std::move(stages),
        .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}}},
        .cullMode = CullMode::Disabled,
        .fragmentUnitSamplerMap = {{0, IGL_NAMEHANDLE(data::shader::kSimpleSampler)}},
    };
    pipeline_ = device_->createRenderPipeline(pipelineDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(pipeline_, nullptr);
  }

  [[nodiscard]] std::unique_ptr<IBuffer> createVertexBuffer(const std::vector<float>& data) {
    Result ret;
    auto buffer = device_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Vertex,
                                                   .data = data.data(),
                                                   .length = sizeof(float) * data.size()},
                                        &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return buffer;
  }

  void createGeometry() {
    leftPositions_ = createVertexBuffer(makeQuadPositions(-1.0f, 0.0f));
    rightPositions_ = createVertexBuffer(makeQuadPositions(0.0f, 1.0f));
    fullPositions_ = createVertexBuffer(makeQuadPositions(-1.0f, 1.0f));
    uvs_ = createVertexBuffer(makeQuadUvs());
    ASSERT_NE(leftPositions_, nullptr);
    ASSERT_NE(rightPositions_, nullptr);
    ASSERT_NE(fullPositions_, nullptr);
    ASSERT_NE(uvs_, nullptr);
  }

  // A 4x4 sampled texture that holds `pixel` everywhere.
  [[nodiscard]] std::shared_ptr<ITexture> createSolidTexture(uint32_t pixel) {
    Result ret;
    auto texture =
        device_->createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                  kTextureSize,
                                                  kTextureSize,
                                                  TextureDesc::TextureUsageBits::Sampled),
                               &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    if (texture) {
      const std::vector<uint32_t> pixels(kTextureSize * kTextureSize, pixel);
      EXPECT_TRUE(
          texture->upload(TextureRangeDesc::new2D(0, 0, kTextureSize, kTextureSize), pixels.data())
              .isOk());
    }
    return texture;
  }

  [[nodiscard]] std::shared_ptr<ICommandBuffer> createCommandBuffer() {
    Result ret;
    auto commandBuffer = queue_->createCommandBuffer({}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return commandBuffer;
  }

  [[nodiscard]] std::unique_ptr<IRenderCommandEncoder> createEncoder(
      ICommandBuffer& commandBuffer,
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer) {
    Result ret;
    auto encoder = commandBuffer.createRenderCommandEncoder(renderPass, framebuffer, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return encoder;
  }

  void bindGeometry(IRenderCommandEncoder& encoder, IBuffer& positions) {
    encoder.bindRenderPipelineState(pipeline_);
    encoder.bindVertexBuffer(data::shader::kSimplePosIndex, positions);
    encoder.bindVertexBuffer(data::shader::kSimpleUvIndex, *uvs_);
    encoder.bindSamplerState(0, BindTarget::kFragment, sampler_.get());
  }

  // Draws one quad textured with `texture`.
  void drawQuad(IRenderCommandEncoder& encoder, IBuffer& positions, ITexture& texture) {
    bindGeometry(encoder, positions);
    encoder.bindTexture(0, &texture);
    encoder.draw(6);
  }

  // Leaves `texture` in COPY_DEST, like a copy into it would, so that sampling it afterward needs a
  // resource barrier. The contents stay intact.
  void leaveInCopyDest(ICommandBuffer& commandBuffer, ITexture& texture) {
    auto& d3dCommandBuffer = static_cast<d3d12::CommandBuffer&>(commandBuffer);
    ASSERT_TRUE(d3dCommandBuffer.begin().isOk());
    static_cast<d3d12::Texture&>(texture).transitionAll(d3dCommandBuffer.getCommandList(),
                                                        D3D12_RESOURCE_STATE_COPY_DEST);
  }

  // Everything the test recorded and executed has to validate; reading the pixels back is not part
  // of that.
  void submitAndWait(ICommandBuffer& commandBuffer) {
    queue_->submit(commandBuffer);
    commandBuffer.waitUntilCompleted();
    expectNoValidationErrors();
  }

  [[nodiscard]] std::vector<uint32_t> readPixels(const std::shared_ptr<IFramebuffer>& framebuffer) {
    std::vector<uint32_t> pixels(kTargetSize * kTargetSize);
    framebuffer->copyBytesColorAttachment(
        *queue_, 0, pixels.data(), TextureRangeDesc::new2D(0, 0, kTargetSize, kTargetSize));
    return pixels;
  }

  // Expects `left` in the left half of the target and `right` in the right half.
  void expectHalves(const std::vector<uint32_t>& pixels, uint32_t left, uint32_t right) {
    ASSERT_EQ(pixels.size(), kTargetSize * kTargetSize);
    for (uint32_t y = 0; y < kTargetSize; ++y) {
      for (uint32_t x = 0; x < kTargetSize; ++x) {
        const uint32_t expected = x < kTargetSize / 2 ? left : right;
        ASSERT_EQ(pixels[y * kTargetSize + x], expected)
            << "pixel (" << x << ", " << y << ") is 0x" << std::hex << pixels[y * kTargetSize + x];
      }
    }
  }

  [[nodiscard]] size_t beginCount() const {
    return device_->getTelemetry().getRenderPassBeginCount();
  }
  [[nodiscard]] size_t splitCount() const {
    return device_->getTelemetry().getRenderPassSplitCount();
  }

  std::shared_ptr<d3d12::Device> device_;
  std::shared_ptr<ICommandQueue> queue_;
  igl::d3d12::ComPtr<ID3D12InfoQueue> infoQueue_;

  std::shared_ptr<IRenderPipelineState> pipeline_;
  std::shared_ptr<ISamplerState> sampler_;
  std::unique_ptr<IBuffer> leftPositions_;
  std::unique_ptr<IBuffer> rightPositions_;
  std::unique_ptr<IBuffer> fullPositions_;
  std::unique_ptr<IBuffer> uvs_;
  std::shared_ptr<ITexture> redTexture_;
  std::shared_ptr<ITexture> greenTexture_;

  std::shared_ptr<ITexture> colorTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;
};

TEST_P(RenderPassApiTest, ModeSelectsTheRenderPassApi) {
  EXPECT_EQ(device_->isRenderPassApiEnabled(), forcedOn());
  EXPECT_EQ(beginCount(), 0u);

  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kRedColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  EXPECT_EQ(beginCount(), whenForcedOn(1));
  EXPECT_EQ(splitCount(), 0u);
}

TEST_P(RenderPassApiTest, ClearsTheColorAttachment) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kRedColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kRed, kRed);
}

TEST_P(RenderPassApiTest, LoadPreservesTheContentsOfAnEarlierPass) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);

  const auto first = createEncoder(*commandBuffer,
                                   makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                                   framebuffer_);
  ASSERT_NE(first, nullptr);
  drawQuad(*first, *leftPositions_, *redTexture_);
  first->endEncoding();

  const auto second = createEncoder(*commandBuffer,
                                    makeColorPass(LoadAction::Load, StoreAction::Store, kBlueColor),
                                    framebuffer_);
  ASSERT_NE(second, nullptr);
  drawQuad(*second, *rightPositions_, *greenTexture_);
  second->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kRed, kGreen);
  EXPECT_EQ(beginCount(), whenForcedOn(2));
  EXPECT_EQ(splitCount(), 0u);
}

TEST_P(RenderPassApiTest, DontCarePassThenClearPass) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);

  const auto first =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::DontCare, StoreAction::DontCare, kBlueColor),
                    framebuffer_);
  ASSERT_NE(first, nullptr);
  drawQuad(*first, *leftPositions_, *greenTexture_);
  first->endEncoding();

  // Only the clear decides what the target holds afterward.
  const auto second =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                    framebuffer_);
  ASSERT_NE(second, nullptr);
  second->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kBlue, kBlue);
}

TEST_P(RenderPassApiTest, MissingColorEntryPreservesTheContents) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);

  const auto first = createEncoder(*commandBuffer,
                                   makeColorPass(LoadAction::Clear, StoreAction::Store, kRedColor),
                                   framebuffer_);
  ASSERT_NE(first, nullptr);
  first->endEncoding();

  const auto second = createEncoder(*commandBuffer, RenderPassDesc{}, framebuffer_);
  ASSERT_NE(second, nullptr);
  second->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kRed, kRed);
}

TEST_P(RenderPassApiTest, DepthAndStencilAttachmentsBeginAndEnd) {
  for (const TextureFormat depthFormat :
       {TextureFormat::Z_UNorm32, TextureFormat::S8_UInt_Z32_UNorm}) {
    Result ret;
    TextureDesc depthDesc = TextureDesc::new2D(depthFormat,
                                               kTargetSize,
                                               kTargetSize,
                                               TextureDesc::TextureUsageBits::Sampled |
                                                   TextureDesc::TextureUsageBits::Attachment);
    depthDesc.storage = ResourceStorage::Private;
    const auto depthTexture = device_->createTexture(depthDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(depthTexture, nullptr);
    const auto framebuffer =
        device_->createFramebuffer({.colorAttachments = {{.texture = colorTexture_}},
                                    .depthAttachment = {.texture = depthTexture},
                                    .stencilAttachment = {.texture = depthTexture}},
                                   &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(framebuffer, nullptr);

    RenderPassDesc renderPass = makeColorPass(LoadAction::Clear, StoreAction::Store, kRedColor);
    renderPass.depthAttachment = {
        .loadAction = LoadAction::Clear, .storeAction = StoreAction::Store, .clearDepth = 0.5f};
    renderPass.stencilAttachment = {
        .loadAction = LoadAction::Clear, .storeAction = StoreAction::Store, .clearStencil = 1};

    const auto commandBuffer = createCommandBuffer();
    ASSERT_NE(commandBuffer, nullptr);
    const auto encoder = createEncoder(*commandBuffer, renderPass, framebuffer);
    ASSERT_NE(encoder, nullptr);
    encoder->endEncoding();
    submitAndWait(*commandBuffer);

    expectHalves(readPixels(framebuffer), kRed, kRed);
  }
}

TEST_P(RenderPassApiTest, MsaaResolveWritesTheResolveTexture) {
  size_t maxSamples = 0;
  if (!device_->getFeatureLimits(DeviceFeatureLimits::MaxMultisampleCount, maxSamples) ||
      maxSamples < 4) {
    GTEST_SKIP() << "4x multisampling is not supported";
  }

  Result ret;
  const TextureDesc msaaDesc{
      .width = kTargetSize,
      .height = kTargetSize,
      .numSamples = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };
  const auto msaaTexture = device_->createTexture(msaaDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(msaaTexture, nullptr);

  const auto resolveFramebuffer = device_->createFramebuffer(
      {.colorAttachments = {{.texture = msaaTexture, .resolveTexture = colorTexture_}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(resolveFramebuffer, nullptr);

  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::MsaaResolve, kRedColor),
                    resolveFramebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  // The resolve texture is the color target of `framebuffer_`.
  expectHalves(readPixels(framebuffer_), kRed, kRed);
  EXPECT_EQ(beginCount(), whenForcedOn(1));
}

TEST_P(RenderPassApiTest, BindingBeforeTheFirstDrawDoesNotSplitThePass) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  leaveInCopyDest(*commandBuffer, *greenTexture_);

  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  // The barrier this bind needs is recorded before the pass begins at the draw.
  drawQuad(*encoder, *rightPositions_, *greenTexture_);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kBlue, kGreen);
  EXPECT_EQ(splitCount(), 0u);
  EXPECT_EQ(beginCount(), whenForcedOn(1));
}

TEST_P(RenderPassApiTest, BindingAfterADrawSplitsThePass) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  leaveInCopyDest(*commandBuffer, *greenTexture_);

  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  drawQuad(*encoder, *leftPositions_, *redTexture_);
  // This texture needs a barrier, which cannot be recorded inside the pass that the draw began.
  drawQuad(*encoder, *rightPositions_, *greenTexture_);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kRed, kGreen);
  EXPECT_EQ(splitCount(), whenForcedOn(1));
  EXPECT_EQ(beginCount(), whenForcedOn(2));
}

TEST_P(RenderPassApiTest, BindGroupAfterADrawSplitsThePass) {
  Result ret;
  const auto bindGroup = device_->createBindGroup(
      BindGroupTextureDesc{.textures = {greenTexture_}, .samplers = {sampler_}}, nullptr, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  leaveInCopyDest(*commandBuffer, *greenTexture_);

  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  drawQuad(*encoder, *leftPositions_, *redTexture_);
  bindGeometry(*encoder, *rightPositions_);
  encoder->bindBindGroup(bindGroup);
  encoder->draw(6);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kRed, kGreen);
  EXPECT_EQ(splitCount(), whenForcedOn(1));
  EXPECT_EQ(beginCount(), whenForcedOn(2));
}

TEST_P(RenderPassApiTest, ScissorSurvivesASplit) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  leaveInCopyDest(*commandBuffer, *greenTexture_);

  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  encoder->bindScissorRect({.x = 0, .y = 0, .width = kTargetSize / 2, .height = kTargetSize});
  drawQuad(*encoder, *fullPositions_, *redTexture_);
  // The second half of the pass keeps the scissor, so only the left half turns green.
  drawQuad(*encoder, *fullPositions_, *greenTexture_);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kGreen, kBlue);
  EXPECT_EQ(splitCount(), whenForcedOn(1));
}

TEST_P(RenderPassApiTest, SampledDepthAfterADrawDoesNotSplit) {
  Result ret;
  TextureDesc depthDesc = TextureDesc::new2D(TextureFormat::Z_UNorm32,
                                             kTargetSize,
                                             kTargetSize,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
  depthDesc.storage = ResourceStorage::Private;
  const auto depthTexture = device_->createTexture(depthDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const auto auxColor =
      device_->createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                kTargetSize,
                                                kTargetSize,
                                                TextureDesc::TextureUsageBits::Sampled |
                                                    TextureDesc::TextureUsageBits::Attachment),
                             &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const auto depthFramebuffer = device_->createFramebuffer(
      {.colorAttachments = {{.texture = auxColor}}, .depthAttachment = {.texture = depthTexture}},
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  RenderPassDesc depthPass = makeColorPass(LoadAction::Clear, StoreAction::Store, kRedColor);
  depthPass.depthAttachment = {
      .loadAction = LoadAction::Clear, .storeAction = StoreAction::Store, .clearDepth = 0.25f};

  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  const auto depthEncoder = createEncoder(*commandBuffer, depthPass, depthFramebuffer);
  ASSERT_NE(depthEncoder, nullptr);
  depthEncoder->endEncoding();

  // After a render pass, depth texture should be in a readable state.
  if (forcedOn()) {
    auto* d3dDepth = static_cast<d3d12::Texture*>(depthTexture.get());
    const auto depthState = d3dDepth->getSubresourceState();
    EXPECT_EQ(depthState,
              D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
  }

  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  drawQuad(*encoder, *leftPositions_, *redTexture_);
  drawQuad(*encoder, *rightPositions_, *depthTexture);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  const auto pixels = readPixels(framebuffer_);
  EXPECT_EQ(pixels[0], kRed);
  EXPECT_EQ(pixels[kTargetSize - 1], 0xFF000040u);
  EXPECT_EQ(splitCount(), 0u);
  EXPECT_EQ(beginCount(), whenForcedOn(2));
}

TEST_P(RenderPassApiTest, ComputeOutputAfterADrawDoesNotSplit) {
  if (!device_->hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "Compute not supported";
  }

  constexpr std::string_view kImageStoreShader =
      "RWTexture2D<float4> destImage : register(u0);\n"
      "RWStructuredBuffer<float4> colorBuf : register(u1);\n"
      "[numthreads(1, 1, 1)]\n"
      "void imageStoreOrange(uint3 gid : SV_DispatchThreadID) {\n"
      "  destImage[gid.xy] = colorBuf[0];\n"
      "}\n";

  Result ret;
  const auto computeOut =
      device_->createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                kTextureSize,
                                                kTextureSize,
                                                TextureDesc::TextureUsageBits::Sampled |
                                                    TextureDesc::TextureUsageBits::Storage),
                             &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  const float orange[4] = {1.0f, 128.0f / 255.0f, 0.0f, 1.0f};
  const BufferDesc colorBufDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .data = orange,
      .length = sizeof(orange),
      .storageStride = sizeof(orange),
  };
  const auto colorBuffer = device_->createBuffer(colorBufDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto computeStages = ShaderStagesCreator::fromModuleStringInput(
      *device_, kImageStoreShader.data(), "imageStoreOrange", "", &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  const ComputePipelineDesc computeDesc{
      .buffersMap = {{1, IGL_NAMEHANDLE("colorBuf")}},
      .shaderStages = std::move(computeStages),
  };
  const auto computePipeline = device_->createComputePipeline(computeDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  {
    auto computeEncoder = commandBuffer->createComputeCommandEncoder();
    ASSERT_NE(computeEncoder, nullptr);
    computeEncoder->bindComputePipelineState(computePipeline);
    computeEncoder->bindImageTexture(0, computeOut.get(), TextureFormat::RGBA_UNorm8);
    computeEncoder->bindBuffer(1, colorBuffer.get());
    computeEncoder->dispatchThreadGroups(
        Dimensions(kTextureSize, kTextureSize, 1), Dimensions(1, 1, 1), {});
    computeEncoder->endEncoding();
  }

  // After compute, the output texture should be in a readable state on ForceOn.
  if (forcedOn()) {
    auto* d3dOut = static_cast<d3d12::Texture*>(computeOut.get());
    const auto outState = d3dOut->getSubresourceState();
    EXPECT_EQ(outState, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
  }

  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  drawQuad(*encoder, *leftPositions_, *redTexture_);
  drawQuad(*encoder, *rightPositions_, *computeOut);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  const auto pixels = readPixels(framebuffer_);
  EXPECT_EQ(pixels[0], kRed);
  EXPECT_EQ(pixels[kTargetSize - 1], 0xFF0080FFu);
  EXPECT_EQ(splitCount(), 0u);
  EXPECT_EQ(beginCount(), whenForcedOn(1));
}

TEST_P(RenderPassApiTest, DependenciesAvoidTheSplit) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  leaveInCopyDest(*commandBuffer, *redTexture_);
  leaveInCopyDest(*commandBuffer, *greenTexture_);

  Dependencies second;
  second.textures[0] = greenTexture_.get();
  Dependencies first;
  first.textures[0] = redTexture_.get();
  first.next = &second;
  Result ret;
  const auto encoder = commandBuffer->createRenderCommandEncoder(
      makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor), framebuffer_, first, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(encoder, nullptr);
  drawQuad(*encoder, *leftPositions_, *redTexture_);
  drawQuad(*encoder, *rightPositions_, *greenTexture_);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kRed, kGreen);
  EXPECT_EQ(splitCount(), 0u);
  EXPECT_EQ(beginCount(), whenForcedOn(1));
}

TEST_P(RenderPassApiTest, TrailingBindDoesNotOpenAnExtraSegment) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  leaveInCopyDest(*commandBuffer, *greenTexture_);

  const auto encoder =
      createEncoder(*commandBuffer,
                    makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                    framebuffer_);
  ASSERT_NE(encoder, nullptr);
  drawQuad(*encoder, *leftPositions_, *redTexture_);
  encoder->bindTexture(0, greenTexture_.get());
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  expectHalves(readPixels(framebuffer_), kRed, kBlue);
  EXPECT_EQ(splitCount(), whenForcedOn(1));
  EXPECT_EQ(beginCount(), whenForcedOn(1));
}

TEST_P(RenderPassApiTest, EmptyPassesBeginNothing) {
  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);

  // Load/Store with no draws begins nothing.
  {
    const auto encoder =
        createEncoder(*commandBuffer,
                      makeColorPass(LoadAction::Load, StoreAction::Store, kBlueColor),
                      framebuffer_);
    ASSERT_NE(encoder, nullptr);
    encoder->endEncoding();
  }
  EXPECT_EQ(beginCount(), 0u);

  // DontCare/DontCare with no draws begins nothing.
  {
    const auto encoder =
        createEncoder(*commandBuffer,
                      makeColorPass(LoadAction::DontCare, StoreAction::DontCare, kBlueColor),
                      framebuffer_);
    ASSERT_NE(encoder, nullptr);
    encoder->endEncoding();
  }
  EXPECT_EQ(beginCount(), 0u);

  // Clear/Store with no draws begins once on ForceOn.
  {
    const auto encoder =
        createEncoder(*commandBuffer,
                      makeColorPass(LoadAction::Clear, StoreAction::Store, kBlueColor),
                      framebuffer_);
    ASSERT_NE(encoder, nullptr);
    encoder->endEncoding();
  }
  EXPECT_EQ(beginCount(), whenForcedOn(1));
  submitAndWait(*commandBuffer);
  expectHalves(readPixels(framebuffer_), kBlue, kBlue);
}

TEST_P(RenderPassApiTest, CubeFaceResolve) {
  if (!forcedOn()) {
    GTEST_SKIP() << "Cube face resolve only works with render pass API (ForceOn)";
  }

  size_t maxSamples = 0;
  if (!device_->getFeatureLimits(DeviceFeatureLimits::MaxMultisampleCount, maxSamples) ||
      maxSamples < 4) {
    GTEST_SKIP() << "4x multisampling is not supported";
  }

  Result ret;
  TextureDesc msaaDesc = TextureDesc::newCube(TextureFormat::RGBA_UNorm8,
                                              kTargetSize,
                                              kTargetSize,
                                              TextureDesc::TextureUsageBits::Attachment);
  msaaDesc.numSamples = 4;
  msaaDesc.storage = ResourceStorage::Private;
  const auto msaaCube = device_->createTexture(msaaDesc, &ret);
  if (!ret.isOk() || !msaaCube) {
    GTEST_SKIP() << "MSAA cube not supported: " << ret.message;
  }

  const auto resolveCube =
      device_->createTexture(TextureDesc::newCube(TextureFormat::RGBA_UNorm8,
                                                  kTargetSize,
                                                  kTargetSize,
                                                  TextureDesc::TextureUsageBits::Sampled |
                                                      TextureDesc::TextureUsageBits::Attachment),
                             &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  // Every face starts blue, so a face that the resolve must not write has a known value
  const std::vector<uint32_t> pixels(6 * kTargetSize * kTargetSize, kBlue);
  ASSERT_TRUE(
      resolveCube->upload(TextureRangeDesc::newCube(0, 0, kTargetSize, kTargetSize), pixels.data())
          .isOk());

  const auto resolveFramebuffer = device_->createFramebuffer(
      {.colorAttachments = {{.texture = msaaCube, .resolveTexture = resolveCube}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  RenderPassDesc pass = makeColorPass(LoadAction::Clear, StoreAction::MsaaResolve, kRedColor);
  pass.colorAttachments[0].face = 2;

  const auto commandBuffer = createCommandBuffer();
  ASSERT_NE(commandBuffer, nullptr);
  const auto encoder = createEncoder(*commandBuffer, pass, resolveFramebuffer);
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();
  submitAndWait(*commandBuffer);

  const auto readFramebuffer =
      device_->createFramebuffer({.colorAttachments = {{.texture = resolveCube}}}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  uint32_t face0 = 0xDEADBEEF;
  uint32_t face2 = 0xDEADBEEF;
  readFramebuffer->copyBytesColorAttachment(
      *queue_, 0, &face0, TextureRangeDesc::newCubeFace(0, 0, 1, 1, 0u));
  readFramebuffer->copyBytesColorAttachment(
      *queue_, 0, &face2, TextureRangeDesc::newCubeFace(0, 0, 1, 1, 2u));

  EXPECT_EQ(face2, kRed);
  EXPECT_EQ(face0, kBlue);
}

INSTANTIATE_TEST_SUITE_P(RenderPassModes,
                         RenderPassApiTest,
                         ::testing::Values(d3d12::RenderPassMode::ForceOff,
                                           d3d12::RenderPassMode::ForceOn),
                         [](const ::testing::TestParamInfo<d3d12::RenderPassMode>& info) {
                           return std::string(d3d12::renderPassModeName(info.param));
                         });

} // namespace igl::tests

#endif // IGL_D3D12_TEST
