/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanImage.h>

#include <array>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/gtc/color_space.hpp>
#include <memory>
#include <igl/Common.h>
#include <igl/Texture.h>
#include <igl/tests/util/device/TestDevice.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImmediateCommands.h>
#include <igl/vulkan/VulkanStagingDevice.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX

namespace igl::tests {

namespace {
constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 1024;
constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_UNORM;

constexpr uint32_t kBytesPerTexel = 4;
constexpr uint8_t kOpaque = 255;

constexpr uint32_t kCheckerSize = 8; // 8x8 -> 4x4 -> 2x2 -> 1x1
constexpr uint32_t kCheckerMipLevels = 4;

// The mean linear light of the checkerboard source: half its texels sit at 0.0, half at 1.0.
// Downsampling is an averaging filter, so every generated level must carry this same mean.
constexpr float kSourceMeanLight = 0.5f;

uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
}

int channel(uint32_t texel, uint32_t index) {
  return static_cast<int>((texel >> (index * 8)) & 0xFFu);
}

// Encoded byte -> the light it actually represents.
float toLinear(int encoded) {
  return glm::convertSRGBToLinear(glm::vec3(static_cast<float>(encoded) / 255.0f)).r;
}

void expectGreyAndOpaque(uint32_t texel) {
  EXPECT_NEAR(channel(texel, 1), channel(texel, 0), 1) << "green must match red";
  EXPECT_NEAR(channel(texel, 2), channel(texel, 0), 1) << "blue must match red";
  // The sRGB transfer function applies to RGB only, so alpha must survive untouched.
  EXPECT_NEAR(channel(texel, 3), kOpaque, 1) << "alpha must stay opaque";
}

// Alternating black and white texels. Every 2x2 block holds two of each, so mip 1 and every level
// below it is a uniform grey -- which makes the top-left texel representative of the whole level.
std::array<uint32_t, kCheckerSize * kCheckerSize> checkerboardMip0() {
  std::array<uint32_t, kCheckerSize * kCheckerSize> mip0{};
  for (uint32_t y = 0; y < kCheckerSize; ++y) {
    for (uint32_t x = 0; x < kCheckerSize; ++x) {
      const uint8_t value = ((x + y) % 2 == 0) ? 0 : 255;
      mip0[(y * kCheckerSize) + x] = packRGBA(value, value, value, kOpaque);
    }
  }
  return mip0;
}

// Puts the size of the error in the test log, so "how far off is it" is a measured number rather
// than something the reader has to infer from the expectation.
void logRetainedLight(uint32_t level, int encoded) {
  const float light = toLinear(encoded);
  GTEST_LOG_(INFO) << "mip " << level << ": encoded " << encoded << ", linear light " << light
                   << " = " << (100.0f * light / kSourceMeanLight) << "% of the source's "
                   << kSourceMeanLight;
}
} // namespace

