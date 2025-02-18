/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>

#include "Texture.h"

namespace igl::tests {

//
// Test render to mip
//
// Create a square output texture with a mip chain and render several different colors into each
// mip level. Read back individual mips to make sure they were written to correctly.
//

TEST_F(TextureTest, RenderToMip) {
  Result ret;
  std::shared_ptr<IRenderPipelineState> pipelineState;

  // Use a square output texture with mips
  constexpr uint32_t kNumMipLevels = 4;
  constexpr uint32_t kOutputTexWidth = 8u;
  constexpr uint32_t kOutputTexHeight = 8u;
  static_assert(kOutputTexWidth > 1);
  static_assert(1 << (kNumMipLevels - 1) == kOutputTexWidth);
  static_assert(kOutputTexWidth == kOutputTexHeight);

  static constexpr std::array<uint32_t, kNumMipLevels> kColors = {
      0xdeadbeef, 0x8badf00d, 0xc00010ff, 0xbaaaaaad};

  std::vector<std::vector<uint32_t>> inputTexData{
      std::vector(64 /* 8 * 8 */, kColors[0]),
      std::vector(16 /* 4 * 4 */, kColors[1]),
      std::vector(4 /* 2 * 2 */, kColors[2]),
      std::vector(1 /* 1 * 1 */, kColors[3]),
  };

  //---------------------------------------------------------------------
  // Create output texture with mip levels and attach it to a framebuffer
  //---------------------------------------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kOutputTexWidth,
                                           kOutputTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = kNumMipLevels;

  auto outputTex = iglDev_->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(outputTex != nullptr);

  // Create framebuffer using the output texture
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = outputTex;
  auto fb = iglDev_->createFramebuffer(framebufferDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(fb != nullptr);

  //----------------
  // Create Pipeline
  //----------------
  pipelineState = iglDev_->createRenderPipeline(renderPipelineDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(pipelineState != nullptr);

  //-------------------------
  // Render to each mip level
  //-------------------------
  for (auto mipLevel = 0; mipLevel < kNumMipLevels; mipLevel++) {
    //---------------------
    // Create input texture
    //---------------------
    const int inTexWidth = kOutputTexWidth >> mipLevel;
    texDesc = TextureDesc::new2D(
        TextureFormat::RGBA_UNorm8, inTexWidth, inTexWidth, TextureDesc::TextureUsageBits::Sampled);
    inputTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(inputTexture_ != nullptr);

    // Initialize the input texture's color
    auto rangeDesc = TextureRangeDesc::new2D(0, 0, inTexWidth, inTexWidth);
    inputTexture_->upload(rangeDesc, inputTexData[mipLevel].data());

    cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf_ != nullptr);

    // Modify render pass to only draw to nth mip level
    renderPass_.colorAttachments[0].mipLevel = mipLevel;

    auto cmds = cmdBuf_->createRenderCommandEncoder(renderPass_, fb);
    cmds->bindVertexBuffer(data::shader::simplePosIndex, *vb_);
    cmds->bindVertexBuffer(data::shader::simpleUvIndex, *uv_);

    cmds->bindRenderPipelineState(pipelineState);

    cmds->bindTexture(textureUnit_, BindTarget::kFragment, inputTexture_.get());
    cmds->bindSamplerState(textureUnit_, BindTarget::kFragment, samp_.get());

    cmds->bindIndexBuffer(*ib_, IndexFormat::UInt16);
    cmds->drawIndexed(6);

    cmds->endEncoding();

    cmdQueue_->submit(*cmdBuf_);

    cmdBuf_->waitUntilCompleted();
  }

  // Do readback in a separate loop to ensure all mip levels have been rendered.
  for (size_t mipLevel = 0; mipLevel < kNumMipLevels; mipLevel++) {
    //----------------
    // Validate output
    //----------------
    util::validateFramebufferTextureRange(*iglDev_,
                                          *cmdQueue_,
                                          *fb,
                                          outputTex->getFullRange(mipLevel),
                                          inputTexData[mipLevel].data(),
                                          (std::string("Mip ") + std::to_string(mipLevel)).c_str());
  }
}

namespace {
void testUploadToMip(IDevice& device, ICommandQueue& cmdQueue, bool singleUpload) {
  Result ret;

  // Use a square output texture with mips
  constexpr uint32_t kNumMipLevels = 2;
  constexpr uint32_t kTexWidth = 2u;
  constexpr uint32_t kTexHeight = 2u;
  static_assert(kTexWidth > 1);
  static_assert(1 << (kNumMipLevels - 1) == kTexWidth);
  static_assert(kTexWidth == kTexHeight);

  constexpr uint32_t kBaseMipColor = 0xdeadbeef;
  constexpr uint32_t kMip1Color = 0x8badf00d;

  static constexpr std::array<uint32_t, 5> kMipTextureData = {
      kBaseMipColor, // Base Mip
      kBaseMipColor, // Base Mip
      kBaseMipColor, // Base Mip
      kBaseMipColor, // Base Mip
      kMip1Color, // Mip 1
  };
  static constexpr const uint32_t* kBaseMipData = kMipTextureData.data();
  static constexpr const uint32_t* kMip1Data = kMipTextureData.data() + 4;

  //---------------------------------------------------------------------
  // Create texture with mip levels
  //---------------------------------------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kTexWidth,
                                           kTexHeight,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = kNumMipLevels;
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  ASSERT_TRUE(tex != nullptr);

  //---------------------------------------------------------------------
  // Validate initial state, upload pixel data, and generate mipmaps
  //---------------------------------------------------------------------
  if (singleUpload) {
    ret = tex->upload(tex->getFullRange(0, 2), kMipTextureData.data());
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  } else {
    ret = tex->upload(tex->getFullRange(0), kBaseMipData);
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;

    ret = tex->upload(tex->getFullRange(1), kMip1Data);
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  }

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(0), kBaseMipData, "Base Level");

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(1), kMip1Data, "Mip 1");
}
} // namespace

