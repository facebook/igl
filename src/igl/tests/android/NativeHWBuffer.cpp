/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/android/NativeHWBuffer.h>

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

#include <android/hardware_buffer.h>

namespace igl::tests {

using namespace igl::android;

class NativeHWBufferTest : public ::testing::Test {
 private:
 public:
  NativeHWBufferTest() = default;
  ~NativeHWBufferTest() override = default;

  void SetUp() override {}
  void TearDown() override {}
};

class NativeHWTextureBufferTest : public igl::android::INativeHWTextureBuffer {
 public:
  ~NativeHWTextureBufferTest() override {}

 protected:
  Result createTextureInternal(const TextureDesc& desc, AHardwareBuffer* buffer) override {
    return Result();
  }
};

TEST_F(NativeHWBufferTest, Basic_getNativeHWFormat) {
  EXPECT_EQ(getNativeHWFormat(TextureFormat::RGBX_UNorm8), AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::RGBA_UNorm8), AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::B5G6R5_UNorm), AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::RGBA_F16), AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::RGB10_A2_UNorm_Rev),
            AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::Z_UNorm16), AHARDWAREBUFFER_FORMAT_D16_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::Z_UNorm24), AHARDWAREBUFFER_FORMAT_D24_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::Z_UNorm32), AHARDWAREBUFFER_FORMAT_D32_FLOAT);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::S8_UInt_Z24_UNorm),
            AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::S_UInt8), AHARDWAREBUFFER_FORMAT_S8_UINT);
  EXPECT_EQ(getNativeHWFormat(igl::TextureFormat::YUV_NV12),
            AHARDWAREBUFFER_FORMAT_YCbCr_420_SP_VENUS);
  EXPECT_EQ(getNativeHWFormat(igl::TextureFormat::Invalid), 0);
}

TEST_F(NativeHWBufferTest, Basic_getIglFormat) {
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM),
            TextureFormat::RGBX_UNorm8);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM),
            TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM),
            TextureFormat::B5G6R5_UNorm);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT),
            TextureFormat::RGBA_F16);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM),
            TextureFormat::RGB10_A2_UNorm_Rev);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_D16_UNORM), TextureFormat::Z_UNorm16);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_D24_UNORM), TextureFormat::Z_UNorm24);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_D32_FLOAT), TextureFormat::Z_UNorm32);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT),
            TextureFormat::S8_UInt_Z24_UNorm);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_S8_UINT), TextureFormat::S_UInt8);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_YCbCr_420_SP_VENUS),
            TextureFormat::YUV_NV12);
}

TEST_F(NativeHWBufferTest, getNativeHWBufferUsage) {
  EXPECT_TRUE(getNativeHWBufferUsage(TextureDesc::TextureUsageBits::Sampled) |
              AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE);
  EXPECT_TRUE(getNativeHWBufferUsage(TextureDesc::TextureUsageBits::Storage) |
              AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN);
  EXPECT_TRUE(getNativeHWBufferUsage(TextureDesc::TextureUsageBits::Attachment) |
              AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT);
}

TEST_F(NativeHWBufferTest, getIglBufferUsage) {
  EXPECT_TRUE(getIglBufferUsage(AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) |
              TextureDesc::TextureUsageBits::Sampled);
  EXPECT_TRUE(getIglBufferUsage(AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                                AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN) |
              TextureDesc::TextureUsageBits::Storage);
  EXPECT_TRUE(getIglBufferUsage(AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) |
              TextureDesc::TextureUsageBits::Attachment);
}

TEST_F(NativeHWBufferTest, allocateNativeHWBuffer) {
  AHardwareBuffer* hwBuffer;
  EXPECT_TRUE(allocateNativeHWBuffer(
                  TextureDesc::newNativeHWBufferImage(
                      TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled, 100, 100),
                  false,
                  &hwBuffer)
                  .isOk());
  EXPECT_NE(hwBuffer, nullptr);

  AHardwareBuffer_Desc hwbDesc;
  AHardwareBuffer_describe(hwBuffer, &hwbDesc);

  AHardwareBuffer_release(hwBuffer);

  EXPECT_EQ(hwbDesc.format, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);
  EXPECT_EQ(hwbDesc.width, 100);
  EXPECT_EQ(hwbDesc.height, 100);
  EXPECT_EQ(hwbDesc.layers, 1);
  EXPECT_EQ(hwbDesc.usage, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE);
}

TEST_F(NativeHWBufferTest, LockBuffer) {
  AHardwareBuffer* hwBuffer;
  EXPECT_TRUE(allocateNativeHWBuffer(
                  TextureDesc::newNativeHWBufferImage(
                      TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled, 100, 100),
                  false,
                  &hwBuffer)
                  .isOk());
  EXPECT_NE(hwBuffer, nullptr);

  {
    NativeHWTextureBufferTest testTxBuffer;

    EXPECT_TRUE(testTxBuffer.attachHWBuffer(hwBuffer).isOk());

    std::byte* bytes = nullptr;
    INativeHWTextureBuffer::RangeDesc outRange;
    Result outResult;
    auto lockGuard = testTxBuffer.lockHWBuffer(&bytes, outRange, &outResult);

    EXPECT_TRUE(outResult.isOk());
    EXPECT_NE(bytes, nullptr);
    EXPECT_EQ(outRange.width, 100);
    EXPECT_EQ(outRange.height, 100);
    EXPECT_EQ(outRange.layer, 1);
    EXPECT_EQ(outRange.mipLevel, 1);

    lockGuard.~LockGuard();
  }

  AHardwareBuffer_release(hwBuffer);
}

} // namespace igl::tests

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
