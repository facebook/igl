/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <cstring>
#include <vector>
#include <igl/IGL.h>
#include <igl/metal/CommandBuffer.h>
#include <igl/metal/Device.h>
#include <igl/metal/Texture.h>

namespace igl::tests {

#define ROUNDTRIP_TEX_WIDTH 4
#define ROUNDTRIP_TEX_HEIGHT 4

//
// MetalTextureDataRoundtripTest
//
// This test covers uploading texture data and reading it back on Metal.
// Tests RGBA upload/readback, mip level uploads, and 3D textures.
//
class MetalTextureDataRoundtripTest : public ::testing::Test {
 public:
  MetalTextureDataRoundtripTest() = default;
  ~MetalTextureDataRoundtripTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// UploadAndReadback_RGBA
//
// Upload RGBA data to a texture, read it back via framebuffer copyBytes, and verify match.
//
TEST_F(MetalTextureDataRoundtripTest, UploadAndReadback_RGBA) {
  Result res;

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           ROUNDTRIP_TEX_WIDTH,
                                           ROUNDTRIP_TEX_HEIGHT,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  auto texture = device_->createTexture(texDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(texture, nullptr);

  // Prepare test data: solid green
  const size_t numPixels = ROUNDTRIP_TEX_WIDTH * ROUNDTRIP_TEX_HEIGHT;
  std::vector<uint32_t> uploadData(numPixels, 0xFF00FF00); // RGBA: green with full alpha

  auto range = TextureRangeDesc::new2D(0, 0, ROUNDTRIP_TEX_WIDTH, ROUNDTRIP_TEX_HEIGHT);
  res = texture->upload(range, uploadData.data());
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create framebuffer to read back
  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = texture;
  auto framebuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  std::vector<uint32_t> readbackData(numPixels, 0);
  framebuffer->copyBytesColorAttachment(
      *cmdQueue_, 0, readbackData.data(), range, ROUNDTRIP_TEX_WIDTH * 4);

  for (size_t i = 0; i < numPixels; ++i) {
    ASSERT_EQ(readbackData[i], uploadData[i]) << "Mismatch at pixel " << i;
  }
}

//
// UploadToMipLevel
//
// Upload data to a specific mip level and verify the upload succeeds.
//
TEST_F(MetalTextureDataRoundtripTest, UploadToMipLevel) {
  Result res;

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           ROUNDTRIP_TEX_WIDTH,
                                           ROUNDTRIP_TEX_HEIGHT,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = 2; // base (4x4) + mip 1 (2x2)

  auto texture = device_->createTexture(texDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(texture, nullptr);

  // Upload to mip level 0
  const size_t numPixels0 = ROUNDTRIP_TEX_WIDTH * ROUNDTRIP_TEX_HEIGHT;
  std::vector<uint32_t> data0(numPixels0, 0xFFFF0000);
  res = texture->upload(texture->getFullRange(0), data0.data());
  ASSERT_TRUE(res.isOk()) << res.message;

  // Upload to mip level 1 (2x2)
  const size_t numPixels1 = (ROUNDTRIP_TEX_WIDTH / 2) * (ROUNDTRIP_TEX_HEIGHT / 2);
  std::vector<uint32_t> data1(numPixels1, 0xFF0000FF);
  res = texture->upload(texture->getFullRange(1), data1.data());
  ASSERT_TRUE(res.isOk()) << res.message;
}

//
// Upload3DTexture
//
// Upload data to a 3D texture and verify the upload succeeds.
//
TEST_F(MetalTextureDataRoundtripTest, Upload3DTexture) {
  Result res;

  constexpr uint32_t kWidth = 4;
  constexpr uint32_t kHeight = 4;
  constexpr uint32_t kDepth = 4;

  TextureDesc texDesc;
  texDesc.type = TextureType::ThreeD;
  texDesc.format = TextureFormat::RGBA_UNorm8;
  texDesc.width = kWidth;
  texDesc.height = kHeight;
  texDesc.depth = kDepth;
  texDesc.numMipLevels = 1;
  texDesc.usage = TextureDesc::TextureUsageBits::Sampled;

  auto texture = device_->createTexture(texDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(texture, nullptr);

  const size_t numPixels = kWidth * kHeight * kDepth;
  std::vector<uint32_t> data(numPixels, 0xFF808080);

  auto range = TextureRangeDesc::new3D(0, 0, 0, kWidth, kHeight, kDepth);
  res = texture->upload(range, data.data());
  ASSERT_TRUE(res.isOk()) << res.message;
}

} // namespace igl::tests
