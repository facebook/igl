/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/Readback.h>

#include <IGLU/texture_accessor/WebGPUTextureAccessor.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <vector>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/webgpu/PlatformDevice.h>

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

TEST_F(WebGPUReadbackTest, AsyncReadbackCompletesByPolling) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);

  webgpu::AsyncTextureReadback readback;
  std::vector<uint8_t> result(pixels.size());
  EXPECT_EQ(readback.copyTo(result.data()).code, Result::Code::InvalidOperation);

  Result ret = readback.begin(device_->getContext(),
                              {.texture = texture.get(),
                               .width = kWidth,
                               .height = kHeight,
                               .bytesPerTexel = kBytesPerTexel,
                               .flipVertically = false});
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_TRUE(readback.isPending());
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!readback.poll() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  ASSERT_TRUE(readback.poll()) << "The readback did not complete within 10 seconds";
  ret = readback.copyTo(result.data());
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_FALSE(readback.isPending());
  EXPECT_EQ(result, pixels);
}

TEST_F(WebGPUReadbackTest, AsyncReadbackCanWaitOrBeDropped) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);
  const webgpu::TextureReadbackDesc desc = {
      .texture = texture.get(),
      .width = kWidth,
      .height = kHeight,
      .bytesPerTexel = kBytesPerTexel,
  };

  {
    webgpu::AsyncTextureReadback dropped;
    ASSERT_TRUE(dropped.begin(device_->getContext(), desc).isOk());
  }

  webgpu::AsyncTextureReadback readback;
  ASSERT_TRUE(readback.begin(device_->getContext(), desc).isOk());
  // Restarting drops the first readback.
  ASSERT_TRUE(readback.begin(device_->getContext(), desc).isOk());
  Result ret = readback.wait();
  ASSERT_TRUE(ret.isOk()) << ret.message;
  std::vector<uint8_t> result(pixels.size());
  ret = readback.copyTo(result.data());
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(std::vector<uint8_t>(result.begin(), result.begin() + kTightBytesPerRow),
            std::vector<uint8_t>(pixels.begin() + kTightBytesPerRow, pixels.end()));
  EXPECT_EQ(std::vector<uint8_t>(result.begin() + kTightBytesPerRow, result.end()),
            std::vector<uint8_t>(pixels.begin(), pixels.begin() + kTightBytesPerRow));
  device_->getContext().processEvents();
}

