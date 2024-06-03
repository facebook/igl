/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/texture_loader/ktx2/Header.h>
#include <IGLU/texture_loader/ktx2/TextureLoaderFactory.h>
#include <cstring>
#include <igl/vulkan/util/TextureFormat.h>
#include <numeric>
#include <vector>

namespace igl::tests::ktx2 {

namespace {

template<typename T>
T align(T offset, T alignment) {
  return (offset + (alignment - 1)) & ~(alignment - 1);
}

std::vector<uint8_t> getBuffer(uint32_t capacity) {
  std::vector<uint8_t> buffer(static_cast<size_t>(capacity));
  return buffer;
}

template<typename T>
void put(std::vector<uint8_t>& buffer, uint32_t offset, T data) {
  if (buffer.size() < static_cast<size_t>(offset) + sizeof(T)) {
    throw std::runtime_error("Overflow when storing a word");
  }
  std::memcpy(buffer.data() + offset, &data, sizeof(T));
}

constexpr uint32_t kHeaderSize = 80u;
constexpr uint32_t kOffsetVkFormat = 12u;
constexpr uint32_t kOffsetTypeSize = 16u;
constexpr uint32_t kOffsetWidth = 20u;
constexpr uint32_t kOffsetHeight = 24u;
constexpr uint32_t kOffsetFaceCount = 36u;
constexpr uint32_t kOffsetLevelCount = 40u;
constexpr uint32_t kOffsetDfdByteOffset = 48u;
constexpr uint32_t kOffsetDfdByteLength = 52u;
constexpr uint32_t kOffsetKvdByteOffset = 56u;
constexpr uint32_t kOffsetKvdByteLength = 60u;

constexpr uint32_t kMipmapMetadataSize = 24u;
constexpr uint32_t kDfdMetadataSize = 48u;

uint32_t getTotalHeaderSize(uint32_t numMipLevels, uint32_t bytesOfKeyValueData) {
  return kHeaderSize + numMipLevels * kMipmapMetadataSize + bytesOfKeyValueData + kDfdMetadataSize;
}

uint32_t getTotalDataSize(uint32_t vkFormat,
                          uint32_t width,
                          uint32_t height,
                          uint32_t numMipLevels) {
  const auto format =
      igl::vulkan::util::vkTextureFormatToTextureFormat(static_cast<int32_t>(vkFormat));
  const auto properties = igl::TextureFormatProperties::fromTextureFormat(format);

  const auto range = igl::TextureRangeDesc::new2D(0, 0, std::max(width, 1u), std::max(height, 1u));

  const uint32_t mipLevelAlignment = std::lcm(static_cast<uint32_t>(properties.bytesPerBlock), 4u);

  uint32_t dataSize = 0;
  for (uint32_t i = 0; i < numMipLevels; ++i) {
    const uint32_t mipLevel = numMipLevels - i - 1;
    const uint32_t rangeBytes =
        static_cast<uint32_t>(properties.getBytesPerRange(range.atMipLevel(mipLevel)));

    dataSize = align(dataSize + rangeBytes, mipLevelAlignment);
  }

  return dataSize;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void putDfd(std::vector<uint8_t>& buffer, uint32_t vkFormat, uint32_t numMipLevels) {
  const uint32_t dfdMetadataffset = kHeaderSize + numMipLevels * kMipmapMetadataSize;

  constexpr uint32_t VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG = 1000054004u;
  constexpr uint32_t VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK = 147u;

  ASSERT_TRUE(vkFormat == VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG ||
              vkFormat == VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);

  const auto format =
      igl::vulkan::util::vkTextureFormatToTextureFormat(static_cast<int32_t>(vkFormat));
  const auto properties = igl::TextureFormatProperties::fromTextureFormat(format);

  const uint16_t descriptorType = 0;
  const uint16_t vendorId = 0;
  const uint16_t descriptorBlockSize = 40;
  const uint16_t version = 2;
  const uint8_t flags = 0 /*KHR_DF_FLAG_ALPHA_STRAIGHT*/;
  const uint8_t transferFunction =
      properties.isSRGB() ? 1 /*KHR_DF_TRANSFER_SRGB*/ : 2 /*KHR_DF_TRANSFER_LINEAR*/;
  const uint8_t colorPrimaries = 1 /*KHR_DF_PRIMARIES_BT709*/;
  const uint8_t colorModel = vkFormat == VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG
                                 ? 164 /*KHR_DF_MODEL_PVRTC*/
                                 : 160 /*KHR_DF_MODEL_ETC1*/;
  const uint8_t texelBlockDimension3 = 0;
  const uint8_t texelBlockDimension2 = properties.blockDepth - 1;
  const uint8_t texelBlockDimension1 = properties.blockHeight - 1;
  const uint8_t texelBlockDimension0 = properties.blockWidth - 1;
  const uint32_t bytesPlane3210 = 8u;
  const uint32_t bytesPlane7654 = 0;

  const uint8_t channelFlags = 0;
  const uint8_t bitLength = 63;
  const uint16_t bitOffset = 0;
  const uint8_t samplePosition3 = 0;
  const uint8_t samplePosition2 = 0;
  const uint8_t samplePosition1 = 0;
  const uint8_t samplePosition0 = 0;
  const uint32_t sampleLower = 0;
  const uint32_t sampleUpper = std::numeric_limits<uint32_t>::max();

  put(buffer, kOffsetDfdByteOffset, dfdMetadataffset);
  put(buffer, kOffsetDfdByteLength, kDfdMetadataSize - 4);

  put(buffer, dfdMetadataffset, kDfdMetadataSize - 4); // Total length

  put(buffer, dfdMetadataffset + 4u, vendorId);
  put(buffer, dfdMetadataffset + 6u, descriptorType);
  put(buffer, dfdMetadataffset + 8u, version);
  put(buffer, dfdMetadataffset + 10u, descriptorBlockSize);
  put(buffer, dfdMetadataffset + 12u, colorModel);
  put(buffer, dfdMetadataffset + 13u, colorPrimaries);
  put(buffer, dfdMetadataffset + 14u, transferFunction);
  put(buffer, dfdMetadataffset + 15u, flags);
  put(buffer, dfdMetadataffset + 16u, texelBlockDimension0);
  put(buffer, dfdMetadataffset + 17u, texelBlockDimension1);
  put(buffer, dfdMetadataffset + 18u, texelBlockDimension2);
  put(buffer, dfdMetadataffset + 19u, texelBlockDimension3);
  put(buffer, dfdMetadataffset + 20u, bytesPlane3210);
  put(buffer, dfdMetadataffset + 24u, bytesPlane7654);

  put(buffer, dfdMetadataffset + 28u, bitOffset);
  put(buffer, dfdMetadataffset + 30u, bitLength);
  put(buffer, dfdMetadataffset + 31u, channelFlags);
  put(buffer, dfdMetadataffset + 32u, samplePosition0);
  put(buffer, dfdMetadataffset + 33u, samplePosition1);
  put(buffer, dfdMetadataffset + 34u, samplePosition2);
  put(buffer, dfdMetadataffset + 35u, samplePosition3);
  put(buffer, dfdMetadataffset + 36u, sampleLower);
  put(buffer, dfdMetadataffset + 40u, sampleUpper);
}

void putMipLevel(std::vector<uint8_t>& buffer, uint32_t mipLevel, uint32_t imageSize) {
  const auto* header = reinterpret_cast<const iglu::textureloader::ktx2::Header*>(buffer.data());
  const auto format =
      igl::vulkan::util::vkTextureFormatToTextureFormat(static_cast<int32_t>(header->vkFormat));
  const auto properties = igl::TextureFormatProperties::fromTextureFormat(format);

  const auto range = igl::TextureRangeDesc::new2D(
      0, 0, std::max(header->pixelWidth, 1u), std::max(header->pixelHeight, 1u));

  const uint32_t maxMipLevels = igl::TextureDesc::calcNumMipLevels(range.width, range.height);
  const uint32_t levelCount = std::min(maxMipLevels, header->levelCount);

  const uint32_t mipLevelAlignment = std::lcm(static_cast<uint32_t>(properties.bytesPerBlock), 4u);
  const uint32_t mipmapMetadataLength = levelCount * kMipmapMetadataSize;

  const uint32_t metadataLength = iglu::textureloader::ktx2::kHeaderLength + mipmapMetadataLength +
                                  header->kvdByteLength + kDfdMetadataSize;

  std::vector<uint32_t> mipmapOffsets(levelCount);
  uint32_t mipmapOffset = align(metadataLength, mipLevelAlignment);

  for (size_t i = 0; i < levelCount; ++i) {
    const uint32_t workingLevel = levelCount - i - 1;
    mipmapOffsets[workingLevel] = mipmapOffset;
    if (workingLevel == mipLevel) {
      break;
    }

    const uint32_t rangeBytes =
        static_cast<uint32_t>(properties.getBytesPerRange(range.atMipLevel(workingLevel)));

    mipmapOffset = align(mipmapOffset + rangeBytes, mipLevelAlignment);
  }

  const uint32_t mipmapMetadataOffset = kHeaderSize + mipLevel * kMipmapMetadataSize;
  put(buffer, mipmapMetadataOffset, static_cast<uint64_t>(mipmapOffsets[mipLevel]));
  put(buffer, mipmapMetadataOffset + 8u, static_cast<uint64_t>(imageSize));
  put(buffer, mipmapMetadataOffset + 16u, static_cast<uint64_t>(imageSize));
}

void populateMinimalValidFile(std::vector<uint8_t>& buffer,
                              uint32_t vkFormat,
                              uint32_t width,
                              uint32_t height,
                              uint32_t numMipLevels,
                              uint32_t bytesOfKeyValueData,
                              uint32_t imageSize,
                              bool forceDfdAfterMipLevel1 = false) {
  // HEADER
  // Zero-out the whole buffer, since there might be garbage in it.
  std::memset(buffer.data(), 0x00, buffer.size());

  const uint32_t mipmapMetadataLength = numMipLevels * kMipmapMetadataSize;

  // Put the default values in
  const char fixedTag[] = {'\xAB', 'K', 'T', 'X', ' ', '2', '0', '\xBB', '\r', '\n', '\x1A', '\n'};
  std::memcpy(buffer.data(), &fixedTag, sizeof(fixedTag));
  put(buffer, kOffsetVkFormat, vkFormat);
  put(buffer, kOffsetTypeSize, 1);
  put(buffer, kOffsetFaceCount, 1);

  put(buffer, kOffsetWidth, width);
  put(buffer, kOffsetHeight, height);
  put(buffer, kOffsetLevelCount, numMipLevels);
  put(buffer,
      kOffsetKvdByteOffset,
      bytesOfKeyValueData == 0 ? 0u : kHeaderSize + mipmapMetadataLength);
  put(buffer, kOffsetKvdByteLength, bytesOfKeyValueData);

  putMipLevel(buffer, 0u, imageSize);

  putDfd(buffer, vkFormat, forceDfdAfterMipLevel1 ? 1u : numMipLevels);
}

} // namespace

class Ktx2TextureLoaderTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
  }

