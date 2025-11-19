/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/Texture.h>

#include "../util/TestDevice.h"

#include <igl/vulkan/Device.h>

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

} // namespace igl::tests
