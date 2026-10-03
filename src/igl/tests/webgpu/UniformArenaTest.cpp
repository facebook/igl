/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/UniformArena.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/ComputePipelineState.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/ShaderCreator.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/ResourcesBinder.h>

namespace igl::tests {

namespace {

constexpr uint32_t kSize = 4;

// A full-screen triangle colored from a 16-byte uniform struct.
constexpr const char* kVertex = R"(
@vertex
fn main(@builtin(vertex_index) i : u32) -> @builtin(position) vec4f {
  let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
  return vec4f(uv * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char* kFragment = R"(
struct Uniforms { color : vec4f, };
@group(1) @binding(3) var<uniform> uniforms : Uniforms;

@fragment
fn main() -> @location(0) vec4f {
  return uniforms.color;
}
)";

constexpr const char* kCompute = R"(
struct Params { scale : f32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(1) @binding(1) var<storage, read_write> values : array<f32>;

@compute @workgroup_size(4)
fn main(@builtin(global_invocation_id) id : vec3u) {
  values[id.x] = values[id.x] * params.scale;
}
)";

uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return r | (g << 8) | (b << 16) | (static_cast<uint32_t>(a) << 24);
}

} // namespace

class WebGPUUniformArenaTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
    Result ret;
    queue_ = device_->createCommandQueue({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    color_ = device_->createTexture(
        TextureDesc::new2D(
            TextureFormat::RGBA_UNorm8, kSize, kSize, TextureDesc::TextureUsageBits::Attachment),
        &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    framebuffer_ = device_->createFramebuffer({.colorAttachments = {{.texture = color_}}}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    auto stages = ShaderStagesCreator::fromModuleStringInput(
        *device_, kVertex, "main", "", kFragment, "main", "", &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    pipeline_ = device_->createRenderPipeline(
        {.shaderStages = std::move(stages),
         .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}}}},
        &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
  }

  void TearDown() override {
    EXPECT_EQ(device_->getContext().getUncapturedErrorCount(), 0u);
  }

 protected:
  // Draws the left and right halves with bindBytes() colors and returns the bottom row.
  std::array<uint32_t, kSize> drawHalves(const std::array<float, 4>& left,
                                         const std::array<float, 4>& right) {
    auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
    auto encoder = cmdBuffer->createRenderCommandEncoder(
        {.colorAttachments = {{.loadAction = LoadAction::Clear,
                               .storeAction = StoreAction::Store}}},
        framebuffer_);
    encoder->bindRenderPipelineState(pipeline_);
    encoder->bindScissorRect({.x = 0, .y = 0, .width = kSize / 2, .height = kSize});
    encoder->bindBytes(3, BindTarget::kFragment, left.data(), sizeof(left));
    encoder->draw(3);
    encoder->bindScissorRect({.x = kSize / 2, .y = 0, .width = kSize / 2, .height = kSize});
    encoder->bindBytes(3, BindTarget::kFragment, right.data(), sizeof(right));
    encoder->draw(3);
    encoder->endEncoding();
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
    std::array<uint32_t, kSize * kSize> pixels = {};
    framebuffer_->copyBytesColorAttachment(
        *queue_, 0, pixels.data(), TextureRangeDesc::new2D(0, 0, kSize, kSize));
    std::array<uint32_t, kSize> row = {};
    std::memcpy(row.data(), pixels.data(), sizeof(row));
    return row;
  }

  std::unique_ptr<webgpu::Device> device_;
  std::shared_ptr<ICommandQueue> queue_;
  std::shared_ptr<ITexture> color_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IRenderPipelineState> pipeline_;
};

TEST_F(WebGPUUniformArenaTest, Features) {
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::BindBytes));
  EXPECT_TRUE(device_->hasFeature(DeviceFeatures::FillBuffer));
  size_t limit = 0;
  EXPECT_TRUE(device_->getFeatureLimits(DeviceFeatureLimits::MaxBindBytesBytes, limit));
  EXPECT_EQ(limit, 4096u);
}

TEST_F(WebGPUUniformArenaTest, BindBytesSharesChunksAndBindGroups) {
  const uint32_t red = rgba(255, 0, 0, 255);
  const uint32_t blue = rgba(0, 0, 255, 255);
  EXPECT_EQ(drawHalves({1, 0, 0, 1}, {0, 0, 1, 1}),
            (std::array<uint32_t, kSize>{red, red, blue, blue}));

  auto& cache = device_->getContext().getBindGroupCache();
  auto& pool = device_->getContext().getUniformArenaPool();
  const size_t bindGroups = cache.getCreationCount();
  const size_t chunks = pool.getCreationCount();
  for (int frame = 0; frame < 3; ++frame) {
    EXPECT_EQ(drawHalves({0, 0, 1, 1}, {1, 0, 0, 1}),
              (std::array<uint32_t, kSize>{blue, blue, red, red}));
  }
  EXPECT_EQ(cache.getCreationCount(), bindGroups);
  EXPECT_EQ(pool.getCreationCount(), chunks);
  EXPECT_EQ(chunks, 1u);
}

