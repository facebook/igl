/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/ResourcesBinder.h>

#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <igl/CommandQueue.h>
#include <igl/ShaderCreator.h>
#include <igl/tests/data/ShaderData.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/CommandBuffer.h>
#include <igl/webgpu/RenderPipelineState.h>
#include <igl/webgpu/SamplerState.h>
#include <igl/webgpu/Texture.h>

namespace igl::tests {

namespace {

// A texture unit and a uniform buffer.
constexpr const char* kFragment = R"(
struct Uniforms { tint : vec4f, };
@group(0) @binding(0) var tex : texture_2d<f32>;
@group(0) @binding(1) var samp : sampler;
@group(1) @binding(0) var<uniform> uniforms : Uniforms;

@fragment
fn main(@location(0) uv : vec2f) -> @location(0) vec4f {
  return textureSample(tex, samp, uv) * uniforms.tint;
}
)";

} // namespace

class WebGPUResourcesBinderTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
    Result ret;
    queue_ = device_->createCommandQueue({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    target_ = createTexture(TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Attachment);
    texture_ = createTexture(TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled);
    sampler_ = std::static_pointer_cast<webgpu::SamplerState>(
        device_->createSamplerState(SamplerStateDesc::newLinear(), &ret));
    uniforms_ = createBuffer(BufferDesc::BufferTypeBits::Uniform, 512);
    vertices_ = createBuffer(BufferDesc::BufferTypeBits::Vertex, 64);
    ASSERT_TRUE(target_ && texture_ && sampler_ && uniforms_ && vertices_);

    auto vertexInput = device_->createVertexInputState(
        {
            .numAttributes = 2,
            .attributes =
                {{.bufferIndex = 0, .format = VertexAttributeFormat::Float4, .location = 0},
                 {.bufferIndex = 1, .format = VertexAttributeFormat::Float2, .location = 1}},
            .numInputBindings = 2,
            .inputBindings = {{.stride = 16}, {.stride = 8}},
        },
        &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    std::shared_ptr<IShaderStages> stages = ShaderStagesCreator::fromModuleStringInput(
        *device_,
        std::string(data::shader::kWgslSimpleVertShader).c_str(),
        "main",
        "",
        kFragment,
        "main",
        "",
        &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    pipeline_ = std::static_pointer_cast<webgpu::RenderPipelineState>(device_->createRenderPipeline(
        {
            .vertexInputState = vertexInput,
            .shaderStages = stages,
            .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}}},
        },
        &ret));
    ASSERT_TRUE(ret.isOk()) << ret.message;
  }

  void TearDown() override {
    EXPECT_EQ(context().getUncapturedErrorCount(), 0u);
  }

 protected:
  [[nodiscard]] webgpu::WebGPUContext& context() const {
    return device_->getContext();
  }
  [[nodiscard]] webgpu::BindGroupCache& cache() const {
    return context().getBindGroupCache();
  }

  [[nodiscard]] std::shared_ptr<webgpu::Texture> createTexture(TextureFormat format,
                                                               TextureDesc::TextureUsage usage) {
    Result ret;
    auto texture = device_->createTexture(TextureDesc::new2D(format, 4, 4, usage), &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return std::static_pointer_cast<webgpu::Texture>(texture);
  }

  [[nodiscard]] std::shared_ptr<webgpu::Buffer> createBuffer(BufferDesc::BufferType type,
                                                             size_t length) {
    Result ret;
    auto buffer = device_->createBuffer({.type = type, .length = length}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return std::shared_ptr<webgpu::Buffer>(static_cast<webgpu::Buffer*>(buffer.release()));
  }

  // Records one draw in a fresh render pass after `bind` set up the binder, and submits it.
  // Returns the result of the binder.
  Result draw(const std::function<void(webgpu::ResourcesBinder&)>& bind) {
    Result ret;
    auto cmdBuffer = queue_->createCommandBuffer({}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    auto& webgpuCmdBuffer = static_cast<webgpu::CommandBuffer&>(*cmdBuffer);

    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = target_->getAttachmentView(0, 0);
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &color;
    webgpu::Handle<WGPURenderPassEncoder> pass(
        wgpuCommandEncoderBeginRenderPass(webgpuCmdBuffer.getWGPUCommandEncoder(), &passDesc));

    webgpu::ResourcesBinder binder(context(), device_->getDeviceFeatureSet());
    bind(binder);
    webgpu::SampleClasses classes = 0;
    Result result = binder.getSampleClasses(*pipeline_, classes);
    if (result.isOk()) {
      webgpu::RenderPipelineDynamicState state = pipeline_->getDefaultDynamicState();
      state.sampleClasses = classes;
      WGPURenderPipeline pipeline = pipeline_->getPipeline(state, &result);
      if (pipeline != nullptr) {
        wgpuRenderPassEncoderSetPipeline(pass.get(), pipeline);
        result = binder.flush(pass.get(), *pipeline_, classes, webgpuCmdBuffer.getSerial());
      }
    }
    if (result.isOk()) {
      wgpuRenderPassEncoderSetVertexBuffer(pass.get(), 0, vertices_->getWGPUBuffer(), 0, 64);
      wgpuRenderPassEncoderSetVertexBuffer(pass.get(), 1, vertices_->getWGPUBuffer(), 0, 64);
      wgpuRenderPassEncoderDraw(pass.get(), 3, 1, 0, 0);
    }
    wgpuRenderPassEncoderEnd(pass.get());
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
    return result;
  }

  void bindAll(webgpu::ResourcesBinder& binder,
               webgpu::Texture* texture,
               size_t uniformOffset = 0) const {
    binder.bindTexture(0, texture);
    binder.bindSampler(0, sampler_.get());
    binder.bindBuffer(0, uniforms_.get(), uniformOffset, 16);
  }

  std::unique_ptr<webgpu::Device> device_;
  std::shared_ptr<ICommandQueue> queue_;
  std::shared_ptr<webgpu::Texture> target_;
  std::shared_ptr<webgpu::Texture> texture_;
  std::shared_ptr<webgpu::SamplerState> sampler_;
  std::shared_ptr<webgpu::Buffer> uniforms_;
  std::shared_ptr<webgpu::Buffer> vertices_;
  std::shared_ptr<webgpu::RenderPipelineState> pipeline_;
};

TEST_F(WebGPUResourcesBinderTest, SteadyStateCreatesNoBindGroups) {
  ASSERT_TRUE(draw([this](auto& binder) { bindAll(binder, texture_.get()); }).isOk());
  const size_t created = cache().getCreationCount();
  EXPECT_EQ(created, 2u);
  const size_t pipelines = pipeline_->getPipelineCreationCount();
  for (int frame = 0; frame < 10; ++frame) {
    ASSERT_TRUE(draw([this](auto& binder) { bindAll(binder, texture_.get()); }).isOk());
  }
  EXPECT_EQ(cache().getCreationCount(), created);
  EXPECT_EQ(cache().getHitCount(), 20u);
  EXPECT_EQ(pipeline_->getPipelineCreationCount(), pipelines);
}

TEST_F(WebGPUResourcesBinderTest, DynamicOffsetsShareOneBindGroup) {
  ASSERT_TRUE(draw([this](auto& binder) { bindAll(binder, texture_.get(), 0); }).isOk());
  ASSERT_TRUE(draw([this](auto& binder) { bindAll(binder, texture_.get(), 256); }).isOk());
  EXPECT_EQ(cache().getCreationCount(), 2u);
  EXPECT_EQ(draw([this](auto& binder) { bindAll(binder, texture_.get(), 16); }).code,
            Result::Code::ArgumentInvalid);
}

TEST_F(WebGPUResourcesBinderTest, DestroyedResourcesAreEvicted) {
  auto texture = createTexture(TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled);
  ASSERT_TRUE(draw([&](auto& binder) { bindAll(binder, texture.get()); }).isOk());
  const size_t size = cache().size();
  texture.reset();
  EXPECT_EQ(cache().size(), size - 1);
  uniforms_.reset();
  EXPECT_EQ(cache().size(), size - 2);
}

TEST_F(WebGPUResourcesBinderTest, LeastRecentlyUsedEviction) {
  webgpu::BindGroupCache cache(context(), 2);
  Result ret;
  WGPUBindGroupLayout layout = pipeline_->getBindGroupLayout(webgpu::kTextureGroup, 0, &ret);
  ASSERT_NE(layout, nullptr);
  const auto entries = [this](const webgpu::Texture& texture) {
    std::vector<webgpu::BindGroupCache::Entry> result(2);
    result[0].entry.binding = 0;
    result[0].entry.textureView = texture.getSampledView();
    result[0].resourceId = texture.getTextureId();
    result[1].entry.binding = 1;
    result[1].entry.sampler = sampler_->getWGPUSampler();
    result[1].resourceId = sampler_->getSamplerId();
    return result;
  };
  auto a = createTexture(TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled);
  auto b = createTexture(TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled);
  auto c = createTexture(TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled);
  EXPECT_NE(cache.get(layout, entries(*a), &ret), nullptr);
  EXPECT_NE(cache.get(layout, entries(*b), &ret), nullptr);
  EXPECT_NE(cache.get(layout, entries(*a), &ret), nullptr);
  // b is the least recently used and makes room for c.
  EXPECT_NE(cache.get(layout, entries(*c), &ret), nullptr);
  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.getCreationCount(), 3u);
  EXPECT_NE(cache.get(layout, entries(*a), &ret), nullptr);
  EXPECT_EQ(cache.getCreationCount(), 3u);
  EXPECT_NE(cache.get(layout, entries(*b), &ret), nullptr);
  EXPECT_EQ(cache.getCreationCount(), 4u);
}

TEST_F(WebGPUResourcesBinderTest, UnboundSlotsGetDummies) {
  EXPECT_TRUE(draw([](auto& /*binder*/) {}).isOk());
}

TEST_F(WebGPUResourcesBinderTest, UnfilterableTextureUsesNearestTwin) {
  auto depth = createTexture(TextureFormat::Z_UNorm32,
                             TextureDesc::TextureUsageBits::Sampled |
                                 TextureDesc::TextureUsageBits::Attachment);
  ASSERT_NE(depth, nullptr);
  EXPECT_TRUE(draw([&](auto& binder) { bindAll(binder, depth.get()); }).isOk());
  const size_t pipelines = pipeline_->getPipelineCreationCount();
  // Back to a filterable texture: the default variant is reused.
  EXPECT_TRUE(draw([this](auto& binder) { bindAll(binder, texture_.get()); }).isOk());
  EXPECT_EQ(pipeline_->getPipelineCreationCount(), pipelines);
}

TEST_F(WebGPUResourcesBinderTest, RejectsWhatWebGPUCannotBind) {
  auto integer = createTexture(TextureFormat::R_UInt32, TextureDesc::TextureUsageBits::Sampled);
  EXPECT_EQ(draw([&](auto& binder) { bindAll(binder, integer.get()); }).code,
            Result::Code::ArgumentInvalid);

  Result ret;
  auto comparison = std::static_pointer_cast<webgpu::SamplerState>(device_->createSamplerState(
      {.depthCompareFunction = CompareFunction::Less, .depthCompareEnabled = true}, &ret));
  EXPECT_EQ(draw([&](auto& binder) {
              bindAll(binder, texture_.get());
              binder.bindSampler(0, comparison.get());
            }).code,
            Result::Code::ArgumentInvalid);

  auto storage = createBuffer(BufferDesc::BufferTypeBits::Storage, 256);
  EXPECT_EQ(draw([&](auto& binder) {
              bindAll(binder, texture_.get());
              binder.bindBuffer(0, storage.get(), 0, 16);
            }).code,
            Result::Code::ArgumentInvalid);
}

TEST_F(WebGPUResourcesBinderTest, RejectsBuffersSmallerThanTheStruct) {
  constexpr const char* kLargeUniformsFragment = R"(
struct Uniforms { tint : vec4f, extra : array<vec4f, 3>, };
@group(0) @binding(0) var tex : texture_2d<f32>;
@group(0) @binding(1) var samp : sampler;
@group(1) @binding(0) var<uniform> uniforms : Uniforms;
@fragment
fn main(@location(0) uv : vec2f) -> @location(0) vec4f {
  return textureSample(tex, samp, uv) * uniforms.tint + uniforms.extra[0];
}
)";
  Result ret;
  auto vertexInput = device_->createVertexInputState(
      {
          .numAttributes = 2,
          .attributes = {{.bufferIndex = 0, .format = VertexAttributeFormat::Float4, .location = 0},
                         {.bufferIndex = 1,
                          .format = VertexAttributeFormat::Float2,
                          .location = 1}},
          .numInputBindings = 2,
          .inputBindings = {{.stride = 16}, {.stride = 8}},
      },
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  std::shared_ptr<IShaderStages> stages = ShaderStagesCreator::fromModuleStringInput(
      *device_,
      std::string(data::shader::kWgslSimpleVertShader).c_str(),
      "main",
      "",
      kLargeUniformsFragment,
      "main",
      "",
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  pipeline_ = std::static_pointer_cast<webgpu::RenderPipelineState>(device_->createRenderPipeline(
      {
          .vertexInputState = vertexInput,
          .shaderStages = stages,
          .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}}},
      },
      &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message;

  // 64-byte struct: a 32-byte buffer cannot be widened enough.
  auto small = createBuffer(BufferDesc::BufferTypeBits::Uniform, 32);
  EXPECT_EQ(draw([&](auto& binder) {
              bindAll(binder, texture_.get());
              binder.bindBuffer(0, small.get(), 0, 32);
            }).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_TRUE(draw([&](auto& binder) {
                bindAll(binder, texture_.get());
                binder.bindBuffer(0, uniforms_.get(), 0, 16);
              }).isOk());
}

TEST_F(WebGPUResourcesBinderTest, PushConstantRangeChecksDoNotWrap) {
  webgpu::ResourcesBinder binder(context(), device_->getDeviceFeatureSet());
  const std::array<uint8_t, 8> data = {};
  EXPECT_TRUE(binder.updatePushConstants(data.data(), data.size(), 120).isOk());
  EXPECT_EQ(binder.updatePushConstants(data.data(), data.size(), 121).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(binder.updatePushConstants(data.data(), 4, std::numeric_limits<size_t>::max() - 1).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(binder.updatePushConstants(data.data(), std::numeric_limits<size_t>::max(), 8).code,
            Result::Code::ArgumentOutOfRange);
}

TEST_F(WebGPUResourcesBinderTest, BindGroupApiRecords) {
  Result ret;
  auto textureGroup = device_->createBindGroup(
      BindGroupTextureDesc{.textures = {texture_}, .samplers = {sampler_}, .debugName = "group"},
      nullptr,
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const BindGroupTextureDesc* textureDesc = device_->getBindGroupTextureDesc(textureGroup);
  ASSERT_NE(textureDesc, nullptr);
  EXPECT_EQ(textureDesc->textures[0], texture_);

  auto bufferGroup = device_->createBindGroup(
      BindGroupBufferDesc{.buffers = {uniforms_}, .size = {16}, .debugName = "buffers"}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const BindGroupBufferDesc* bufferDesc = device_->getBindGroupBufferDesc(bufferGroup);
  ASSERT_NE(bufferDesc, nullptr);
  EXPECT_EQ(bufferDesc->size[0], 16u);
}

} // namespace igl::tests
