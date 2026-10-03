/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/Buffer.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/RenderPass.h>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/webgpu/Readback.h>

namespace igl::tests {

namespace {

using Bytes16 = std::array<uint8_t, 16>;

constexpr Bytes16 kPattern = {0x10,
                              0x11,
                              0x12,
                              0x13,
                              0x14,
                              0x15,
                              0x16,
                              0x17,
                              0x18,
                              0x19,
                              0x1a,
                              0x1b,
                              0x1c,
                              0x1d,
                              0x1e,
                              0x1f};

} // namespace

class WebGPUBufferTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
    Result ret;
    queue_ = device_->createCommandQueue({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(queue_, nullptr);
  }

  void TearDown() override {
    EXPECT_EQ(context().getUncapturedErrorCount(), 0u);
  }

 protected:
  [[nodiscard]] webgpu::WebGPUContext& context() const {
    return device_->getContext();
  }

  [[nodiscard]] std::unique_ptr<webgpu::Buffer> createBuffer(size_t length,
                                                             const void* data = nullptr) const {
    Result ret;
    auto buffer = device_->createBuffer(
        {.type = BufferDesc::BufferTypeBits::Storage, .data = data, .length = length}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return std::unique_ptr<webgpu::Buffer>(static_cast<webgpu::Buffer*>(buffer.release()));
  }

  [[nodiscard]] std::shared_ptr<ICommandBuffer> createCommandBuffer() const {
    Result ret;
    auto cmdBuffer = queue_->createCommandBuffer({}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return cmdBuffer;
  }

  // Reads the GPU copy of `buffer`, bypassing its shadow.
  [[nodiscard]] Bytes16 readGpu(const webgpu::Buffer& buffer) const {
    Bytes16 bytes{};
    const Result ret =
        webgpu::readBuffer(context(), buffer.getWGPUBuffer(), 0, bytes.size(), bytes.data());
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return bytes;
  }

  [[nodiscard]] static Bytes16 map(webgpu::Buffer& buffer) {
    Bytes16 bytes{};
    Result ret;
    const void* data = buffer.map({bytes.size(), 0}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    if (data != nullptr) {
      std::memcpy(bytes.data(), data, bytes.size());
    }
    buffer.unmap();
    return bytes;
  }

  std::unique_ptr<webgpu::Device> device_;
  std::shared_ptr<ICommandQueue> queue_;
};

TEST_F(WebGPUBufferTest, InitialDataAndMap) {
  auto buffer = createBuffer(kPattern.size(), kPattern.data());
  ASSERT_NE(buffer, nullptr);
  EXPECT_EQ(map(*buffer), kPattern);
  EXPECT_EQ(readGpu(*buffer), kPattern);
}

TEST_F(WebGPUBufferTest, UploadWritesGpu) {
  auto buffer = createBuffer(kPattern.size());
  ASSERT_NE(buffer, nullptr);
  ASSERT_TRUE(buffer->upload(kPattern.data(), {kPattern.size(), 0}).isOk());
  EXPECT_EQ(readGpu(*buffer), kPattern);
}

TEST_F(WebGPUBufferTest, UnalignedUploadPreservesNeighbors) {
  auto buffer = createBuffer(kPattern.size(), kPattern.data());
  ASSERT_NE(buffer, nullptr);
  const std::array<uint8_t, 3> patch = {0xa0, 0xa1, 0xa2};
  ASSERT_TRUE(buffer->upload(patch.data(), {patch.size(), 5}).isOk());

  Bytes16 expected = kPattern;
  std::memcpy(expected.data() + 5, patch.data(), patch.size());
  EXPECT_EQ(readGpu(*buffer), expected);
  EXPECT_EQ(map(*buffer), expected);
}

TEST_F(WebGPUBufferTest, LengthIsPaddedToFourBytes) {
  auto buffer = createBuffer(6);
  ASSERT_NE(buffer, nullptr);
  EXPECT_EQ(buffer->getSizeInBytes(), 6u);
  EXPECT_EQ(buffer->getAllocatedSize(), 8u);

  const std::array<uint8_t, 4> data = {1, 2, 3, 4};
  EXPECT_EQ(buffer->upload(data.data(), {data.size(), 4}).code, Result::Code::ArgumentOutOfRange);
  EXPECT_TRUE(buffer->upload(data.data(), {2, 4}).isOk());
  Result ret;
  EXPECT_EQ(buffer->map({4, 4}, &ret), nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

TEST_F(WebGPUBufferTest, UploadRejectsNullData) {
  auto buffer = createBuffer(16);
  ASSERT_NE(buffer, nullptr);
  EXPECT_EQ(buffer->upload(nullptr, {4, 0}).code, Result::Code::ArgumentNull);
}

TEST_F(WebGPUBufferTest, MapWriteIsUploadedOnUnmap) {
  auto buffer = createBuffer(kPattern.size());
  ASSERT_NE(buffer, nullptr);
  Result ret;
  void* data = buffer->map({kPattern.size(), 0}, &ret);
  ASSERT_NE(data, nullptr);
  std::memcpy(data, kPattern.data(), kPattern.size());
  buffer->unmap();
  EXPECT_EQ(readGpu(*buffer), kPattern);
}

TEST_F(WebGPUBufferTest, MapReadsBackGpuWrites) {
  auto src = createBuffer(kPattern.size(), kPattern.data());
  auto dst = createBuffer(kPattern.size());
  ASSERT_TRUE(src && dst);

  auto cmdBuffer = createCommandBuffer();
  cmdBuffer->copyBuffer(*src, *dst, 0, 0, kPattern.size());
  EXPECT_TRUE(dst->isShadowStale());
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  EXPECT_EQ(map(*dst), kPattern);
  EXPECT_FALSE(dst->isShadowStale());
}

TEST_F(WebGPUBufferTest, UnalignedUploadAfterGpuWriteKeepsGpuBytes) {
  auto src = createBuffer(kPattern.size(), kPattern.data());
  auto dst = createBuffer(kPattern.size());
  ASSERT_TRUE(src && dst);

  auto cmdBuffer = createCommandBuffer();
  cmdBuffer->copyBuffer(*src, *dst, 0, 0, kPattern.size());
  queue_->submit(*cmdBuffer);

  const uint8_t patch = 0xee;
  ASSERT_TRUE(dst->upload(&patch, {1, 1}).isOk());

  Bytes16 expected = kPattern;
  expected[1] = patch;
  EXPECT_EQ(readGpu(*dst), expected);
}

TEST_F(WebGPUBufferTest, UploadsAndCopiesFollowQueueOrder) {
  const Bytes16 first{1};
  const Bytes16 second{2};
  auto src = createBuffer(first.size(), first.data());
  auto dst1 = createBuffer(first.size());
  auto dst2 = createBuffer(first.size());
  ASSERT_TRUE(src && dst1 && dst2);

  auto cmdBuffer1 = createCommandBuffer();
  cmdBuffer1->copyBuffer(*src, *dst1, 0, 0, first.size());
  queue_->submit(*cmdBuffer1);

  ASSERT_TRUE(src->upload(second.data(), {second.size(), 0}).isOk());
  auto cmdBuffer2 = createCommandBuffer();
  cmdBuffer2->copyBuffer(*src, *dst2, 0, 0, second.size());
  queue_->submit(*cmdBuffer2);
  cmdBuffer2->waitUntilCompleted();

  EXPECT_EQ(map(*dst1), first);
  EXPECT_EQ(map(*dst2), second);
}

TEST_F(WebGPUBufferTest, CopyRequiresFourByteAlignment) {
  auto src = createBuffer(kPattern.size(), kPattern.data());
  auto dst = createBuffer(kPattern.size());
  ASSERT_TRUE(src && dst);

  auto cmdBuffer = createCommandBuffer();
  cmdBuffer->copyBuffer(*src, *dst, 0, 0, 2);
  cmdBuffer->copyBuffer(*src, *dst, 2, 0, 4);
  EXPECT_FALSE(dst->isShadowStale());
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();
  EXPECT_EQ(readGpu(*dst), Bytes16{});
}

TEST_F(WebGPUBufferTest, UniformBuffersArePaddedToSixteenBytes) {
  Result ret;
  auto uniform =
      device_->createBuffer({.type = BufferDesc::BufferTypeBits::Uniform, .length = 68}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(static_cast<webgpu::Buffer&>(*uniform).getAllocatedSize(), 80u);
  EXPECT_EQ(uniform->getSizeInBytes(), 68u);
  EXPECT_EQ(createBuffer(68)->getAllocatedSize(), 68u);
}

TEST_F(WebGPUBufferTest, UnalignedCopyToTheEndCoversPadding) {
  auto src = createBuffer(11, kPattern.data());
  auto dst = createBuffer(11);
  ASSERT_TRUE(src && dst);

  auto cmdBuffer = createCommandBuffer();
  cmdBuffer->copyBuffer(*src, *dst, 0, 0, 11);
  EXPECT_TRUE(dst->isShadowStale());
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();
  Result ret;
  const auto* data = static_cast<const uint8_t*>(dst->map({11, 0}, &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(std::memcmp(data, kPattern.data(), 11), 0);
  dst->unmap();
}

TEST_F(WebGPUBufferTest, DropBeforeSubmitDefersDestroy) {
  auto src = createBuffer(kPattern.size(), kPattern.data());
  auto dst = createBuffer(kPattern.size());
  ASSERT_TRUE(src && dst);
  webgpu::ResourceTracker& tracker = context().getResourceTracker();

  auto cmdBuffer = createCommandBuffer();
  cmdBuffer->copyBuffer(*src, *dst, 0, 0, kPattern.size());
  src.reset();
  dst.reset();
  EXPECT_EQ(tracker.getPendingRetirementCount(), 2u);

  queue_->submit(*cmdBuffer);
  EXPECT_EQ(tracker.getPendingRetirementCount(), 0u);
  cmdBuffer->waitUntilCompleted();
}

TEST_F(WebGPUBufferTest, DropAfterSubmitDestroysImmediately) {
  auto src = createBuffer(kPattern.size(), kPattern.data());
  auto dst = createBuffer(kPattern.size());
  ASSERT_TRUE(src && dst);

  auto cmdBuffer = createCommandBuffer();
  cmdBuffer->copyBuffer(*src, *dst, 0, 0, kPattern.size());
  queue_->submit(*cmdBuffer);
  src.reset();
  dst.reset();
  EXPECT_EQ(context().getResourceTracker().getPendingRetirementCount(), 0u);
  cmdBuffer->waitUntilCompleted();
}

TEST_F(WebGPUBufferTest, EarlierOpenCommandBufferKeepsBufferAlive) {
  auto src = createBuffer(kPattern.size(), kPattern.data());
  auto dst = createBuffer(kPattern.size());
  ASSERT_TRUE(src && dst);
  webgpu::ResourceTracker& tracker = context().getResourceTracker();

  auto cmdBuffer1 = createCommandBuffer();
  auto cmdBuffer2 = createCommandBuffer();
  cmdBuffer1->copyBuffer(*src, *dst, 0, 0, kPattern.size());
  cmdBuffer2->copyBuffer(*src, *dst, 0, 0, kPattern.size());
  dst.reset();

  queue_->submit(*cmdBuffer2);
  EXPECT_EQ(tracker.getPendingRetirementCount(), 1u);
  queue_->submit(*cmdBuffer1);
  EXPECT_EQ(tracker.getPendingRetirementCount(), 0u);
  cmdBuffer1->waitUntilCompleted();
}

TEST_F(WebGPUBufferTest, AbandonedCommandBufferReleasesRetiredBuffers) {
  auto src = createBuffer(kPattern.size(), kPattern.data());
  ASSERT_NE(src, nullptr);
  webgpu::ResourceTracker& tracker = context().getResourceTracker();

  auto cmdBuffer = createCommandBuffer();
  auto dst = createBuffer(kPattern.size());
  cmdBuffer->copyBuffer(*src, *dst, 0, 0, kPattern.size());
  dst.reset();
  EXPECT_EQ(tracker.getPendingRetirementCount(), 1u);
  cmdBuffer.reset();
  EXPECT_EQ(tracker.getPendingRetirementCount(), 0u);
  EXPECT_EQ(tracker.getOpenCommandBufferCount(), 0u);
}

TEST_F(WebGPUBufferTest, ApiHints) {
  Result ret;
  auto buffer = device_->createBuffer(
      {.type = BufferDesc::BufferTypeBits::Uniform,
       .length = 16,
       .hint = BufferDesc::BufferAPIHintBits::Ring | BufferDesc::BufferAPIHintBits::NoCopy},
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(buffer->requestedApiHints(),
            BufferDesc::BufferAPIHintBits::Ring | BufferDesc::BufferAPIHintBits::NoCopy);
  EXPECT_EQ(buffer->acceptedApiHints(), BufferDesc::BufferAPIHintBits::Ring);
  EXPECT_EQ(buffer->getBufferType(), BufferDesc::BufferTypeBits::Uniform);
  EXPECT_EQ(buffer->gpuAddress(), 0u);
}

TEST_F(WebGPUBufferTest, TooLargeBufferFails) {
  Result ret;
  auto buffer = device_->createBuffer(
      {.type = BufferDesc::BufferTypeBits::Storage, .length = size_t{1} << 40}, &ret);
  EXPECT_EQ(buffer, nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
}

TEST_F(WebGPUBufferTest, SubmitReturnsSerialsOnce) {
  auto cmdBuffer1 = createCommandBuffer();
  auto cmdBuffer2 = createCommandBuffer();
  const SubmitHandle handle1 = queue_->submit(*cmdBuffer1);
  const SubmitHandle handle2 = queue_->submit(*cmdBuffer2);
  EXPECT_NE(handle1, 0u);
  EXPECT_GT(handle2, handle1);
  EXPECT_EQ(queue_->submit(*cmdBuffer1), 0u);
  cmdBuffer2->waitUntilCompleted();
}

TEST_F(WebGPUBufferTest, DebugGroupsAndEmptySubmit) {
  auto cmdBuffer = createCommandBuffer();
  cmdBuffer->waitUntilCompleted();
  cmdBuffer->pushDebugGroupLabel("outer");
  cmdBuffer->pushDebugGroupLabel("inner");
  cmdBuffer->popDebugGroupLabel();
  cmdBuffer->popDebugGroupLabel();
  queue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();
}

TEST_F(WebGPUBufferTest, RenderPassesNeedAFramebuffer) {
  auto cmdBuffer = createCommandBuffer();
  Result ret;
  EXPECT_EQ(cmdBuffer->createRenderCommandEncoder({}, nullptr, &ret), nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentNull);
}

} // namespace igl::tests