  void TearDown() override {}

  iglu::textureloader::ktx2::TextureLoaderFactory factory_;
};

TEST_F(Ktx2TextureLoaderTest, EmptyBuffer_Fails) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 1u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Ktx2TextureLoaderTest, MinimumValidHeader_Succeeds) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 1u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t imageSize = 512u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize);
  populateMinimalValidFile(
      buffer, vkFormat, width, height, numMipLevels, bytesOfKeyValueData, imageSize);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_NE(loader, nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
}

TEST_F(Ktx2TextureLoaderTest, HeaderWithMipLevels_Succeeds) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 5u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t imageSize = 512u; // For first mip level
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize);
  populateMinimalValidFile(
      buffer, vkFormat, width, height, numMipLevels, bytesOfKeyValueData, imageSize);

  // Fill the other mip levels
  putMipLevel(buffer, 1u, 128u);
  putMipLevel(buffer, 2u, 32u);
  putMipLevel(buffer, 3u, 32u);
  putMipLevel(buffer, 4u, 32u);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_NE(loader, nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
}

TEST_F(Ktx2TextureLoaderTest, ValidHeaderWithExtraData_Succeeds) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 1u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t imageSize = 512u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize + 1u);
  populateMinimalValidFile(
      buffer, vkFormat, width, height, numMipLevels, bytesOfKeyValueData, imageSize);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_NE(loader, nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
}

