/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../../data/TextureData.h"
#include "../../util/Common.h"

#include <IGLU/texture_loader/ITextureLoader.h>
#include <gtest/gtest.h>
#include <igl/Common.h>

namespace igl::tests {

class TestTextureLoader : public iglu::textureloader::ITextureLoader {
 public:
  TestTextureLoader(iglu::textureloader::DataReader reader,
                    TextureDesc::TextureUsage usage) noexcept :
    iglu::textureloader::ITextureLoader(reader, usage) {}

  TextureDesc& descriptorRef() noexcept {
    return mutableDescriptor();
  }
};

class BaseTextureLoaderTest : public ::testing::Test {
 private:
 public:
  BaseTextureLoaderTest() = default;
  ~BaseTextureLoaderTest() override = default;

  //
  // SetUp()
  // Create device, commandQueue
  //
  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);
  }

  void TearDown() override {}

  // Member variables
 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(BaseTextureLoaderTest, CheckCapabilities) {
  Result result;
  auto dataReader = iglu::textureloader::DataReader::tryCreate(
      reinterpret_cast<const uint8_t*>(data::texture::TEX_RGBA_2x2),
      sizeof(data::texture::TEX_RGBA_2x2),
      &result);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(dataReader.has_value());

  TestTextureLoader loader(*dataReader, TextureDesc::TextureUsageBits::Sampled);
  loader.descriptorRef().format = TextureFormat::RGBA_UNorm8;

  ASSERT_TRUE(loader.isSupported(*iglDev_));

  ASSERT_FALSE(loader.canUploadSourceData());
  ASSERT_FALSE(loader.canUseExternalMemory());
  ASSERT_FALSE(loader.shouldGenerateMipmaps());
}

TEST_F(BaseTextureLoaderTest, CreateTexture) {
  Result result;
  auto dataReader = iglu::textureloader::DataReader::tryCreate(
      reinterpret_cast<const uint8_t*>(data::texture::TEX_RGBA_2x2),
      sizeof(data::texture::TEX_RGBA_2x2),
      &result);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(dataReader.has_value());

  TestTextureLoader loader(*dataReader, TextureDesc::TextureUsageBits::Sampled);
  loader.descriptorRef().type = TextureType::TwoD;
  loader.descriptorRef().format = TextureFormat::RGBA_UNorm8;
  EXPECT_TRUE(loader.create(*iglDev_, &result) != nullptr);
  EXPECT_TRUE(result.isOk());

  EXPECT_TRUE(loader.create(*iglDev_, TextureFormat::RGBA_UNorm8, &result) != nullptr);
  EXPECT_TRUE(result.isOk());

  EXPECT_TRUE(loader.create(*iglDev_, TextureDesc::TextureUsageBits::Sampled, &result) != nullptr);
  EXPECT_TRUE(result.isOk());
}

TEST_F(BaseTextureLoaderTest, UploadTexture) {
  Result result;
  auto dataReader = iglu::textureloader::DataReader::tryCreate(
      reinterpret_cast<const uint8_t*>(data::texture::TEX_RGBA_2x2),
      sizeof(data::texture::TEX_RGBA_2x2),
      &result);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(dataReader.has_value());

  TestTextureLoader loader(*dataReader, TextureDesc::TextureUsageBits::Sampled);
  loader.descriptorRef().type = TextureType::TwoD;
  loader.descriptorRef().format = TextureFormat::RGBA_UNorm8;
  auto texture = loader.create(*iglDev_, &result);
  ASSERT_TRUE(texture != nullptr);
  ASSERT_TRUE(result.isOk());

  loader.upload(*texture, &result);
  ASSERT_TRUE(result.isOk());

  uint8_t data = 0;
  loader.loadToExternalMemory(&data, 0, &result);
  ASSERT_FALSE(result.isOk());
}
} // namespace igl::tests
