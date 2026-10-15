/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <memory>
#include <igl/tests/util/device/webgpu/TestDevice.h>
#include <igl/webgpu/PlatformDevice.h>
#include <igl/webgpu/Texture.h>

namespace igl::tests {

class WebGPUMemorylessTextureTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
    platformDevice_ = &device_->getPlatformDevice();
  }

  void TearDown() override {
    EXPECT_EQ(device_->getContext().getUncapturedErrorCount(), 0u);
  }

 protected:
  [[nodiscard]] std::shared_ptr<webgpu::Texture> createTexture(const TextureDesc& desc,
                                                               Result* outResult = nullptr) {
    Result ret;
    auto texture = device_->createTexture(desc, &ret);
    if (outResult != nullptr) {
      *outResult = ret;
    } else {
      EXPECT_TRUE(ret.isOk()) << ret.message;
    }
    return std::static_pointer_cast<webgpu::Texture>(texture);
  }

  std::unique_ptr<webgpu::Device> device_;
  const webgpu::PlatformDevice* platformDevice_ = nullptr;
};

TEST_F(WebGPUMemorylessTextureTest, CreateMemorylessAttachmentTexture) {
  // Create a 4x4 RGBA_UNorm8 memoryless attachment texture
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };

  auto texture = createTexture(desc);
  ASSERT_NE(texture, nullptr);
  EXPECT_NE(texture->getNativeImage(), nullptr);
}

TEST_F(WebGPUMemorylessTextureTest, MemorylessTextureUsageFlags) {
  // When the device has transient attachment support, the memoryless texture uses the feature
  if (!platformDevice_->hasWGPUFeature(WGPUFeatureName_TransientAttachments)) {
    GTEST_SKIP() << "Device does not support WGPUFeatureName_TransientAttachments";
  }

  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };

  auto texture = createTexture(desc);
  ASSERT_NE(texture, nullptr);

  auto* const wgpuTexture = static_cast<WGPUTexture>(texture->getNativeImage());
  ASSERT_NE(wgpuTexture, nullptr);
  // Dawn allows no other usage on a transient attachment
  EXPECT_EQ(wgpuTextureGetUsage(wgpuTexture),
            WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TransientAttachment);
}

TEST_F(WebGPUMemorylessTextureTest, MemorylessTextureGetBytesUnsupported) {
  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };

  auto texture = createTexture(desc);
  ASSERT_NE(texture, nullptr);

  // getBytes() should return Unsupported for memoryless textures
  std::vector<uint8_t> buffer(16);
  const TextureRangeDesc range = TextureRangeDesc::new2D(0, 0, 4, 4);
  const Result result = texture->getBytes(range, WGPUTextureAspect_All, buffer.data(), 0, false);
  EXPECT_EQ(result.code, Result::Code::Unsupported);
}

TEST_F(WebGPUMemorylessTextureTest, MemorylessWithOptionalFeaturesDisabled) {
  // When optional features are disabled, memoryless texture creation should still succeed
  // (it falls back to non-memoryless storage)
  auto device = util::device::webgpu::createTestDevice({.requestOptionalFeatures = false});
  ASSERT_NE(device, nullptr);

  const TextureDesc desc{
      .width = 4,
      .height = 4,
      .usage = TextureDesc::TextureUsageBits::Attachment,
      .type = TextureType::TwoD,
      .format = TextureFormat::RGBA_UNorm8,
      .storage = ResourceStorage::Memoryless,
  };

  Result ret;
  auto texture = device->createTexture(desc, &ret);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(texture, nullptr);
  // Without the optional features, the texture falls back to a regular attachment
  auto* const wgpuTexture = static_cast<WGPUTexture>(
      std::static_pointer_cast<webgpu::Texture>(texture)->getNativeImage());
  ASSERT_NE(wgpuTexture, nullptr);
  EXPECT_EQ(wgpuTextureGetUsage(wgpuTexture) & WGPUTextureUsage_TransientAttachment,
            WGPUTextureUsage_None);
}

} // namespace igl::tests