TEST_F(TextureTest, UploadToMip_LevelByLevel) {
  testUploadToMip(*iglDev_, *cmdQueue_, false);
}

TEST_F(TextureTest, UploadToMip_SingleUpload) {
  testUploadToMip(*iglDev_, *cmdQueue_, true);
}

namespace {
void testGenerateMipmap(IDevice& device, ICommandQueue& cmdQueue, bool withCommandQueue) {
  Result ret;

  // Use a square output texture with mips
  constexpr uint32_t kNumMipLevels = 2;
  constexpr uint32_t kTexWidth = 2u;
  constexpr uint32_t kTexHeight = 2u;
  static_assert(kTexWidth > 1);
  static_assert(1 << (kNumMipLevels - 1) == kTexWidth);
  static_assert(kTexWidth == kTexHeight);

  constexpr uint32_t kColor = 0xdeadbeef;
  constexpr std::array<uint32_t, 4> kBaseMipData = {kColor, kColor, kColor, kColor};
  constexpr std::array<uint32_t, 1> kInitialMip1Data = {0};
  constexpr std::array<uint32_t, 1> kGeneratedMip1Data = {kColor};

  //---------------------------------------------------------------------
  // Create texture with mip levels
  //---------------------------------------------------------------------
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           kTexWidth,
                                           kTexWidth,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  texDesc.numMipLevels = kNumMipLevels;
  auto tex = device.createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
  ASSERT_TRUE(tex != nullptr);

  //---------------------------------------------------------------------
  // Validate initial state, upload pixel data, and generate mipmaps
  //---------------------------------------------------------------------
  ret = tex->upload(tex->getFullRange(0), kBaseMipData.data());
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;

  ret = tex->upload(tex->getFullRange(1), kInitialMip1Data.data());
  ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(0), kBaseMipData.data(), "Initial (level 0)");

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(1), kInitialMip1Data.data(), "Initial (level 1)");

  if (withCommandQueue) {
    tex->generateMipmap(cmdQueue);

    // Dummy command buffer to wait for completion.
    auto cmdBuf = cmdQueue.createCommandBuffer({}, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf != nullptr);
    cmdQueue.submit(*cmdBuf);
    cmdBuf->waitUntilCompleted();
  } else {
    auto cmdBuffer = cmdQueue.createCommandBuffer({}, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
    tex->generateMipmap(*cmdBuffer);
    cmdQueue.submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
  }

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(0), kBaseMipData.data(), "Final (level 0)");

  util::validateUploadedTextureRange(
      device, cmdQueue, tex, tex->getFullRange(1), kGeneratedMip1Data.data(), "Final (level 1)");
}
} // namespace

//
// Test generating mipmaps
//
// Create a texture and upload a solid color into the base mip level, verify the base and 1st mip
// level colors. Then generate mipmaps and verify again.
//
TEST_F(TextureTest, GenerateMipmapWithCommandQueue) {
  testGenerateMipmap(*iglDev_, *cmdQueue_, true);
}

TEST_F(TextureTest, GenerateMipmapWithCommandBuffer) {
  testGenerateMipmap(*iglDev_, *cmdQueue_, false);
}

TEST_F(TextureTest, GetTextureBytesPerRow) {
  const auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
  const auto range = TextureRangeDesc::new2D(0, 0, 10, 10);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(0)), 40);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(1)), 20);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(2)), 8);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(3)), 4);
  ASSERT_EQ(properties.getBytesPerRow(range.atMipLevel(4)), 4);
}

