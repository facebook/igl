/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "util/Common.h"

#include <array>
#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/Framebuffer.h>
#include <igl/NameHandle.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/Shader.h>
#include <igl/ShaderCreator.h>
#include <igl/Texture.h>

namespace igl::tests {

namespace {

constexpr uint32_t kSize = 4; // width and height of the test textures

// The vertex shader of the GPU tests: a triangle that covers the framebuffer
constexpr const char* kVertexShader = R"(#version 460
out gl_PerVertex { vec4 gl_Position; };
void main() {
  vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
})";

// Writes (100, 20, 0, 255) to color attachment 1 and (0, 50, 30, 255) to color attachment 2
constexpr const char* kWriteAttachments1And2 = R"(#version 460
layout(location = 1) out vec4 outA;
layout(location = 2) out vec4 outB;
void main() {
  outA = vec4(100.0 / 255.0, 20.0 / 255.0, 0.0, 1.0);
  outB = vec4(0.0, 50.0 / 255.0, 30.0 / 255.0, 1.0);
})";

// Writes the sum of input attachments 1 and 2 to color attachment 0
constexpr const char* kSumInputs1And2 = R"(#version 460
layout(input_attachment_index = 1, set = 2, binding = 17) uniform subpassInput inA;
layout(input_attachment_index = 2, set = 2, binding = 18) uniform subpassInput inB;
layout(location = 0) out vec4 outColor;
void main() {
  outColor = vec4(subpassLoad(inA).rgb + subpassLoad(inB).rgb, 1.0);
})";

// pack(100, 70, 30, 255): kSumInputs1And2 after kWriteAttachments1And2
constexpr uint32_t kSumOfAttachments1And2 = 0xFF1E4664;

// Writes input attachment 1 with red and green swapped to color attachment 3
constexpr const char* kSwapInput1To3 = R"(#version 460
layout(input_attachment_index = 1, set = 2, binding = 17) uniform subpassInput inA;
layout(location = 3) out vec4 outColor;
void main() {
  vec4 a = subpassLoad(inA);
  outColor = vec4(a.g, a.r, 0.0, 1.0);
})";

// Writes the sum of input attachments 2 and 3 to color attachment 0
constexpr const char* kSumInputs2And3 = R"(#version 460
layout(input_attachment_index = 2, set = 2, binding = 18) uniform subpassInput inB;
layout(input_attachment_index = 3, set = 2, binding = 19) uniform subpassInput inC;
layout(location = 0) out vec4 outColor;
void main() {
  outColor = vec4(subpassLoad(inB).rgb + subpassLoad(inC).rgb, 1.0);
})";

// Writes red, green and blue to color attachments 0, 1 and 2
constexpr const char* kWriteRgb = R"(#version 460
layout(location = 0) out vec4 out0;
layout(location = 1) out vec4 out1;
layout(location = 2) out vec4 out2;
void main() {
  out0 = vec4(1.0, 0.0, 0.0, 1.0);
  out1 = vec4(0.0, 1.0, 0.0, 1.0);
  out2 = vec4(0.0, 0.0, 1.0, 1.0);
})";

// Writes red to color attachment 0
constexpr const char* kWriteRed = R"(#version 460
layout(location = 0) out vec4 out0;
void main() {
  out0 = vec4(1.0, 0.0, 0.0, 1.0);
})";

constexpr uint32_t kRed = 0xFF0000FF; // pack(255, 0, 0, 255)
constexpr uint32_t kBlue = 0xFFFF0000; // pack(0, 0, 255, 255)

} // namespace

class SubpassesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);
  }

  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  [[nodiscard]] std::shared_ptr<ITexture> createColorTexture(
      TextureDesc::TextureUsage usage,
      ResourceStorage storage = ResourceStorage::Invalid) const {
    const TextureDesc desc{
        .width = kSize,
        .height = kSize,
        .usage = usage,
        .type = TextureType::TwoD,
        .format = TextureFormat::RGBA_UNorm8,
        .storage = storage,
    };
    Result ret;
    const auto tex = iglDev_->createTexture(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    EXPECT_NE(tex, nullptr);
    return tex;
  }

  [[nodiscard]] std::shared_ptr<IFramebuffer> createFramebuffer(
      const std::vector<std::shared_ptr<ITexture>>& colorAttachments) const {
    FramebufferDesc desc;
    for (size_t i = 0; i < colorAttachments.size() && i < IGL_COLOR_ATTACHMENTS_MAX; ++i) {
      desc.colorAttachments[i].texture = colorAttachments[i];
    }
    Result ret;
    const auto fb = iglDev_->createFramebuffer(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    EXPECT_NE(fb, nullptr);
    return fb;
  }

  [[nodiscard]] static RenderPassDesc makeRenderPass(
      size_t numColorAttachments,
      std::vector<RenderPassDesc::SubpassDesc> subpasses) {
    RenderPassDesc renderPass;
    renderPass.colorAttachments.resize(numColorAttachments);
    for (size_t i = 0; i < numColorAttachments; ++i) {
      renderPass.colorAttachments[i].loadAction = LoadAction::Clear;
      renderPass.colorAttachments[i].storeAction = StoreAction::Store;
    }
    renderPass.subpasses = std::move(subpasses);
    return renderPass;
  }

  // Returns why the GPU tests cannot run on this device, or nullptr if they can
  [[nodiscard]] const char* IGL_NULLABLE gpuTestSkipReason() const {
    if (!iglDev_->hasFeature(DeviceFeatures::Subpasses)) {
      return "Subpasses are not supported";
    }
    if (!iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets)) {
      return "Multiple render targets are not supported";
    }
    if (iglDev_->getBackendType() != BackendType::Vulkan) {
      return "The GPU tests have no shaders for this backend";
    }
    return nullptr;
  }

  // Creates a pipeline with kVertexShader, `fragmentShader`, and an RGBA_UNorm8 format for each of
  // the `numColorAttachments` color attachments
  [[nodiscard]] std::shared_ptr<IRenderPipelineState> createPipeline(const char* fragmentShader,
                                                                     size_t numColorAttachments,
                                                                     const char* debugName) const {
    Result ret;
    std::unique_ptr<IShaderStages> stages = ShaderStagesCreator::fromModuleStringInput(
        *iglDev_, kVertexShader, "main", "", fragmentShader, "main", "", &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    RenderPipelineDesc::TargetDesc targetDesc;
    targetDesc.colorAttachments.resize(numColorAttachments);
    for (auto& attachment : targetDesc.colorAttachments) {
      attachment.textureFormat = TextureFormat::RGBA_UNorm8;
    }
    const RenderPipelineDesc desc{
        .shaderStages = std::move(stages),
        .targetDesc = std::move(targetDesc),
        .debugName = genNameHandle(debugName),
    };
    auto pipeline = iglDev_->createRenderPipeline(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    EXPECT_NE(pipeline, nullptr);
    return pipeline;
  }

  // Encodes `renderPass` with one draw of a triangle per subpass, using the pipeline of that
  // subpass, then submits it and waits for it to complete
  void renderSubpasses(
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      const std::vector<std::shared_ptr<IRenderPipelineState>>& subpassPipelines) const {
    Result ret;
    const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, framebuffer, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(encoder, nullptr);
    for (size_t i = 0; i != subpassPipelines.size(); ++i) {
      if (i != 0) {
        encoder->nextSubpass();
      }
      encoder->bindRenderPipelineState(subpassPipelines[i]);
      encoder->draw(3);
    }
    encoder->endEncoding();
    cmdQueue_->submit(*cmdBuf);
    cmdBuf->waitUntilCompleted();
  }

  // Expects every pixel of `texture` to be `expected`. Reads `texture` through a framebuffer that
  // has it as its only color attachment, because some backends can only read color attachment 0.
  void expectPixels(const std::shared_ptr<ITexture>& texture, uint32_t expected) const {
    const auto framebuffer = createFramebuffer({texture});
    ASSERT_NE(framebuffer, nullptr);
    std::array<uint32_t, kSize * kSize> pixels{};
    framebuffer->copyBytesColorAttachment(*cmdQueue_, 0, pixels.data(), texture->getFullRange(0));
    for (const uint32_t pixel : pixels) {
      EXPECT_EQ(pixel, expected) << std::hex << pixel;
    }
  }
};

TEST_F(SubpassesTest, ValidateAcceptsDeferredLayout) {
  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto att2 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto fb = createFramebuffer({att0, att1, att2});
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass =
      makeRenderPass(3,
                     {{.colorAttachmentMask = 0b110},
                      {.colorAttachmentMask = 0b001, .inputAttachmentMask = 0b110}});

  const Result ret = renderPass.validate(*fb);
  EXPECT_TRUE(ret.isOk());
}

TEST_F(SubpassesTest, ValidateRejectsStereoFramebuffer) {
  if (!iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets)) {
    GTEST_SKIP() << "Device does not support multiple render targets";
  }

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture =
      createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  fbDesc.mode = FramebufferMode::Stereo;
  Result ret;
  const auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result validateRet = renderPass.validate(*fb);
  EXPECT_EQ(validateRet.code, Result::Code::Unsupported);
}

TEST_F(SubpassesTest, ValidateRejectsInputWithoutUsageBit) {
  if (!iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets)) {
    GTEST_SKIP() << "Device does not support multiple render targets";
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = createFramebuffer({att0, att1});
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass = makeRenderPass(
      2,
      {{.colorAttachmentMask = 0b01}, {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0b01}});

  const Result ret = renderPass.validate(*fb);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST_F(SubpassesTest, ValidateRejectsMultisampled) {
  if (!iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets)) {
    GTEST_SKIP() << "Device does not support multiple render targets";
  }

  if (!iglDev_->hasFeature(DeviceFeatures::MultiSample)) {
    GTEST_SKIP() << "MultiSample feature not supported";
  }

  TextureDesc msaaDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Attachment);
  msaaDesc.numSamples = 4;

  Result ret;
  const auto msTex = iglDev_->createTexture(msaaDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(msTex, nullptr);

  const auto fb = createFramebuffer({msTex});
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  const Result validateRet = renderPass.validate(*fb);
  EXPECT_EQ(validateRet.code, Result::Code::Unsupported);
}

TEST_F(SubpassesTest, EncoderRejectsSubpassesWhenUnsupported) {
  if (iglDev_->hasFeature(DeviceFeatures::Subpasses)) {
    GTEST_SKIP() << "Device supports subpasses";
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = createFramebuffer({att0});
  ASSERT_NE(fb, nullptr);

  const RenderPassDesc renderPass =
      makeRenderPass(1, {{.colorAttachmentMask = 0b001}, {.colorAttachmentMask = 0b001}});

  Result ret;
  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(cmdBuf, nullptr);

  const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
  EXPECT_EQ(encoder, nullptr);
}

// GPU tests

TEST_F(SubpassesTest, TwoSubpassesReadInputAttachments) {
  if (const char* reason = gpuTestSkipReason()) {
    GTEST_SKIP() << reason;
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::Sampled);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto att2 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto fb = createFramebuffer({att0, att1, att2});
  ASSERT_NE(fb, nullptr);

  const auto p0 = createPipeline(kWriteAttachments1And2, 3, "subpass 0");
  ASSERT_NE(p0, nullptr);
  const auto p1 = createPipeline(kSumInputs1And2, 3, "subpass 1");
  ASSERT_NE(p1, nullptr);

  const RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::DontCare}},
      .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
          {.colorAttachmentMask = 0b110, .inputAttachmentMask = 0},
          {.colorAttachmentMask = 0b001, .inputAttachmentMask = 0b110}}};

  renderSubpasses(renderPass, fb, {p0, p1});
  expectPixels(att0, kSumOfAttachments1And2);
}