//
// VulkanImageTest
//
// Unit tests for igl::vulkan::VulkanImage.
//
class VulkanImageTest : public ::testing::Test {
 public:
  // Set up common resources.
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    device_ = igl::tests::util::device::createTestDevice(igl::BackendType::Vulkan);
    ASSERT_TRUE(device_ != nullptr);
    auto& device = static_cast<igl::vulkan::Device&>(*device_);
    context_ = &device.getVulkanContext();
    ASSERT_TRUE(context_ != nullptr);
    ASSERT_TRUE(context_->stagingDevice_ != nullptr);
    ASSERT_TRUE(context_->immediate_ != nullptr);
  }

 protected:
  [[nodiscard]] vulkan::VulkanImage makeImage(
      VkFormat format,
      VkImageCreateFlags createFlags,
      const char* debugName,
      bool isSrgbMutableFormat = false,
      uint32_t size = 2,
      uint32_t mipLevels = 1,
      uint32_t arrayLayers = 1,
      VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT) const {
    return vulkan::VulkanImage(*context_,
                               VkExtent3D{.width = size, .height = size, .depth = 1},
                               VK_IMAGE_TYPE_2D,
                               format,
                               mipLevels,
                               arrayLayers,
                               VK_IMAGE_TILING_OPTIMAL,
                               usageFlags,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               createFlags,
                               VK_SAMPLE_COUNT_1_BIT,
                               debugName,
                               isSrgbMutableFormat);
  }

  [[nodiscard]] static TextureFormatProperties properties() {
    return TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
  }

  [[nodiscard]] bool supportsLinearBlitDownscale() const {
    VkFormatProperties formatProperties{};
    context_->vf_.vkGetPhysicalDeviceFormatProperties(
        context_->getVkPhysicalDevice(), VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);
    constexpr VkFormatFeatureFlags kRequiredFeatures =
        VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    return (formatProperties.optimalTilingFeatures & kRequiredFeatures) == kRequiredFeatures;
  }

  void uploadMip0(vulkan::VulkanImage& image,
                  TextureType type,
                  const TextureRangeDesc& range,
                  uint32_t size,
                  const void* data) const {
    context_->stagingDevice_->imageData(
        image, type, range, properties(), size * kBytesPerTexel, VK_IMAGE_ASPECT_COLOR_BIT, data);
  }

  void generateMips(vulkan::VulkanImage& image, const TextureRangeDesc& range) const {
    auto& ctx = *context_;
    const auto& wrapper = ctx.immediate_->acquire();
    image.generateMipmap(wrapper.cmdBuf, range);
    ctx.immediate_->wait(ctx.immediate_->submit(wrapper), ctx.config_.fenceTimeoutNanoseconds);
  }

  // Reads the top-left texel of `level` / `layer`.
  [[nodiscard]] uint32_t readTexel(const vulkan::VulkanImage& image,
                                   uint32_t level,
                                   uint32_t layer) const {
    uint32_t texel = 0;
    context_->stagingDevice_->getImageData2D(image.getVkImage(),
                                             level,
                                             layer,
                                             VkRect2D{.offset = {0, 0}, .extent = {1, 1}},
                                             properties(),
                                             VK_FORMAT_R8G8B8A8_UNORM,
                                             image.imageLayout_,
                                             VK_IMAGE_ASPECT_COLOR_BIT,
                                             &texel,
                                             kBytesPerTexel,
                                             false /* flipImageVertical */);
    return texel;
  }

  // Uploads a black-and-white checkerboard, generates the full mip chain and returns the grey each
  // generated level settled on. `createFlags` selects the path under test.
  void checkerboardMipChainGreys(VkImageCreateFlags createFlags,
                                 std::array<int, kCheckerMipLevels>& outGreys) {
    const std::array<uint32_t, kCheckerSize * kCheckerSize> mip0 = checkerboardMip0();

    vulkan::VulkanImage image =
        makeImage(VK_FORMAT_R8G8B8A8_UNORM,
                  createFlags,
                  "Image: sRGB checkerboard mipmap",
                  (createFlags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0,
                  kCheckerSize,
                  kCheckerMipLevels,
                  1 /* arrayLayers */,
                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    ASSERT_TRUE(image.valid());
    ASSERT_EQ(image.isSrgbMutableFormat_, (createFlags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0);

    uploadMip0(image,
               TextureType::TwoD,
               TextureRangeDesc::new2D(0, 0, kCheckerSize, kCheckerSize),
               kCheckerSize,
               mip0.data());
    generateMips(image,
                 TextureRangeDesc::new2D(0, 0, kCheckerSize, kCheckerSize, 0, kCheckerMipLevels));

    for (uint32_t level = 1; level < kCheckerMipLevels; ++level) {
      const uint32_t texel = readTexel(image, level, 0 /* layer */);
      SCOPED_TRACE(testing::Message() << "mip level " << level);
      // The generated levels are uniform grey, so one channel characterizes the level.
      expectGreyAndOpaque(texel);
      outGreys[level] = channel(texel, 0);
      logRetainedLight(level, outGreys[level]);
    }
  }

  std::shared_ptr<IDevice> device_;
  vulkan::VulkanContext* context_ = nullptr;
};

TEST_F(VulkanImageTest, DefaultConstructedIsInvalid) {
  const vulkan::VulkanImage image;

  EXPECT_FALSE(image.valid());
  EXPECT_EQ(image.getVkImage(), VK_NULL_HANDLE);
  EXPECT_EQ(image.getVkImageUsageFlags(), 0u);
  EXPECT_FALSE(image.isSampledImage());
  EXPECT_FALSE(image.isStorageImage());
}

TEST_F(VulkanImageTest, CreateBasicImage) {
  vulkan::VulkanImage image(*context_,
                            VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                            VK_IMAGE_TYPE_2D,
                            kFormat,
                            1, /* mipLevels */
                            1, /* arrayLayers */
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, /* memFlags */
                            0, /* createFlags */
                            VK_SAMPLE_COUNT_1_BIT,
                            "Test Basic Image");

  EXPECT_TRUE(image.valid());
  EXPECT_NE(image.getVkImage(), VK_NULL_HANDLE);
  EXPECT_EQ(image.extent_.width, kWidth);
  EXPECT_EQ(image.extent_.height, kHeight);
  EXPECT_EQ(image.extent_.depth, 1u);
  EXPECT_EQ(image.imageFormat_, kFormat);
  EXPECT_EQ(image.mipLevels_, 1u);
  EXPECT_EQ(image.arrayLayers_, 1u);
  EXPECT_EQ(image.samples_, VK_SAMPLE_COUNT_1_BIT);
  EXPECT_FALSE(image.isExternallyManaged_);
  EXPECT_FALSE(image.isImported_);
  EXPECT_FALSE(image.isExported_);
}

TEST_F(VulkanImageTest, UsageFlagQueries) {
  const vulkan::VulkanImage sampledImage(*context_,
                                         VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                                         VK_IMAGE_TYPE_2D,
                                         kFormat,
                                         1, /* mipLevels */
                                         1, /* arrayLayers */
                                         VK_IMAGE_TILING_OPTIMAL,
                                         VK_IMAGE_USAGE_SAMPLED_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, /* memFlags */
                                         0, /* createFlags */
                                         VK_SAMPLE_COUNT_1_BIT,
                                         "Sampled Image");

  EXPECT_TRUE(sampledImage.isSampledImage());
  EXPECT_FALSE(sampledImage.isStorageImage());
  EXPECT_EQ(sampledImage.getVkImageUsageFlags() & VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_USAGE_SAMPLED_BIT);

  const vulkan::VulkanImage storageImage(*context_,
                                         VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                                         VK_IMAGE_TYPE_2D,
                                         kFormat,
                                         1, /* mipLevels */
                                         1, /* arrayLayers */
                                         VK_IMAGE_TILING_OPTIMAL,
                                         VK_IMAGE_USAGE_STORAGE_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, /* memFlags */
                                         0, /* createFlags */
                                         VK_SAMPLE_COUNT_1_BIT,
                                         "Storage Image");

  EXPECT_FALSE(storageImage.isSampledImage());
  EXPECT_TRUE(storageImage.isStorageImage());
  EXPECT_EQ(storageImage.getVkImageUsageFlags() & VK_IMAGE_USAGE_STORAGE_BIT,
            VK_IMAGE_USAGE_STORAGE_BIT);
}

TEST_F(VulkanImageTest, CreateImageWithExportedMemory) {
  auto vulkanImage = igl::vulkan::VulkanImage::createWithExportMemory(
      *context_,
      VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
      VK_IMAGE_TYPE_2D,
      kFormat,
      1, /* mipLevels */
      1, /* arrayLayers */
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      0, /* createFlags */
      VK_SAMPLE_COUNT_1_BIT,
      "Image: vulkan export memory");
  ASSERT_NE(vulkanImage.valid(), false);
  EXPECT_TRUE(vulkanImage.isExported_);
  // The exported memory's size must be populated for consumers (e.g.
  // RemoteVulkanIglSwapchain) that forward it to importers via IPC.
  EXPECT_GT(vulkanImage.allocatedSize, 0u);
#if IGL_PLATFORM_WINDOWS
  EXPECT_NE(vulkanImage.exportedMemoryHandle_, nullptr);
  EXPECT_NE(vulkanImage.getVkImage(), static_cast<VkImage_T*>(VK_NULL_HANDLE));
#elif IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
  EXPECT_NE(vulkanImage.exportedFd_, -1);
  EXPECT_NE(vulkanImage.getVkImage(), VK_NULL_HANDLE);
#endif
}

#if IGL_PLATFORM_WINDOWS
TEST_F(VulkanImageTest, CreateImageWithImportedMemoryWin32) {
  auto exportedImage = igl::vulkan::VulkanImage::createWithExportMemory(
      *context_,
      VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
      VK_IMAGE_TYPE_2D,
      kFormat,
      1, /* mipLevels */
      1, /* arrayLayers */
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      0, /* createFlags */
      VK_SAMPLE_COUNT_1_BIT,
      "Image: vulkan export memory");
  ASSERT_NE(exportedImage.valid(), false);
  EXPECT_NE(exportedImage.exportedMemoryHandle_, nullptr);

  auto importedImage =
      igl::vulkan::VulkanImage(*context_,
                               exportedImage.exportedMemoryHandle_,
                               VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                               VK_IMAGE_TYPE_2D,
                               kFormat,
                               1, /* mipLevels */
                               1, /* arrayLayers */
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                               0, /* createFlags */
                               VK_SAMPLE_COUNT_1_BIT,
                               "Image: vulkan import memory");
  EXPECT_TRUE(importedImage.isImported_);
  EXPECT_NE(importedImage.getVkImage(), static_cast<VkImage_T*>(VK_NULL_HANDLE));
}
#endif // IGL_PLATFORM_WINDOWS

TEST_F(VulkanImageTest, CreateInfoDefaultValues) {
  const vulkan::VulkanImageCreateInfo ci;

  EXPECT_EQ(ci.usageFlags, 0u);
  EXPECT_TRUE(ci.isExternallyManaged);
  EXPECT_EQ(ci.extent.width, 0u);
  EXPECT_EQ(ci.extent.height, 0u);
  EXPECT_EQ(ci.extent.depth, 0u);
  EXPECT_EQ(ci.type, VK_IMAGE_TYPE_MAX_ENUM);
  EXPECT_EQ(ci.imageFormat, VK_FORMAT_UNDEFINED);
  EXPECT_EQ(ci.mipLevels, 1u);
  EXPECT_EQ(ci.arrayLayers, 1u);
  EXPECT_EQ(ci.samples, VK_SAMPLE_COUNT_1_BIT);
  EXPECT_FALSE(ci.isImported);
  EXPECT_FALSE(ci.isSrgbMutableFormat);
}

TEST_F(VulkanImageTest, GetImageAspectFlagsColor) {
  vulkan::VulkanImage image(*context_,
                            VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                            VK_IMAGE_TYPE_2D,
                            kFormat,
                            1,
                            1,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            0,
                            VK_SAMPLE_COUNT_1_BIT,
                            "Color Aspect Image");
  ASSERT_TRUE(image.valid());

  EXPECT_EQ(image.getImageAspectFlags(), VK_IMAGE_ASPECT_COLOR_BIT);
  EXPECT_FALSE(image.isDepthFormat_);
  EXPECT_FALSE(image.isStencilFormat_);
  EXPECT_FALSE(image.isDepthOrStencilFormat_);
}

TEST_F(VulkanImageTest, MoveConstruction) {
  vulkan::VulkanImage source(*context_,
                             VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                             VK_IMAGE_TYPE_2D,
                             kFormat,
                             1,
                             1,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             0,
                             VK_SAMPLE_COUNT_1_BIT,
                             "Move Source Image");
  ASSERT_TRUE(source.valid());
  const VkImage sourceHandle = source.getVkImage();

  vulkan::VulkanImage dest(std::move(source));

  EXPECT_TRUE(dest.valid());
  EXPECT_EQ(dest.getVkImage(), sourceHandle);
  EXPECT_EQ(dest.extent_.width, kWidth);
  EXPECT_EQ(dest.extent_.height, kHeight);
  EXPECT_EQ(dest.imageFormat_, kFormat);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_FALSE(source.valid());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(source.getVkImage(), VK_NULL_HANDLE);
}

TEST_F(VulkanImageTest, MoveAssignment) {
  vulkan::VulkanImage source(*context_,
                             VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                             VK_IMAGE_TYPE_2D,
                             kFormat,
                             1,
                             1,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             0,
                             VK_SAMPLE_COUNT_1_BIT,
                             "Move Assign Source");
  ASSERT_TRUE(source.valid());
  const VkImage sourceHandle = source.getVkImage();

  vulkan::VulkanImage dest;
  ASSERT_FALSE(dest.valid());

  dest = std::move(source);

  EXPECT_TRUE(dest.valid());
  EXPECT_EQ(dest.getVkImage(), sourceHandle);
  EXPECT_EQ(dest.extent_.width, kWidth);
  EXPECT_EQ(dest.extent_.height, kHeight);
  EXPECT_EQ(dest.imageFormat_, kFormat);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_FALSE(source.valid());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(source.getVkImage(), VK_NULL_HANDLE);
}

TEST_F(VulkanImageTest, CreateImageViewBasic) {
  vulkan::VulkanImage image(*context_,
                            VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                            VK_IMAGE_TYPE_2D,
                            kFormat,
                            1,
                            1,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            0,
                            VK_SAMPLE_COUNT_1_BIT,
                            "ImageView Source");
  ASSERT_TRUE(image.valid());

  auto imageView = image.createImageView(
      VK_IMAGE_VIEW_TYPE_2D, kFormat, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, "Test ImageView");

  EXPECT_TRUE(imageView.valid());
  EXPECT_NE(imageView.getVkImageView(), VK_NULL_HANDLE);
  EXPECT_EQ(imageView.getVkImageAspectFlags(), VK_IMAGE_ASPECT_COLOR_BIT);
}

TEST_F(VulkanImageTest, IsStorageImage) {
  auto& ctx = *context_;

  const VkImageUsageFlags storageUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  igl::vulkan::VulkanImage image(ctx,
                                 VkExtent3D{.width = 4, .height = 4, .depth = 1},
                                 VK_IMAGE_TYPE_2D,
                                 kFormat,
                                 1,
                                 1,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 storageUsage,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 0,
                                 VK_SAMPLE_COUNT_1_BIT,
                                 "Storage Image");
  ASSERT_TRUE(image.valid());

  EXPECT_TRUE(image.isStorageImage());
  EXPECT_TRUE(image.isSampledImage());
}

TEST_F(VulkanImageTest, CreateImageMultipleMipLevels) {
  auto& ctx = *context_;

  const uint32_t kMipLevels = 4;

  igl::vulkan::VulkanImage image(ctx,
                                 VkExtent3D{.width = 16, .height = 16, .depth = 1},
                                 VK_IMAGE_TYPE_2D,
                                 kFormat,
                                 kMipLevels,
                                 1,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 0,
                                 VK_SAMPLE_COUNT_1_BIT,
                                 "Multi Mip Image");
  ASSERT_TRUE(image.valid());
  EXPECT_NE(image.getVkImage(), VK_NULL_HANDLE);
  EXPECT_EQ(image.mipLevels_, kMipLevels);
}

TEST_F(VulkanImageTest, CreateImageViewWithMipLevelSubset) {
  auto& ctx = *context_;

  const uint32_t kMipLevels = 4;

  igl::vulkan::VulkanImage image(ctx,
                                 VkExtent3D{.width = 16, .height = 16, .depth = 1},
                                 VK_IMAGE_TYPE_2D,
                                 kFormat,
                                 kMipLevels,
                                 1,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 0,
                                 VK_SAMPLE_COUNT_1_BIT,
                                 "Mip Subset Source");
  ASSERT_TRUE(image.valid());
  EXPECT_EQ(image.mipLevels_, kMipLevels);

  auto viewAllMips = image.createImageView(VK_IMAGE_VIEW_TYPE_2D,
                                           kFormat,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           0,
                                           kMipLevels,
                                           0,
                                           1,
                                           "View All Mips");
  EXPECT_TRUE(viewAllMips.valid());
  EXPECT_NE(viewAllMips.getVkImageView(), VK_NULL_HANDLE);
  EXPECT_EQ(viewAllMips.getVkImageAspectFlags(), VK_IMAGE_ASPECT_COLOR_BIT);

  auto viewSingleMip = image.createImageView(
      VK_IMAGE_VIEW_TYPE_2D, kFormat, VK_IMAGE_ASPECT_COLOR_BIT, 2, 1, 0, 1, "View Mip 2");
  EXPECT_TRUE(viewSingleMip.valid());
  EXPECT_NE(viewSingleMip.getVkImageView(), VK_NULL_HANDLE);
  EXPECT_EQ(viewSingleMip.getVkImageAspectFlags(), VK_IMAGE_ASPECT_COLOR_BIT);
}

//
// SrgbMutableRequiresMutableBitAndSrgbCounterpart
//
// isSrgbMutableFormat_ is set when a caller explicitly requests sRGB mutable handling for a UNORM
// VkImage with VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT and an sRGB counterpart, in which case creation
// chains a VkImageFormatListCreateInfo constraining views to exactly {UNORM, sRGB}. The flag must
// be set only when all conditions hold; every image the texture layer creates today misses at least
// one, which is what keeps this change inert.
//
TEST_F(VulkanImageTest, SrgbMutableRequiresMutableBitAndSrgbCounterpart) {
  const vulkan::VulkanImage plain =
      makeImage(VK_FORMAT_R8G8B8A8_UNORM, 0, "Image: sRGB mutable flag test (plain)", true);
  ASSERT_TRUE(plain.valid());
  EXPECT_FALSE(plain.isSrgbMutableFormat_);

  const vulkan::VulkanImage mutablePaired = makeImage(VK_FORMAT_R8G8B8A8_UNORM,
                                                      VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
                                                      "Image: sRGB mutable flag test (paired)",
                                                      true);
  ASSERT_TRUE(mutablePaired.valid());
  EXPECT_TRUE(mutablePaired.isSrgbMutableFormat_);

  // No sRGB counterpart exists for a float format, so there is no second view format to constrain.
  const vulkan::VulkanImage mutableUnpaired = makeImage(VK_FORMAT_R16G16B16A16_SFLOAT,
                                                        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
                                                        "Image: sRGB mutable flag test (unpaired)",
                                                        true);
  ASSERT_TRUE(mutableUnpaired.valid());
  EXPECT_FALSE(mutableUnpaired.isSrgbMutableFormat_);
}

//
// SrgbMutableSurvivesMove
//
// VulkanImage's move assignment is hand-written member-by-member (and the move constructor
// delegates to it), so a newly added field is silently dropped unless it is listed there.
// Texture::create() reaches every VkImage through exactly that path -- `VulkanImage image;` then
// `image = ctx.createImage(...)` -- so a lost flag here means every consumer of it silently sees
// a plain UNORM image.
//
TEST_F(VulkanImageTest, SrgbMutableSurvivesMove) {
  const auto makeMutableImage = [this]() {
    return makeImage(VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
                     "Image: sRGB mutable move test",
                     true);
  };

  // Move assignment onto a default-constructed image: what Texture::create() does.
  vulkan::VulkanImage assigned;
  assigned = makeMutableImage();
  ASSERT_TRUE(assigned.valid());
  EXPECT_TRUE(assigned.isSrgbMutableFormat_);

  // Named source, so this is a real move construction and not an elided prvalue init.
  vulkan::VulkanImage source = makeMutableImage();
  const vulkan::VulkanImage constructed(std::move(source));
  ASSERT_TRUE(constructed.valid());
  EXPECT_TRUE(constructed.isSrgbMutableFormat_);
}

//
// CheckerboardMipChainLosesLightOnUnormBackedSrgb
//
// Mipmap generation for a VkImage that is UNORM but whose contents are sRGB-encoded
// (isSrgbMutableFormat_). This drives VulkanImage directly rather than igl::vulkan::Texture:
// nothing in the IGL texture layer creates a UNORM-base sRGB image yet, so the only way to reach
// the path at this point in the stack is to construct the VkImage with the same flags that layer
// will later use.
//
// This is not a defect in anything shipping today. sRGB textures are currently backed by a
// natively sRGB VkImage, and vkCmdBlitImage() decodes, filters and re-encodes those correctly --
// mip generation is already gamma-correct. What this pins is the hazard that arrives with the
// other backing: storage-capable sRGB textures must move onto a UNORM VkImage, because Vulkan
// forbids STORAGE usage on sRGB formats. isSrgbMutableFormat_ is the flag that marks that shape, so
// the failure mode is pinned here beside it before the texture layer creates one.
//
// The probe is the canonical gamma test. A black-and-white checkerboard averages to mid grey, and
// averaging is all downsampling does, so the source's mean light of 0.5 must reach every level;
// sRGB encodes that grey as 188. On a UNORM image the blit filters in the image's creation-format
// numeric space, averaging the *encoded* bytes of black and white as though they were light and
// landing on 128 -- a byte carrying 0.216 of the light, 43% of the source, lost once at mip 1 and
// inherited all the way down.
//
// THE EXPECTATION BELOW IS THAT WRONG RESULT, pinned so the hazard is executable and its size
// measured rather than argued in prose. The follow-up sRGB mipmap path should flip this test to
// expect kSourceMeanLight.
//
TEST_F(VulkanImageTest, CheckerboardMipChainLosesLightOnUnormBackedSrgb) {
  if (!supportsLinearBlitDownscale()) {
    GTEST_SKIP() << "RGBA8 linear blit downscaling is unavailable on this Vulkan device";
  }

  std::array<int, kCheckerMipLevels> greys{};
  ASSERT_NO_FATAL_FAILURE(checkerboardMipChainGreys(VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, greys));

  // Consistency: downsampling an already-uniform level is the identity, so whatever grey mip 1
  // settles on must hold all the way down. This holds on both sides of the fix: the light is lost
  // once, at mip 1.
  for (uint32_t level = 2; level < kCheckerMipLevels; ++level) {
    EXPECT_NEAR(greys[level], greys[1], 1) << "mip " << level << " drifted from mip 1";
  }

  EXPECT_LT(toLinear(greys[1]), kSourceMeanLight)
      << "encoded-space blit should retain less light than the source average";
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