TEST_F(WebGPUUniformArenaTest, SlicesAndChunks) {
  webgpu::UniformArena arena(device_->getContext());
  const std::array<uint8_t, 20> data = {1, 2, 3};
  const auto first = arena.allocate(data.data(), data.size());
  ASSERT_NE(first.buffer, nullptr);
  EXPECT_EQ(first.offset, 0u);
  EXPECT_EQ(first.size, 32u);
  const auto second = arena.allocate(data.data(), 1);
  EXPECT_EQ(second.buffer, first.buffer);
  EXPECT_EQ(second.offset, 256u);
  EXPECT_EQ(second.size, 16u);

  EXPECT_EQ(arena.allocate(data.data(), 0).buffer, nullptr);
  std::vector<uint8_t> tooLarge(webgpu::UniformArena::kMaxAllocationSize + 1);
  EXPECT_EQ(arena.allocate(tooLarge.data(), tooLarge.size()).buffer, nullptr);

  // 64 KiB chunks hold 256 slices of up to 256 bytes.
  webgpu::Buffer* last = second.buffer;
  for (int i = 2; i < 257; ++i) {
    last = arena.allocate(data.data(), data.size()).buffer;
  }
  EXPECT_NE(last, first.buffer);
  arena.flush();
  arena.releaseChunks();
}

TEST_F(WebGPUUniformArenaTest, ComputeBindBytes) {
  Result ret;
  auto module =
      device_->createShaderModule({.info = {.stage = ShaderStage::Compute, .entryPoint = "main"},
                                   .input = {.source = kCompute, .type = ShaderInputType::String}},
                                  &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto pipeline =
      device_->createComputePipeline({.shaderStages = device_->createShaderStages(
                                          ShaderStagesDesc::fromComputeModule(module), nullptr)},
                                     &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const std::array<float, 4> values = {1, 2, 3, 4};
  auto buffer = device_->createBuffer(
      {.type = BufferDesc::BufferTypeBits::Storage, .data = values.data(), .length = 16}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  auto encoder = cmdBuffer->createComputeCommandEncoder();
  encoder->bindComputePipelineState(pipeline);
  encoder->bindBuffer(1, buffer.get());
  for (const float scale : {2.0f, 5.0f}) {
    encoder->bindBytes(0, &scale, sizeof(scale));
    encoder->dispatchThreadGroups({1, 1, 1}, {4, 1, 1});
  }
  encoder->endEncoding();
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  std::array<float, 4> out = {};
  const void* data = buffer->map({16, 0}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(data, nullptr);
  std::memcpy(out.data(), data, sizeof(out));
  buffer->unmap();
  EXPECT_EQ(out, (std::array<float, 4>{10, 20, 30, 40}));
}

TEST_F(WebGPUUniformArenaTest, FillBuffer) {
  Result ret;
  const std::vector<uint8_t> initial(32, 0x11);
  auto buffer = device_->createBuffer(
      {.type = BufferDesc::BufferTypeBits::Storage, .data = initial.data(), .length = 32}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto cmdBuffer = queue_->createCommandBuffer({}, nullptr);
  cmdBuffer->fillBuffer(*buffer, {4, 8}, 0);
  cmdBuffer->fillBuffer(*buffer, {16, 12}, 0xab);
  // Unaligned and out-of-range fills are skipped. BufferRange is (size, offset).
  cmdBuffer->fillBuffer(*buffer, {4, 2}, 0xff);
  cmdBuffer->fillBuffer(*buffer, {6, 0}, 0xff);
  cmdBuffer->fillBuffer(*buffer, {8, 28}, 0xff);
  cmdBuffer->fillBuffer(*buffer, {std::numeric_limits<size_t>::max() - 3, 4}, 0);
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  std::vector<uint8_t> expected(initial);
  std::fill(expected.begin() + 8, expected.begin() + 12, 0);
  std::fill(expected.begin() + 12, expected.begin() + 28, 0xab);
  std::vector<uint8_t> out(32);
  const void* data = buffer->map({32, 0}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(data, nullptr);
  std::memcpy(out.data(), data, out.size());
  buffer->unmap();
  EXPECT_EQ(out, expected);
}

} // namespace igl::tests
