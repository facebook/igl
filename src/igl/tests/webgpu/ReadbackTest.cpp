/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/Readback.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <igl/tests/util/device/webgpu/TestDevice.h>

namespace igl::tests {

namespace {

constexpr uint32_t kWidth = 3;
constexpr uint32_t kHeight = 2;
constexpr uint32_t kBytesPerTexel = 4;
constexpr size_t kTightBytesPerRow = size_t{kWidth} * kBytesPerTexel;

// Top row first, as written with wgpuQueueWriteTexture().
std::vector<uint8_t> makePixels() {
  std::vector<uint8_t> pixels(kTightBytesPerRow * kHeight);
  for (size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = static_cast<uint8_t>(i + 1);
  }
  return pixels;
}

} // namespace

class WebGPUReadbackTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {
    EXPECT_EQ(device_->getContext().getUncapturedErrorCount(), 0u);
  }

 protected:
  [[nodiscard]] webgpu::Handle<WGPUTexture> createTexture(WGPUTextureFormat format,
                                                          const std::vector<uint8_t>& pixels) {
    WGPUTextureDescriptor desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.usage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
    desc.size = {.width = kWidth, .height = kHeight, .depthOrArrayLayers = 1};
    desc.format = format;
    webgpu::Handle<WGPUTexture> texture(
        wgpuDeviceCreateTexture(device_->getContext().getDevice(), &desc));
    const WGPUTexelCopyTextureInfo destination = {
        .texture = texture.get(),
        .mipLevel = 0,
        .origin = {.x = 0, .y = 0, .z = 0},
        .aspect = WGPUTextureAspect_All,
    };
    const WGPUTexelCopyBufferLayout layout = {
        .offset = 0,
        .bytesPerRow = static_cast<uint32_t>(kTightBytesPerRow),
        .rowsPerImage = kHeight,
    };
    const WGPUExtent3D extent = {.width = kWidth, .height = kHeight, .depthOrArrayLayers = 1};
    wgpuQueueWriteTexture(device_->getContext().getQueue(),
                          &destination,
                          pixels.data(),
                          pixels.size(),
                          &layout,
                          &extent);
    return texture;
  }

  std::unique_ptr<webgpu::Device> device_;
};

TEST_F(WebGPUReadbackTest, FlipsRowsAndRemovesPadding) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);

  std::vector<uint8_t> result(pixels.size());
  const Result ret = webgpu::readTexture(device_->getContext(),
                                         {.texture = texture.get(),
                                          .width = kWidth,
                                          .height = kHeight,
                                          .bytesPerTexel = kBytesPerTexel},
                                         result.data());
  ASSERT_TRUE(ret.isOk()) << ret.message;

  const std::vector<uint8_t> bottomRow(pixels.begin() + kTightBytesPerRow, pixels.end());
  const std::vector<uint8_t> topRow(pixels.begin(), pixels.begin() + kTightBytesPerRow);
  EXPECT_EQ(std::vector<uint8_t>(result.begin(), result.begin() + kTightBytesPerRow), bottomRow);
  EXPECT_EQ(std::vector<uint8_t>(result.begin() + kTightBytesPerRow, result.end()), topRow);
}

TEST_F(WebGPUReadbackTest, KeepsRowOrderWithoutFlip) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);

  std::vector<uint8_t> result(pixels.size());
  const Result ret = webgpu::readTexture(device_->getContext(),
                                         {.texture = texture.get(),
                                          .width = kWidth,
                                          .height = kHeight,
                                          .bytesPerTexel = kBytesPerTexel,
                                          .flipVertically = false},
                                         result.data());
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(result, pixels);
}

TEST_F(WebGPUReadbackTest, HonorsDestinationRowPitch) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);

  constexpr size_t kPitch = kTightBytesPerRow + 4;
  std::vector<uint8_t> result(kPitch * kHeight, 0xcd);
  const Result ret = webgpu::readTexture(device_->getContext(),
                                         {.texture = texture.get(),
                                          .width = kWidth,
                                          .height = kHeight,
                                          .bytesPerTexel = kBytesPerTexel,
                                          .dstBytesPerRow = kPitch,
                                          .flipVertically = false},
                                         result.data());
  ASSERT_TRUE(ret.isOk()) << ret.message;
  for (uint32_t row = 0; row < kHeight; ++row) {
    EXPECT_TRUE(std::equal(pixels.begin() + row * kTightBytesPerRow,
                           pixels.begin() + (row + 1) * kTightBytesPerRow,
                           result.begin() + row * kPitch));
    EXPECT_EQ(result[row * kPitch + kTightBytesPerRow], 0xcd);
  }
}

TEST_F(WebGPUReadbackTest, ReadsSubRegion) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);

  std::array<uint8_t, kBytesPerTexel> texel{};
  const Result ret = webgpu::readTexture(device_->getContext(),
                                         {.texture = texture.get(),
                                          .x = 2,
                                          .y = 1,
                                          .width = 1,
                                          .height = 1,
                                          .bytesPerTexel = kBytesPerTexel},
                                         texel.data());
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const size_t offset = kTightBytesPerRow + 2 * kBytesPerTexel;
  EXPECT_TRUE(std::equal(texel.begin(), texel.end(), pixels.begin() + offset));
}

TEST_F(WebGPUReadbackTest, DoesNotSwizzleBGRA) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_BGRA8Unorm, pixels);

  std::vector<uint8_t> result(pixels.size());
  const Result ret = webgpu::readTexture(device_->getContext(),
                                         {.texture = texture.get(),
                                          .width = kWidth,
                                          .height = kHeight,
                                          .bytesPerTexel = kBytesPerTexel,
                                          .flipVertically = false},
                                         result.data());
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(result, pixels);
}

TEST_F(WebGPUReadbackTest, RejectsInvalidRequests) {
  std::array<uint8_t, 16> bytes{};
  EXPECT_EQ(webgpu::readTexture(device_->getContext(), {}, bytes.data()).code,
            Result::Code::ArgumentInvalid);

  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);
  EXPECT_EQ(webgpu::readTexture(device_->getContext(),
                                {.texture = texture.get(),
                                 .width = kWidth,
                                 .height = kHeight,
                                 .bytesPerTexel = kBytesPerTexel,
                                 .dstBytesPerRow = 4},
                                bytes.data())
                .code,
            Result::Code::ArgumentOutOfRange);
}

TEST_F(WebGPUReadbackTest, OutOfBoundsCopyReturnsValidationError) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);
  std::vector<uint8_t> result(64);
  const Result ret = webgpu::readTexture(device_->getContext(),
                                         {.texture = texture.get(),
                                          .width = kWidth + 1,
                                          .height = kHeight,
                                          .bytesPerTexel = kBytesPerTexel},
                                         result.data());
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

} // namespace igl::tests