TEST_F(SubpassesTest, TwoSubpassesWithMemorylessInputs) {
  if (const char* reason = gpuTestSkipReason()) {
    GTEST_SKIP() << reason;
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::Sampled);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                           TextureDesc::TextureUsageBits::InputAttachment,
                                       ResourceStorage::Memoryless);
  const auto att2 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                           TextureDesc::TextureUsageBits::InputAttachment,
                                       ResourceStorage::Memoryless);
  const auto fb = createFramebuffer({att0, att1, att2});
  ASSERT_NE(fb, nullptr);

  const auto p0 = createPipeline(kWriteAttachments1And2, 3, "sp0_mem");
  ASSERT_NE(p0, nullptr);
  const auto p1 = createPipeline(kSumInputs1And2, 3, "sp1_mem");
  ASSERT_NE(p1, nullptr);

  const RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::DontCare},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::DontCare}},
      .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
          {.colorAttachmentMask = 0b110, .inputAttachmentMask = 0},
          {.colorAttachmentMask = 0b001, .inputAttachmentMask = 0b110}}};

  // Render two frames: the memoryless attachments are cleared in each frame
  for (int frame = 0; frame != 2; ++frame) {
    SCOPED_TRACE(frame);
    renderSubpasses(renderPass, fb, {p0, p1});
    expectPixels(att0, kSumOfAttachments1And2);
  }
}