TEST_F(TextureTest, GetTextureBytesPerLayer) {
  const auto range = TextureRangeDesc::new2D(0, 0, 10, 10);
  {
    // Uncompressed
    const auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(0)), 400);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(1)), 100);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(2)), 16);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(3)), 4);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(4)), 4);
  }
  {
    // Compressed
    // 16 bytes per 5x5 block
    const auto properties =
        TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_ASTC_5x5);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(0)), 64);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(1)), 16);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(2)), 16);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(3)), 16);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(4)), 16);
  }
  {
    // Compressed
    // 8 bytes per 4x4 block
    const auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGB8_ETC2);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(0)), 72);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(1)), 32);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(2)), 8);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(3)), 8);
    EXPECT_EQ(properties.getBytesPerLayer(range.atMipLevel(4)), 8);
  }
}

//
// Test ITexture::getEstimatedSizeInBytes
//
TEST_F(TextureTest, GetEstimatedSizeInBytes) {
  auto calcSize =
      [&](size_t width, size_t height, TextureFormat format, size_t numMipLevels) -> size_t {
    Result ret;
    TextureDesc texDesc = TextureDesc::new2D(format,
                                             width,
                                             height,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
    texDesc.numMipLevels = numMipLevels;
    auto texture = iglDev_->createTexture(texDesc, &ret);
    if (ret.code != Result::Code::Ok || texture == nullptr) {
      return 0;
    }
    return texture->getEstimatedSizeInBytes();
  };

  const auto format = iglDev_->getBackendType() == BackendType::OpenGL
                          ? TextureFormat::R5G5B5A1_UNorm
                          : TextureFormat::RGBA_UNorm8;
  const size_t formatBytes = iglDev_->getBackendType() == BackendType::OpenGL ? 2 : 4;

  size_t bytes = 0;
  bytes = (12 * 34 * formatBytes);
  ASSERT_EQ(calcSize(12, 34, format, 1), bytes);
  bytes = static_cast<size_t>((16 + 8 + 4 + 2 + 1) * formatBytes);
  ASSERT_EQ(calcSize(16, 1, format, 5), bytes);
  if (iglDev_->hasFeature(DeviceFeatures::TextureNotPot)) {
    if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
      // ES 2.0 generates maximum mip levels
      bytes = static_cast<size_t>(
          (128 * 333 + 64 * 166 + 32 * 83 + 16 * 41 + 8 * 20 + 4 * 10 + 2 * 5 + 1 * 2 + 1 * 1) *
          formatBytes);
      ASSERT_EQ(calcSize(128, 333, format, 9), bytes);
    } else {
      bytes = static_cast<size_t>((128 * 333 + 64 * 166) * formatBytes);
      ASSERT_EQ(calcSize(128, 333, format, 2), bytes);
    }

    if (iglDev_->hasFeature(DeviceFeatures::TextureFormatRG)) {
      const auto rBytes = 1;
      const auto rgBytes = 2;
      bytes = static_cast<size_t>((16 + 8 + 4 + 2 + 1) * rBytes);
      ASSERT_EQ(calcSize(16, 1, TextureFormat::R_UNorm8, 5), bytes);
      if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
        // ES 2.0 generates maximum mip levels
        bytes = static_cast<size_t>(
            (128 * 333 + 64 * 166 + 32 * 83 + 16 * 41 + 8 * 20 + 4 * 10 + 2 * 5 + 1 * 2 + 1 * 1) *
            rgBytes);
        ASSERT_EQ(calcSize(128, 333, TextureFormat::RG_UNorm8, 9), bytes);
      } else {
        bytes = static_cast<size_t>((128 * 333 + 64 * 166) * rgBytes);
        ASSERT_EQ(calcSize(128, 333, TextureFormat::RG_UNorm8, 2), bytes);
      }
    }
  }
}

