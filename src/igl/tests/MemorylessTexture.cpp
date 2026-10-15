/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "util/Common.h"

#include <igl/Texture.h>

namespace igl::tests {

class MemorylessTextureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);
  }

  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(MemorylessTextureTest, MemorylessAttachmentCreates) {
  Result ret;
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };
  const auto tex = iglDev_->createTexture(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(tex, nullptr);
}

TEST_F(MemorylessTextureTest, MemorylessInputAttachmentCreates) {
  Result ret;
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment |
               TextureDesc::TextureUsageBits::InputAttachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };
  const auto tex = iglDev_->createTexture(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(tex, nullptr);
}

TEST_F(MemorylessTextureTest, MemorylessRejectsSampledUsage) {
  Result ret;
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };
  const auto tex = iglDev_->createTexture(desc, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(tex, nullptr);
}

TEST_F(MemorylessTextureTest, MemorylessRejectsStorageUsage) {
  Result ret;
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Storage,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };
  const auto tex = iglDev_->createTexture(desc, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(tex, nullptr);
}

TEST_F(MemorylessTextureTest, MemorylessRejectsMipmaps) {
  Result ret;
  TextureDesc desc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Attachment);
  desc.numMipLevels = 2;
  desc.storage = ResourceStorage::Memoryless;

  const auto tex = iglDev_->createTexture(desc, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(tex, nullptr);
}

TEST_F(MemorylessTextureTest, MemorylessRejects3D) {
  Result ret;
  const TextureDesc desc = TextureDesc::new3D(
      TextureFormat::RGBA_UNorm8, 4, 4, 4, TextureDesc::TextureUsageBits::Attachment);
  TextureDesc memorylessDesc = desc;
  memorylessDesc.storage = ResourceStorage::Memoryless;

  const auto tex = iglDev_->createTexture(memorylessDesc, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(tex, nullptr);
}

TEST_F(MemorylessTextureTest, InputAttachmentRequiresAttachment) {
  Result ret;
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::InputAttachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
  };
  const auto tex = iglDev_->createTexture(desc, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(tex, nullptr);
}

TEST_F(MemorylessTextureTest, GetStorageReturnsMemoryless) {
  Result ret;
  const TextureDesc memorylessDesc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };
  const auto memorylessTex = iglDev_->createTexture(memorylessDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(memorylessTex, nullptr);
  EXPECT_EQ(memorylessTex->getStorage(), ResourceStorage::Memoryless);
}

TEST_F(MemorylessTextureTest, GetStorageReturnsPrivate) {
  Result ret;
  const TextureDesc privateDesc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Private,
  };
  const auto privateTex = iglDev_->createTexture(privateDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(privateTex, nullptr);
  EXPECT_EQ(privateTex->getStorage(), ResourceStorage::Private);
}

TEST_F(MemorylessTextureTest, GetStorageReturnsInvalidByDefault) {
  Result ret;
  TextureDesc defaultDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Attachment);
  const auto defaultTex = iglDev_->createTexture(defaultDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(defaultTex, nullptr);
  EXPECT_EQ(defaultTex->getStorage(), ResourceStorage::Invalid);
}

TEST_F(MemorylessTextureTest, MemorylessRejectsExportable) {
  Result ret;
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
      .exportability = TextureDesc::TextureExportability::Exportable,
  };
  const auto tex = iglDev_->createTexture(desc, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(tex, nullptr);
}

TEST_F(MemorylessTextureTest, MemorylessRejectsLinearTiling) {
  Result ret;
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
      .tiling = TextureDesc::TextureTiling::Linear,
  };
  const auto tex = iglDev_->createTexture(desc, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(tex, nullptr);
}

TEST_F(MemorylessTextureTest, MemorylessRejectsCube) {
  Result ret;
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::Cube,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };
  const auto tex = iglDev_->createTexture(desc, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(tex, nullptr);
}

TEST_F(MemorylessTextureTest, MemorylessAccepts2DArray) {
  if (!iglDev_->hasFeature(DeviceFeatures::Texture2DArray)) {
    GTEST_SKIP() << "Texture2DArray not supported";
  }

  Result ret;
  const TextureDesc desc = TextureDesc::new2DArray(
      TextureFormat::RGBA_UNorm8, 4, 4, 2, TextureDesc::TextureUsageBits::Attachment);
  TextureDesc memorylessDesc = desc;
  memorylessDesc.storage = ResourceStorage::Memoryless;

  const auto tex = iglDev_->createTexture(memorylessDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(tex, nullptr);
}

} // namespace igl::tests
