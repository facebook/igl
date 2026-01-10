/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/Texture.h>

#include <igl/Device.h>
#include <igl/tests/util/TestDevice.h>
#include <igl/tests/util/TextureValidationHelpers.h>

namespace igl::tests {

class TextureVulkanTest : public ::testing::Test {
 public:
  TextureVulkanTest() = default;
  ~TextureVulkanTest() override = default;

  // Set up common resources. This will create a device
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
  }

  void TearDown() override {}

  // Member variables
 protected:
  std::shared_ptr<IDevice> iglDev_;
};

TEST_F(TextureVulkanTest, MipmapGenerationFlagInitialization) {
  // Create a texture with default TextureDesc
  const TextureDesc texDesc = igl::TextureDesc::new2D(
      igl::TextureFormat::RGBA_UNorm8, 2, 2, igl::TextureDesc::TextureUsageBits::Sampled);

  Result result;
  auto texture = iglDev_->createTexture(texDesc, &result);
  ASSERT_EQ(result.code, igl::Result::Code::Ok);
  ASSERT_NE(texture, nullptr);

  // Cast to Vulkan texture to access the getMipmapGeneration method
  auto* vulkanTexture = static_cast<vulkan::Texture*>(texture.get());
  ASSERT_NE(vulkanTexture, nullptr);

  // Test that the mipmapGeneration flag is initialized to Manual by default
  ASSERT_EQ(vulkanTexture->getMipmapGeneration(), TextureDesc::TextureMipmapGeneration::Manual);
}

TEST_F(TextureVulkanTest, AutoGenerateMipmapOnUpload) {
  Result ret;

  // Create a texture with AutoGenerateOnUpload flag - similar to existing mipmap tests
  constexpr uint32_t kNumMipLevels = 2u;
  constexpr uint32_t kTexWidth = 2u;
  constexpr uint32_t kTexHeight = 2u;

  constexpr uint32_t kColor = 0xdeadbeef;
  constexpr std::array<uint32_t, 4> kBaseMipData = {kColor, kColor, kColor, kColor};
  constexpr std::array<uint32_t, 1> kExpectedMip1Data = {kColor}; // Should be same color after
                                                                  // generation

  // Create texture with AutoGenerateOnUpload flag
  TextureDesc textureDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                               kTexWidth,
                                               kTexHeight,
                                               TextureDesc::TextureUsageBits::Sampled |
                                                   TextureDesc::TextureUsageBits::Attachment);
  textureDesc.numMipLevels = kNumMipLevels;
  textureDesc.mipmapGeneration = TextureDesc::TextureMipmapGeneration::AutoGenerateOnUpload;

  auto texture = iglDev_->createTexture(textureDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);

  // Cast to Vulkan texture to access Vulkan-specific methods
  auto* vulkanTexture = static_cast<vulkan::Texture*>(texture.get());
  ASSERT_NE(vulkanTexture, nullptr);

  // Verify the texture was created with the correct mipmap generation flag
  ASSERT_EQ(vulkanTexture->getMipmapGeneration(),
            TextureDesc::TextureMipmapGeneration::AutoGenerateOnUpload);

  // Verify the texture has the expected number of mip levels
  ASSERT_EQ(vulkanTexture->getNumMipLevels(), kNumMipLevels);

  // Get command queue for validation
  CommandQueueDesc cmdQueueDesc{};
  auto cmdQueue = iglDev_->createCommandQueue(cmdQueueDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(cmdQueue, nullptr);

  // Upload data to mip level 0 - this should trigger automatic mipmap generation
  ret = texture->upload(texture->getFullRange(0), kBaseMipData.data());
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  // Validate that mip level 0 contains the uploaded data
  util::validateUploadedTextureRange(*iglDev_,
                                     *cmdQueue,
                                     texture,
                                     texture->getFullRange(0),
                                     kBaseMipData.data(),
                                     "AutoGen: Base level (0)");

  // Validate that mip level 1 was auto-generated with expected content
  // The auto-generated mip should contain the same solid color (averaged from base level)
  util::validateUploadedTextureRange(*iglDev_,
                                     *cmdQueue,
                                     texture,
                                     texture->getFullRange(1),
                                     kExpectedMip1Data.data(),
                                     "AutoGen: Generated level (1)");
}

TEST_F(TextureVulkanTest, ManualMipmapGeneration) {
  Result ret;

  constexpr uint32_t kColor = 0xdeadbeef;
  constexpr std::array<uint32_t, 4> kBaseMipData = {kColor, kColor, kColor, kColor};
  constexpr std::array<uint32_t, 1> kExpectedMip1Data = {kColor}; // Should be same color after
                                                                  // generation

  // Create a texture with Manual mipmap generation for comparison
  constexpr uint32_t textureSize = 2;
  const uint32_t expectedMipLevels = TextureDesc::calcNumMipLevels(textureSize, textureSize);

  TextureDesc textureDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                               textureSize,
                                               textureSize,
                                               TextureDesc::TextureUsageBits::Sampled |
                                                   TextureDesc::TextureUsageBits::Attachment);
  textureDesc.numMipLevels = expectedMipLevels;
  textureDesc.mipmapGeneration = TextureDesc::TextureMipmapGeneration::Manual;

  auto texture = iglDev_->createTexture(textureDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);

  auto* vulkanTexture = static_cast<vulkan::Texture*>(texture.get());
  ASSERT_NE(vulkanTexture, nullptr);

  // Verify the texture was created with Manual mipmap generation
  ASSERT_EQ(vulkanTexture->getMipmapGeneration(), TextureDesc::TextureMipmapGeneration::Manual);

  const TextureRangeDesc range = TextureRangeDesc::new2D(0, 0, textureSize, textureSize);
  ret = texture->upload(range, kBaseMipData.data());
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  // With Manual mode, we should be able to call generateMipmap explicitly
  CommandQueueDesc cmdQueueDesc{};
  auto cmdQueue = iglDev_->createCommandQueue(cmdQueueDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  // Validate that mip level 0 contains the uploaded data
  util::validateUploadedTextureRange(*iglDev_,
                                     *cmdQueue,
                                     texture,
                                     texture->getFullRange(0),
                                     kBaseMipData.data(),
                                     "AutoGen: Base level (0)");

  // This should work with Manual mode
  const auto fullRange = texture->getFullRange(0, 2);
  vulkanTexture->generateMipmap(*cmdQueue, &fullRange);

  // Validate that mip level 1 was auto-generated with expected content
  // The auto-generated mip should contain the same solid color (averaged from base level)
  util::validateUploadedTextureRange(*iglDev_,
                                     *cmdQueue,
                                     texture,
                                     texture->getFullRange(1),
                                     kExpectedMip1Data.data(),
                                     "AutoGen: Generated level (1)");
}

} // namespace igl::tests
