/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../../data/TextureData.h"
#include "../../util/Common.h"

#include <IGLU/texture_loader/ITextureLoader.h>
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
      reinterpret_cast<const uint8_t*>(data::texture::kTexRgba2x2.data()),
      sizeof(data::texture::kTexRgba2x2),
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
      reinterpret_cast<const uint8_t*>(data::texture::kTexRgba2x2.data()),
      sizeof(data::texture::kTexRgba2x2),
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
      reinterpret_cast<const uint8_t*>(data::texture::kTexRgba2x2.data()),
      sizeof(data::texture::kTexRgba2x2),
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

TEST_F(BaseTextureLoaderTest, MemorySizeInBytes) {
  Result result;
  auto dataReader = iglu::textureloader::DataReader::tryCreate(
      reinterpret_cast<const uint8_t*>(data::texture::kTexRgba2x2.data()),
      sizeof(data::texture::kTexRgba2x2),
      &result);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(dataReader.has_value());

  TestTextureLoader loader(*dataReader, TextureDesc::TextureUsageBits::Sampled);
  loader.descriptorRef().type = TextureType::TwoD;
  loader.descriptorRef().format = TextureFormat::RGBA_UNorm8;
  loader.descriptorRef().width = 2;
  loader.descriptorRef().height = 2;

  // 2x2 RGBA_UNorm8 => 4 texels * 4 bytes/texel = 16 bytes.
  EXPECT_EQ(loader.memorySizeInBytes(), 16u);

  // The base loader exposes no per-mip byte breakdown.
  EXPECT_TRUE(loader.mipLevelBytes().empty());
}

TEST_F(BaseTextureLoaderTest, Load) {
  Result result;
  auto dataReader = iglu::textureloader::DataReader::tryCreate(
      reinterpret_cast<const uint8_t*>(data::texture::kTexRgba2x2.data()),
      sizeof(data::texture::kTexRgba2x2),
      &result);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(dataReader.has_value());

  TestTextureLoader loader(*dataReader, TextureDesc::TextureUsageBits::Sampled);
  loader.descriptorRef().type = TextureType::TwoD;
  loader.descriptorRef().format = TextureFormat::RGBA_UNorm8;
  loader.descriptorRef().width = 2;
  loader.descriptorRef().height = 2;

  auto data = loader.load(&result);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(data != nullptr);

  // The base loader copies the source bytes verbatim into the loaded IData.
  ASSERT_EQ(data->size(), sizeof(data::texture::kTexRgba2x2));
  const auto* loaded = reinterpret_cast<const uint32_t*>(data->data());
  for (size_t i = 0; i < data::texture::kTexRgba2x2.size(); ++i) {
    EXPECT_EQ(loaded[i], data::texture::kTexRgba2x2[i]);
  }
}

TEST_F(BaseTextureLoaderTest, IsSupportedWithExplicitUsage) {
  Result result;
  auto dataReader = iglu::textureloader::DataReader::tryCreate(
      reinterpret_cast<const uint8_t*>(data::texture::kTexRgba2x2.data()),
      sizeof(data::texture::kTexRgba2x2),
      &result);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(dataReader.has_value());

  TestTextureLoader loader(*dataReader, TextureDesc::TextureUsageBits::Sampled);
  loader.descriptorRef().format = TextureFormat::RGBA_UNorm8;

  // RGBA_UNorm8 is universally sampleable; the explicit-usage overload agrees.
  EXPECT_TRUE(loader.isSupported(*iglDev_, TextureDesc::TextureUsageBits::Sampled));
}
} // namespace igl::tests
