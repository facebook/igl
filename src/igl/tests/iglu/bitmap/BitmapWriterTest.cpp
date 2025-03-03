/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../../util/TestDevice.h"
#include <IGLU/bitmap/BitmapWriter.h>

#include <sstream>

namespace {
// Dumped from a bmp file that was manually validated as being a checkerboard pattern
const uint8_t sExpectedData[] = {
    0x42, 0x4d, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28,
    0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00};
} // namespace

namespace igl::tests::BitmapWriter {

class BitmapWriterTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::createTestDevice();
    ASSERT_TRUE(device_ != nullptr);

    Result result;
    texDesc_ = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                  sTexWidth,
                                  sTexWidth,
                                  TextureDesc::TextureUsageBits::Sampled |
                                      TextureDesc::TextureUsageBits::Attachment);
    texture_ = device_->createTexture(texDesc_, &result);
    ASSERT_TRUE(result.isOk());

    // Initialize a red checkerboard pattern
    std::vector<uint8_t> data;
    data.reserve(sTexWidth * sTexWidth * 4);
    for (int i = 0; i < sTexWidth; i++) {
      for (int j = 0; j < sTexWidth; j++) {
        if (i % 2 != j % 2) {
          data.push_back(255);
        } else {
          data.push_back(0);
        }
        data.push_back(0);
        data.push_back(0);
        data.push_back(255);
      }
    }

    // Initialize texture data
    const auto range = igl::TextureRangeDesc::new2D(0, 0, sTexWidth, sTexWidth);
    texture_->upload(range, data.data());
  }

  void TearDown() override {}

  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ITexture> texture_;
  TextureDesc texDesc_;
  constexpr static size_t sTexWidth = 4;
};

TEST_F(BitmapWriterTest, WriteFile) {
  std::stringstream ss;
  igl::iglu::writeBitmap(ss, texture_, *device_);
  std::string s = ss.str();
  const uint8_t* fileData = reinterpret_cast<const uint8_t*>(s.c_str());
  ASSERT_NE(fileData, nullptr);
  ASSERT_EQ(s.size(), sizeof(sExpectedData));
  ASSERT_EQ(memcmp(fileData, sExpectedData, sizeof(sExpectedData)), 0);
}

} // namespace igl::tests::BitmapWriter