TEST_F(SubpassesTest, ThreeSubpassChainPreservesAttachment) {
  if (const char* reason = gpuTestSkipReason()) {
    GTEST_SKIP() << reason;
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::Sampled);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto att2 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto att3 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto fb = createFramebuffer({att0, att1, att2, att3});
  ASSERT_NE(fb, nullptr);

  const auto p0 = createPipeline(kWriteAttachments1And2, 4, "chain_sp0");
  ASSERT_NE(p0, nullptr);
  const auto p1 = createPipeline(kSwapInput1To3, 4, "chain_sp1");
  ASSERT_NE(p1, nullptr);
  const auto p2 = createPipeline(kSumInputs2And3, 4, "chain_sp2");
  ASSERT_NE(p2, nullptr);

  const RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::DontCare},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::DontCare},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::DontCare}},
      .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
          {.colorAttachmentMask = 0b0110, .inputAttachmentMask = 0},
          {.colorAttachmentMask = 0b1000, .inputAttachmentMask = 0b0010},
          {.colorAttachmentMask = 0b0001, .inputAttachmentMask = 0b1100}}};

  renderSubpasses(renderPass, fb, {p0, p1, p2});
  expectPixels(att0, 0xFF1E9614); // pack(20, 150, 30, 255)
}

TEST_F(SubpassesTest, FramebufferReusedAcrossSingleAndMultiSubpassPasses) {
  if (const char* reason = gpuTestSkipReason()) {
    GTEST_SKIP() << reason;
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::Sampled);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto att2 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::InputAttachment);
  const auto fb = createFramebuffer({att0, att1, att2});
  ASSERT_NE(fb, nullptr);

  const auto pSingle = createPipeline(kWriteRgb, 3, "single subpass");
  ASSERT_NE(pSingle, nullptr);
  const auto p0 = createPipeline(kWriteAttachments1And2, 3, "reuse_sp0");
  ASSERT_NE(p0, nullptr);
  const auto p1 = createPipeline(kSumInputs1And2, 3, "reuse_sp1");
  ASSERT_NE(p1, nullptr);

  const RenderPassDesc singlePass{
      .colorAttachments = {{.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store}},
      .subpasses = {}};

  const RenderPassDesc multiPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::DontCare},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::DontCare}},
      .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
          {.colorAttachmentMask = 0b110, .inputAttachmentMask = 0},
          {.colorAttachmentMask = 0b001, .inputAttachmentMask = 0b110}}};

  // Single-pass render
  renderSubpasses(singlePass, fb, {pSingle});
  expectPixels(att0, kRed);

  // Multi-pass render
  renderSubpasses(multiPass, fb, {p0, p1});
  expectPixels(att0, kSumOfAttachments1And2);

  // Single-pass render again
  renderSubpasses(singlePass, fb, {pSingle});
  expectPixels(att0, kRed);
}

TEST_F(SubpassesTest, EndEncodingCompletesMultiSubpassRenderPass) {
  if (const char* reason = gpuTestSkipReason()) {
    GTEST_SKIP() << reason;
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment);
  const auto fb = createFramebuffer({att0, att1});
  ASSERT_NE(fb, nullptr);
  const auto pipeline = createPipeline(kWriteRed, 2, "subpass 0");
  ASSERT_NE(pipeline, nullptr);

  const RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear,
                            .storeAction = StoreAction::Store,
                            .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}},
                           {.loadAction = LoadAction::Clear,
                            .storeAction = StoreAction::Store,
                            .clearColor = {0.0f, 0.0f, 1.0f, 1.0f}}},
      .subpasses = {{.colorAttachmentMask = 0b01}, {.colorAttachmentMask = 0b10}},
  };

  Result ret;
  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(pipeline);
  encoder->draw(3);
  // No nextSubpass(): endEncoding() still ends the render pass in its last subpass. Debug builds
  // report the missing call.
  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  expectPixels(att0, kRed);
  expectPixels(att1, kBlue);
}