TEST_F(Ktx2TextureLoaderTest, InsufficientData_Fails) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 1u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t imageSize = 512u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize - 1u);
  populateMinimalValidFile(
      buffer, vkFormat, width, height, numMipLevels, bytesOfKeyValueData, imageSize);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Ktx2TextureLoaderTest, InsufficientDataWithMipLevels_Fails) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 6u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t imageSize = 512; // For first mip level
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize - 1u);
  populateMinimalValidFile(
      buffer, vkFormat, width, height, numMipLevels, bytesOfKeyValueData, imageSize);

  // Fill the other mip levels
  putMipLevel(buffer, 1u, 128u);
  putMipLevel(buffer, 2u, 32u);
  putMipLevel(buffer, 3u, 32u);
  putMipLevel(buffer, 4u, 32u);
  putMipLevel(buffer, 5u, 32u);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Ktx2TextureLoaderTest, ValidHeaderWithInvalidImageSize_Fails) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 1u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t imageSize = 4096u;
  const uint32_t vkFormat = 147u; /* VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK */
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize);
  populateMinimalValidFile(
      buffer, vkFormat, width, height, numMipLevels, bytesOfKeyValueData, imageSize);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Ktx2TextureLoaderTest, InvalidHeaderWithExcessiveImageSize_Fails) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 1u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t imageSize = 4294967290u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize);
  populateMinimalValidFile(
      buffer, vkFormat, width, height, numMipLevels, bytesOfKeyValueData, imageSize);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Ktx2TextureLoaderTest, InvalidHeaderWithExcessiveMipLevels_Fails) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 4294967290u;
  const uint32_t bytesOfKeyValueData = 0u;
  const uint32_t imageSize = 512u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t totalHeaderSize = getTotalHeaderSize(6u, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, 6u);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize);
  populateMinimalValidFile(buffer,
                           vkFormat,
                           width,
                           height,
                           numMipLevels,
                           bytesOfKeyValueData,
                           imageSize,
                           true) /* forceDfdAfterMipLevel1*/;

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Ktx2TextureLoaderTest, InvalidHeaderWithExcessiveKeyValueData_Fails) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const uint32_t numMipLevels = 1u;
  const uint32_t bytesOfKeyValueData = 4294967290u;
  const uint32_t imageSize = 512u;
  const uint32_t vkFormat = 1000054004u; /* VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG */
  const uint32_t totalHeaderSize = getTotalHeaderSize(numMipLevels, bytesOfKeyValueData);
  const uint32_t totalDataSize = getTotalDataSize(vkFormat, width, height, numMipLevels);

  auto buffer = getBuffer(totalHeaderSize + totalDataSize);
  populateMinimalValidFile(
      buffer, vkFormat, width, height, numMipLevels, bytesOfKeyValueData, imageSize);

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

} // namespace igl::tests::ktx2