TEST_F(WebGPUReadbackTest, PlatformDeviceReadsPixelsAndBuffersAsync) {
  Result ret;
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = device_->createTexture(
      TextureDesc::new2D(
          TextureFormat::RGBA_UNorm8, kWidth, kHeight, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const auto range = TextureRangeDesc::new2D(0, 0, kWidth, kHeight);
  ASSERT_TRUE(texture->upload(range, pixels.data()).isOk());
  const auto buffer = device_->createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Storage, pixels.data(), pixels.size()), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto* platformDevice =
      static_cast<IDevice&>(*device_).getPlatformDevice<webgpu::PlatformDevice>();
  ASSERT_NE(platformDevice, nullptr);

  webgpu::AsyncTextureReadback pixelReadback;
  ret = platformDevice->readPixelsAsync(*texture, range, pixelReadback, /*flipVertically=*/false);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  webgpu::AsyncBufferReadback bufferReadback;
  ret = platformDevice->mapBufferAsync(*buffer, 4, pixels.size() - 4, bufferReadback);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_TRUE(pixelReadback.wait().isOk());
  ASSERT_TRUE(bufferReadback.wait().isOk());

  std::vector<uint8_t> result(pixels.size());
  ASSERT_TRUE(pixelReadback.copyTo(result.data()).isOk());
  EXPECT_EQ(result, pixels);
  std::vector<uint8_t> bytes(bufferReadback.getSize());
  ASSERT_TRUE(bufferReadback.copyTo(bytes.data()).isOk());
  EXPECT_EQ(bytes, std::vector<uint8_t>(pixels.begin() + 4, pixels.end()));

  EXPECT_EQ(platformDevice->mapBufferAsync(*buffer, 4, pixels.size(), bufferReadback).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(platformDevice
                ->mapBufferAsync(*buffer, std::numeric_limits<size_t>::max() - 3, 8, bufferReadback)
                .code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(platformDevice->mapBufferAsync(*buffer, 2, 4, bufferReadback).code,
            Result::Code::ArgumentInvalid);
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

TEST_F(WebGPUReadbackTest, TextureAccessorResizesForTheRequestedTexture) {
  Result ret;
  auto queue = device_->createCommandQueue({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto rgba8 = device_->createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto rgba32 = device_->createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_F32, 2, 2, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  std::array<float, 16> pixels = {};
  for (size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = static_cast<float>(i);
  }
  ASSERT_TRUE(rgba32->upload(rgba32->getFullRange(0), pixels.data()).isOk());

  iglu::textureaccessor::WebGPUTextureAccessor accessor(rgba8, *device_);
  accessor.requestBytes(*queue, rgba32);
  const std::vector<unsigned char>& bytes = accessor.getBytes();
  ASSERT_EQ(bytes.size(), sizeof(pixels));
  std::array<float, 16> readBack = {};
  std::memcpy(readBack.data(), bytes.data(), sizeof(readBack));
  EXPECT_EQ(readBack, pixels);
}

TEST_F(WebGPUReadbackTest, TextureAccessorFailedReadbackIsNotReady) {
  Result ret;
  auto queue = device_->createCommandQueue({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto texture = device_->createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  iglu::textureaccessor::WebGPUTextureAccessor accessor(texture, *device_);
  accessor.requestBytes(*queue, nullptr);
  // Destroying the device before any event processing aborts the mapping.
  wgpuDeviceDestroy(device_->getContext().getDevice());
  std::vector<unsigned char> bytes(16);
  EXPECT_EQ(accessor.copyBytes(bytes.data(), bytes.size()), 0u);
  EXPECT_EQ(accessor.getRequestStatus(), iglu::textureaccessor::RequestStatus::NotInitialized);
}

TEST_F(WebGPUReadbackTest, TextureAccessorRejectsCompressedTextures) {
  Result ret;
  auto queue = device_->createCommandQueue({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  TextureFormat format = TextureFormat::Invalid;
  for (const TextureFormat candidate : {TextureFormat::RGBA_BC7_UNORM_4x4,
                                        TextureFormat::RGBA_ASTC_4x4,
                                        TextureFormat::RGB8_ETC2}) {
    if (device_->getTextureFormatCapabilities(candidate) != 0) {
      format = candidate;
      break;
    }
  }
  if (format == TextureFormat::Invalid) {
    GTEST_SKIP() << "No compressed texture format";
  }
  auto texture = device_->createTexture(
      TextureDesc::new2D(format, 8, 8, TextureDesc::TextureUsageBits::Sampled), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  // Reading compressed blocks as texels would write past the block-sized buffer.
  iglu::textureaccessor::WebGPUTextureAccessor accessor(texture, *device_);
  accessor.requestBytes(*queue, nullptr);
  EXPECT_EQ(accessor.getRequestStatus(), iglu::textureaccessor::RequestStatus::NotInitialized);
}

TEST_F(WebGPUReadbackTest, AsyncReadbackRejectsSmallDestinations) {
  const std::vector<uint8_t> pixels = makePixels();
  const auto texture = createTexture(WGPUTextureFormat_RGBA8Unorm, pixels);
  webgpu::AsyncTextureReadback readback;
  ASSERT_TRUE(readback
                  .begin(device_->getContext(),
                         {.texture = texture.get(),
                          .width = kWidth,
                          .height = kHeight,
                          .bytesPerTexel = kBytesPerTexel})
                  .isOk());
  ASSERT_TRUE(readback.wait().isOk());
  std::vector<uint8_t> result(pixels.size() - 1);
  EXPECT_EQ(readback.copyTo(result.data(), result.size()).code, Result::Code::ArgumentOutOfRange);
  EXPECT_FALSE(readback.isPending());
}

TEST_F(WebGPUReadbackTest, PlatformDeviceReadsA3DSliceAndRejectsBadRanges) {
  Result ret;
  auto volume = device_->createTexture(
      TextureDesc::new3D(
          TextureFormat::RGBA_UNorm8, 2, 2, 2, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const std::array<uint32_t, 8> texels = {1, 1, 1, 1, 2, 2, 2, 2};
  ASSERT_TRUE(volume->upload(volume->getFullRange(0), texels.data()).isOk());
  const auto* platformDevice =
      static_cast<IDevice&>(*device_).getPlatformDevice<webgpu::PlatformDevice>();
  ASSERT_NE(platformDevice, nullptr);

  webgpu::AsyncTextureReadback readback;
  TextureRangeDesc slice = TextureRangeDesc::new2D(0, 0, 2, 2);
  slice.z = 1;
  ASSERT_TRUE(platformDevice->readPixelsAsync(*volume, slice, readback).isOk());
  ASSERT_TRUE(readback.wait().isOk());
  std::array<uint32_t, 4> result = {};
  ASSERT_TRUE(readback.copyTo(result.data(), sizeof(result)).isOk());
  EXPECT_EQ(result, (std::array<uint32_t, 4>{2, 2, 2, 2}));

  EXPECT_EQ(
      platformDevice->readPixelsAsync(*volume, TextureRangeDesc::new2D(1, 0, 2, 2), readback).code,
      Result::Code::ArgumentOutOfRange);
  TextureRangeDesc pastLastSlice = slice;
  pastLastSlice.z = 2;
  EXPECT_EQ(platformDevice->readPixelsAsync(*volume, pastLastSlice, readback).code,
            Result::Code::ArgumentOutOfRange);
  auto flat = device_->createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  TextureRangeDesc secondLayer = TextureRangeDesc::new2D(0, 0, 2, 2);
  secondLayer.layer = 1;
  EXPECT_EQ(platformDevice->readPixelsAsync(*flat, secondLayer, readback).code,
            Result::Code::ArgumentOutOfRange);
  TextureDesc msaaDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Attachment);
  msaaDesc.numSamples = 4;
  auto msaa = device_->createTexture(msaaDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(
      platformDevice->readPixelsAsync(*msaa, TextureRangeDesc::new2D(0, 0, 2, 2), readback).code,
      Result::Code::Unsupported);
}

} // namespace igl::tests