TEST_F(SubpassesTest, BindGroupsPersistAcrossSubpasses) {
  if (const char* reason = gpuTestSkipReason()) {
    GTEST_SKIP() << reason;
  }

  const auto att0 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::Sampled);
  const auto att1 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::Sampled);
  const auto att2 = createColorTexture(TextureDesc::TextureUsageBits::Attachment |
                                       TextureDesc::TextureUsageBits::Sampled);
  const auto fb = createFramebuffer({att0, att1, att2});
  ASSERT_NE(fb, nullptr);

  // Create texture with pixels (50, 0, 0)
  TextureDesc texDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Sampled);
  Result ret;
  auto texResource = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const std::vector<uint32_t> texPixels(kSize * kSize, 0xFF000032); // pack(50, 0, 0, 255)
  ret = texResource->upload(texResource->getFullRange(0), texPixels.data());
  ASSERT_TRUE(ret.isOk()) << ret.message;

  // Create uniform buffer with color (0, 60, 0)
  const std::array<float, 4> bufferData = {0.0f, 60.0f / 255.0f, 0.0f, 1.0f};
  auto bufferResourceUnique =
      iglDev_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                       .data = bufferData.data(),
                                       .length = sizeof(bufferData),
                                       .storage = ResourceStorage::Shared},
                            &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  std::shared_ptr<IBuffer> bufferResource(bufferResourceUnique.release());

  // Create sampler
  auto sampler =
      iglDev_->createSamplerState(SamplerStateDesc{.minFilter = SamplerMinMagFilter::Nearest,
                                                   .magFilter = SamplerMinMagFilter::Nearest},
                                  &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  // Create bind groups
  auto textureGroup = iglDev_->createBindGroup(BindGroupTextureDesc{.textures = {texResource},
                                                                    .samplers = {sampler},
                                                                    .debugName = "textureGroup"},
                                               nullptr,
                                               &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto bufferGroup = iglDev_->createBindGroup(
      BindGroupBufferDesc{
          .buffers = {bufferResource}, .offset = {0}, .size = {16}, .debugName = "bufferGroup"},
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  // One pipeline serves both subpasses because Vulkan discards the writes to the attachments that
  // are outside a subpass's color mask. Other backends need a pipeline per subpass.
  const char* fs = R"(#version 460
layout(set = 0, binding = 0) uniform sampler2D uTex;
layout(std140, set = 1, binding = 0) uniform UniformBlock {
  vec4 color;
} ub;
layout(location = 0) out vec4 out0;
layout(location = 1) out vec4 out1;
layout(location = 2) out vec4 out2;
void main() {
  vec4 c = texture(uTex, vec2(0.5)) + ub.color;
  out0 = c;
  out1 = c;
  out2 = c;
})";

  auto pipeline = createPipeline(fs, 3, "bind groups");
  ASSERT_NE(pipeline, nullptr);

  const RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store}},
      .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
          {.colorAttachmentMask = 0b110, .inputAttachmentMask = 0},
          {.colorAttachmentMask = 0b001, .inputAttachmentMask = 0}}};

  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk());
  auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(encoder, nullptr);

  encoder->bindRenderPipelineState(pipeline);
  encoder->bindBindGroup(textureGroup);
  encoder->bindBindGroup(bufferGroup);
  encoder->draw(3); // subpass 0: writes att1, att2
  encoder->nextSubpass();
  encoder->draw(3); // subpass 1: writes att0 (groups persist, no re-bind needed)
  encoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  const uint32_t expected = 0xFF003C32; // pack(50, 60, 0, 255)
  expectPixels(att0, expected);
  expectPixels(att1, expected);
  expectPixels(att2, expected);
}

} // namespace igl::tests
