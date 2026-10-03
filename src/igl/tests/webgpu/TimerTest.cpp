/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/Timer.h>

#include <chrono>
#include <memory>
#include <thread>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/ComputePass.h>
#include <igl/ComputePipelineState.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/ShaderCreator.h>
#include <igl/TimestampQueries.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>

namespace igl::tests {

namespace {

constexpr uint32_t kQuantum = 65536;

constexpr const char* kTriangleVertex = R"(
@vertex
fn main(@builtin(vertex_index) i : u32) -> @builtin(position) vec4f {
  let uv = vec2f(f32((i << 1u) & 2u), f32(i & 2u));
  return vec4f(uv * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char* kWhiteFragment = R"(
@fragment
fn main() -> @location(0) vec4f {
  return vec4f(1.0);
}
)";

// Polls until `ready()` or about a second has passed.
template<typename F>
bool waitFor(F ready) {
  for (int i = 0; i < 1000; ++i) {
    if (ready()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

} // namespace

class WebGPUTimerTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    createDevice({});
  }

  void TearDown() override {
    if (device_) {
      EXPECT_EQ(device_->getContext().getUncapturedErrorCount(), 0u);
    }
  }

 protected:
  void createDevice(const webgpu::WebGPUContextDesc& desc) {
    // Resources must not outlive their device.
    computePipeline_ = nullptr;
    renderPipeline_ = nullptr;
    framebuffer_ = nullptr;
    queue_ = nullptr;
    device_ = util::device::webgpu::createTestDevice(desc);
    ASSERT_NE(device_, nullptr);
    Result ret;
    queue_ = device_->createCommandQueue({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    auto texture = device_->createTexture(
        TextureDesc::new2D(
            TextureFormat::RGBA_UNorm8, 64, 64, TextureDesc::TextureUsageBits::Attachment),
        &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    framebuffer_ = device_->createFramebuffer({.colorAttachments = {{.texture = texture}}}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    auto stages = ShaderStagesCreator::fromModuleStringInput(
        *device_, kTriangleVertex, "main", "", kWhiteFragment, "main", "", &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    renderPipeline_ = device_->createRenderPipeline(
        {.shaderStages = std::move(stages),
         .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}}}},
        &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
  }

  // Metal writes render pass timestamps at vertex and fragment stage boundaries, so the pass
  // draws.
  void renderPass(ICommandBuffer& cmdBuffer, const RenderPassDesc::TimestampQueryDesc& query = {}) {
    const RenderPassDesc renderPass = {
        .colorAttachments = {{.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store}},
        .timestampQuery = query,
    };
    auto encoder = cmdBuffer.createRenderCommandEncoder(renderPass, framebuffer_);
    ASSERT_NE(encoder, nullptr);
    encoder->bindRenderPipelineState(renderPipeline_);
    encoder->draw(3);
    encoder->endEncoding();
  }

  void computePass(ICommandBuffer& cmdBuffer, const ComputePassDesc& computePassDesc = {}) {
    if (!computePipeline_) {
      Result ret;
      auto module = device_->createShaderModule(
          {.info = {.stage = ShaderStage::Compute, .entryPoint = "main"},
           .input = {.source = "@compute @workgroup_size(1) fn main() {}",
                     .type = ShaderInputType::String}},
          &ret);
      ASSERT_TRUE(ret.isOk()) << ret.message;
      computePipeline_ = device_->createComputePipeline(
          {.shaderStages =
               device_->createShaderStages(ShaderStagesDesc::fromComputeModule(module), nullptr)},
          &ret);
      ASSERT_TRUE(ret.isOk()) << ret.message;
    }
    auto encoder = cmdBuffer.createComputeCommandEncoder(computePassDesc);
    ASSERT_NE(encoder, nullptr);
    encoder->bindComputePipelineState(computePipeline_);
    encoder->dispatchThreadGroups({1, 1, 1}, {1, 1, 1});
    encoder->endEncoding();
  }

  std::unique_ptr<webgpu::Device> device_;
  std::shared_ptr<ICommandQueue> queue_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IComputePipelineState> computePipeline_;
  std::shared_ptr<IRenderPipelineState> renderPipeline_;
};

TEST_F(WebGPUTimerTest, TimerMeasuresPasses) {
  if (!device_->hasFeature(DeviceFeatures::Timers)) {
    GTEST_SKIP() << "No timestamp-query feature";
  }
  Result ret;
  auto timer = device_->createTimer(&ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(timer, nullptr);
  EXPECT_FALSE(timer->resultsAvailable());

  auto cmdBuffer = queue_->createCommandBuffer({.timer = timer}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  renderPass(*cmdBuffer);
  computePass(*cmdBuffer);
  // Metal skips empty compute passes, which then write no timestamps.
  auto empty = cmdBuffer->createComputeCommandEncoder();
  ASSERT_NE(empty, nullptr);
  empty->endEncoding();
  renderPass(*cmdBuffer);
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();
  ASSERT_TRUE(waitFor([&] { return timer->resultsAvailable(); }));
  EXPECT_GT(timer->getElapsedTimeNanos(), 0u);
  EXPECT_LT(timer->getElapsedTimeNanos(), 1'000'000'000u);

  // Attached to the next command buffer, the timer keeps the completed result until a newer one
  // completes; a command buffer without passes produces none.
  const uint64_t elapsed = timer->getElapsedTimeNanos();
  auto copyOnly = queue_->createCommandBuffer({.timer = timer}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_TRUE(timer->resultsAvailable());
  queue_->submit(*copyOnly);
  copyOnly->waitUntilCompleted();
  EXPECT_TRUE(timer->resultsAvailable());
  EXPECT_EQ(timer->getElapsedTimeNanos(), elapsed);
}

TEST_F(WebGPUTimerTest, TimerKeepsReadbacksSubmittedBeforePolling) {
  if (!device_->hasFeature(DeviceFeatures::Timers)) {
    GTEST_SKIP() << "No timestamp-query feature";
  }
  Result ret;
  auto timer = device_->createTimer(&ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  // The first readback is still mapping when the second command buffer is submitted.
  auto first = queue_->createCommandBuffer({.timer = timer}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  renderPass(*first);
  queue_->submit(*first);
  first->waitUntilCompleted();
  auto second = queue_->createCommandBuffer({.timer = timer}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  renderPass(*second);
  queue_->submit(*second);
  // The first result is readable while the second readback is in flight.
  EXPECT_TRUE(timer->resultsAvailable());
  EXPECT_GT(timer->getElapsedTimeNanos(), 0u);
}

TEST_F(WebGPUTimerTest, FailedReadbackStillCompletes) {
  if (!device_->hasFeature(DeviceFeatures::Timers)) {
    GTEST_SKIP() << "No timestamp-query feature";
  }
  Result ret;
  auto timer = device_->createTimer(&ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto cmdBuffer = queue_->createCommandBuffer({.timer = timer}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  renderPass(*cmdBuffer);
  queue_->submit(*cmdBuffer);
  // Destroying the device aborts the readback mapping unless it already finished.
  wgpuDeviceDestroy(device_->getContext().getDevice());
  ASSERT_TRUE(waitFor([&] { return timer->resultsAvailable(); }));
}

TEST_F(WebGPUTimerTest, TimestampQueriesPerPass) {
  if (!device_->hasFeature(DeviceFeatures::TimestampQueries)) {
    GTEST_SKIP() << "No timestamp-query feature";
  }
  Result ret;
  auto queries = device_->createTimestampQueries(4, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(queries, nullptr);
  EXPECT_EQ(queries->capacity(), 4u);
  EXPECT_TRUE(queries->supportsComputePassTimestamps());

  for (int frame = 0; frame < 2; ++frame) {
    queries->reset();
    EXPECT_EQ(queries->count(), 0u);
    EXPECT_FALSE(queries->resultsAvailable());
    auto cmdBuffer = queue_->createCommandBuffer({.timestampQueries = queries}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    renderPass(*cmdBuffer, {.queries = queries, .slotIndex = 0});
    computePass(*cmdBuffer, {.timestampQuery = {.queries = queries, .slotIndex = 1}});
    renderPass(*cmdBuffer, {.queries = queries, .slotIndex = 7});
    EXPECT_EQ(queries->count(), 2u);
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
    ASSERT_TRUE(waitFor([&] { return queries->resultsAvailable(); }));
    for (uint32_t slot = 0; slot < 2; ++slot) {
      const TimestampQueryResult result = queries->getElapsedNanosResult(slot);
      EXPECT_TRUE(result.valid);
      EXPECT_GE(queries->getEndNanos(slot), queries->getStartNanos(slot));
      EXPECT_GT(queries->getEndNanos(slot), 0u);
    }
    EXPECT_FALSE(queries->getElapsedNanosResult(2).valid);
    EXPECT_GE(queries->getFrameElapsedNanos(), queries->getElapsedNanos(0));
  }

  // A slot written in an earlier frame but not in this one is not reported again.
  queries->reset();
  auto cmdBuffer = queue_->createCommandBuffer({.timestampQueries = queries}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  renderPass(*cmdBuffer, {.queries = queries, .slotIndex = 0});
  renderPass(*cmdBuffer, {.queries = queries, .slotIndex = 2});
  EXPECT_EQ(queries->count(), 3u);
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();
  ASSERT_TRUE(waitFor([&] { return queries->resultsAvailable(); }));
  EXPECT_TRUE(queries->getElapsedNanosResult(0).valid);
  EXPECT_FALSE(queries->getElapsedNanosResult(1).valid);
  EXPECT_TRUE(queries->getElapsedNanosResult(2).valid);
}

TEST_F(WebGPUTimerTest, HighResolutionTimestampsCanBeDisabled) {
  if (!device_->hasFeature(DeviceFeatures::TimestampQueries)) {
    GTEST_SKIP() << "No timestamp-query feature";
  }
  for (const bool highResolution : {true, false}) {
    createDevice({.highResolutionTimestamps = highResolution});
    constexpr uint32_t kSlots = 16;
    Result ret;
    auto queries = device_->createTimestampQueries(kSlots, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    auto cmdBuffer = queue_->createCommandBuffer({.timestampQueries = queries}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    for (uint32_t slot = 0; slot < kSlots; ++slot) {
      renderPass(*cmdBuffer, {.queries = queries, .slotIndex = slot});
    }
    queue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
    ASSERT_TRUE(waitFor([&] { return queries->resultsAvailable(); }));
    bool finerThanQuantum = false;
    for (uint32_t slot = 0; slot < kSlots; ++slot) {
      finerThanQuantum |= queries->getStartNanos(slot) % kQuantum != 0 ||
                          queries->getEndNanos(slot) % kQuantum != 0;
    }
    EXPECT_EQ(finerThanQuantum, highResolution);
  }
}

} // namespace igl::tests
