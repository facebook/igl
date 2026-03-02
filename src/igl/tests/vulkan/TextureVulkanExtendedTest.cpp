/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <igl/Device.h>
#include <igl/Texture.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanTexture.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class TextureVulkanExtendedTest : public ::testing::Test {
 public:
  TextureVulkanExtendedTest() = default;
  ~TextureVulkanExtendedTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
};

TEST_F(TextureVulkanExtendedTest, Create3D) {
  if (!iglDev_->hasFeature(DeviceFeatures::Texture3D)) {
    GTEST_SKIP() << "3D textures not supported.";
  }

  Result ret;
  TextureDesc desc = TextureDesc::new3D(
      TextureFormat::RGBA_UNorm8, 4, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->getType(), TextureType::ThreeD);
}

TEST_F(TextureVulkanExtendedTest, CreateCube) {
  Result ret;
  TextureDesc desc = TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->getType(), TextureType::Cube);
}

TEST_F(TextureVulkanExtendedTest, CreateArray) {
  if (!iglDev_->hasFeature(DeviceFeatures::Texture2DArray)) {
    GTEST_SKIP() << "Texture arrays not supported.";
  }

  Result ret;
  TextureDesc desc = TextureDesc::new2DArray(
      TextureFormat::RGBA_UNorm8, 4, 4, 3, TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->getType(), TextureType::TwoDArray);
  EXPECT_EQ(texture->getNumLayers(), 3u);
}

TEST_F(TextureVulkanExtendedTest, CreateMSAA) {
  Result ret;
  TextureDesc desc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Attachment);
  desc.numSamples = 4;
  auto texture = iglDev_->createTexture(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->getSamples(), 4u);
}

TEST_F(TextureVulkanExtendedTest, DepthStencilFormats) {
  Result ret;

  TextureDesc desc = TextureDesc::new2D(TextureFormat::Z_UNorm24,
                                        4,
                                        4,
                                        TextureDesc::TextureUsageBits::Attachment |
                                            TextureDesc::TextureUsageBits::Sampled);
  auto depthTexture = iglDev_->createTexture(desc, &ret);
  if (ret.isOk()) {
    ASSERT_NE(depthTexture, nullptr);
  }

  TextureDesc descDS = TextureDesc::new2D(TextureFormat::S8_UInt_Z24_UNorm,
                                          4,
                                          4,
                                          TextureDesc::TextureUsageBits::Attachment |
                                              TextureDesc::TextureUsageBits::Sampled);
  auto dsTexture = iglDev_->createTexture(descDS, &ret);
  if (ret.isOk()) {
    ASSERT_NE(dsTexture, nullptr);
  }
}

TEST_F(TextureVulkanExtendedTest, TextureId) {
  Result ret;
  TextureDesc desc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);

  auto* vulkanTexture = static_cast<igl::vulkan::Texture*>(texture.get());
  ASSERT_NE(vulkanTexture, nullptr);
  EXPECT_NE(vulkanTexture->getVulkanTexture().textureId_, 0u);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