//
// Test ITexture::getFullRange and ITexture::getFullMipRange
//
TEST_F(TextureTest, GetRange) {
  auto createTexture = [&](size_t width,
                           size_t height,
                           TextureFormat format,
                           // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                           size_t numMipLevels) -> std::shared_ptr<ITexture> {
    Result ret;
    TextureDesc texDesc = TextureDesc::new2D(format,
                                             width,
                                             height,
                                             TextureDesc::TextureUsageBits::Sampled |
                                                 TextureDesc::TextureUsageBits::Attachment);
    texDesc.numMipLevels = numMipLevels;
    auto texture = iglDev_->createTexture(texDesc, &ret);
    if (ret.code != Result::Code::Ok || texture == nullptr) {
      return {};
    }
    return texture;
  };
  auto getFullRange = [&](size_t width,
                          size_t height,
                          TextureFormat format,
                          // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                          size_t numMipLevels,
                          size_t rangeMipLevel = 0,
                          size_t rangeNumMipLevels = 0) -> TextureRangeDesc {
    auto tex = createTexture(width, height, format, numMipLevels);
    return tex ? tex->getFullRange(rangeMipLevel,
                                   rangeNumMipLevels ? rangeNumMipLevels : numMipLevels)
               : TextureRangeDesc{};
  };
  auto getFullMipRange = [&](size_t width,
                             size_t height,
                             TextureFormat format,
                             // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                             size_t numMipLevels) -> TextureRangeDesc {
    auto tex = createTexture(width, height, format, numMipLevels);
    return tex ? tex->getFullMipRange() : TextureRangeDesc{};
  };
  auto rangesAreEqual = [&](const TextureRangeDesc& a, const TextureRangeDesc& b) -> bool {
    return std::memcmp(&a, &b, sizeof(TextureRangeDesc)) == 0;
  };
  const auto format = iglDev_->getBackendType() == BackendType::OpenGL
                          ? TextureFormat::R5G5B5A1_UNorm
                          : TextureFormat::RGBA_UNorm8;

  TextureRangeDesc range;
  range = TextureRangeDesc::new2D(0, 0, 12, 34, 0, 1);
  ASSERT_TRUE(rangesAreEqual(getFullRange(12, 34, format, 1), range));
  range = TextureRangeDesc::new2D(0, 0, 16, 1, 0, 4);
  ASSERT_TRUE(rangesAreEqual(getFullRange(16, 1, format, 4), range));

  // Test subset of mip levels
  ASSERT_TRUE(rangesAreEqual(getFullRange(16, 1, format, 4, 1, 1), range.atMipLevel(1)));

  // Test all mip levels
  ASSERT_TRUE(rangesAreEqual(getFullMipRange(16, 1, format, 4), range.withNumMipLevels(4)));

  if (iglDev_->hasFeature(DeviceFeatures::TextureNotPot)) {
    if (!iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain)) {
      // ES 2.0 generates maximum mip levels
      range = TextureRangeDesc::new2D(0, 0, 128, 333, 0, 9);
      ASSERT_TRUE(rangesAreEqual(getFullRange(128, 333, format, 9), range));

      // Test all mip levels
      ASSERT_TRUE(rangesAreEqual(getFullMipRange(128, 333, format, 9), range.withNumMipLevels(9)));
    } else {
      range = TextureRangeDesc::new2D(0, 0, 128, 333, 0, 2);
      ASSERT_TRUE(rangesAreEqual(getFullRange(128, 333, format, 2), range));

      // Test all mip levels
      ASSERT_TRUE(rangesAreEqual(getFullMipRange(128, 333, format, 2), range.withNumMipLevels(2)));
    }
  }
}

//
// Test the functionality of TextureDesc::calcMipmapLevelCount
//
TEST(TextureDescStaticTest, CalcMipmapLevelCount) {
  ASSERT_EQ(TextureDesc::calcNumMipLevels(1, 1), 1);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(4, 8), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(8, 4), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(10, 10), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(10, 10, 10), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(8, 4, 4), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(4, 8, 4), 4);
  ASSERT_EQ(TextureDesc::calcNumMipLevels(4, 4, 8), 4);
}

//
// Test TextureFormatProperties::getNumMipLevels
//
TEST_F(TextureTest, GetNumMipLevels) {
  {
    auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8);
    EXPECT_EQ(properties.getNumMipLevels(1, 1, 4), 1);
    EXPECT_EQ(properties.getNumMipLevels(2, 2, 4 * 4 + 4), 2);
    EXPECT_EQ(properties.getNumMipLevels(5, 5, 25 * 4 + 4 * 4 + 4), 3);

    auto range = TextureRangeDesc::new2D(0, 0, 100, 50, 0);
    range.numMipLevels = 5;
    EXPECT_EQ(properties.getNumMipLevels(100, 50, properties.getBytesPerRange(range)), 5);
  }

  {
    // Compressed
    // 16 bytes per 5x5 block
    auto properties = TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_ASTC_5x5);
    EXPECT_EQ(properties.getNumMipLevels(1, 1, 16), 1);
    EXPECT_EQ(properties.getNumMipLevels(2, 2, 16 + 16), 2);
    EXPECT_EQ(properties.getNumMipLevels(5, 5, 16 + 16 + 16), 3);

    auto range = TextureRangeDesc::new2D(0, 0, 100, 50, 0);
    range.numMipLevels = 5;
    EXPECT_EQ(properties.getNumMipLevels(100, 50, properties.getBytesPerRange(range)), 5);
  }
}

} // namespace igl::tests
