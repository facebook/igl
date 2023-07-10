/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/Common.h"
#include "../util/TextureFormatTestBase.h"
#include <igl/metal/CommandBuffer.h>
#include <igl/metal/PlatformDevice.h>
#include <igl/metal/Texture.h>

#include <gtest/gtest.h>
#include <utility>

namespace igl {
namespace tests {

#define OFFSCREEN_TEX_WIDTH 2
#define OFFSCREEN_TEX_HEIGHT 2

// Picking this to check mipmap validity, max mipmap level = log(16) - 1 = 3
#define MIPMAP_TEX_WIDTH 16
#define MIPMAP_TEX_HEIGHT 16

class TextureMTLTest : public ::testing::Test {
 public:
  TextureMTLTest() = default;
  ~TextureMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(device_, cmdQueue_);

    texDesc_ = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                  OFFSCREEN_TEX_WIDTH,
                                  OFFSCREEN_TEX_HEIGHT,
                                  TextureDesc::TextureUsageBits::Sampled);
    Result res;
    texture_ = device_->createTexture(texDesc_, &res);
    ASSERT_TRUE(res.isOk());
  }

  void TearDown() override {}

 public:
  TextureDesc texDesc_;
  std::shared_ptr<ITexture> texture_;
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

class TextureFormatMTLTest : public util::TextureFormatTestBase {
 public:
  TextureFormatMTLTest() = default;
  ~TextureFormatMTLTest() override = default;
};

// Test basic getter methods for successful construction
TEST_F(TextureMTLTest, ConstructionFromMTLTexture) {
  auto mtlTexture = std::dynamic_pointer_cast<metal::Texture>(texture_);
  ASSERT_NE(mtlTexture->get(), nullptr);

  auto dimensions = texture_->getDimensions();
  ASSERT_EQ(dimensions.width, texDesc_.width);
  ASSERT_EQ(dimensions.height, texDesc_.height);
  ASSERT_EQ(dimensions.depth, texDesc_.depth);

  auto samples = texture_->getSamples();
  ASSERT_EQ(samples, texDesc_.numSamples);

  auto format = texture_->getFormat();
  ASSERT_EQ(format, texDesc_.format);
}

// Test upload
TEST_F(TextureMTLTest, Upload) {
  const auto texRangeDesc = TextureRangeDesc();
  Result res = texture_->upload(texRangeDesc, nullptr);
  ASSERT_TRUE(res.isOk());
}

// Test getDrawable
TEST_F(TextureMTLTest, GetDrawable) {
  auto mtlTexture = std::dynamic_pointer_cast<metal::Texture>(texture_);
  ASSERT_EQ(mtlTexture->getDrawable(), nullptr);
}

// Test mipmap generation and methods
TEST_F(TextureMTLTest, GetMipmapsCount) {
  Result res;
  TextureDesc miptexDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                              MIPMAP_TEX_WIDTH,
                                              MIPMAP_TEX_HEIGHT,
                                              TextureDesc::TextureUsageBits::Sampled);

  int targetlevel = 0;
  size_t ind = std::min<size_t>(miptexDesc.width, miptexDesc.height);
  while (ind >>= 1) {
    ++targetlevel;
  }

  miptexDesc.numMipLevels = targetlevel; // log(16) - 1
  std::shared_ptr<ITexture> mipTexture = device_->createTexture(miptexDesc, &res);
  ASSERT_TRUE(res.isOk());

  auto mtlTexture = std::static_pointer_cast<metal::Texture>(mipTexture);
  mtlTexture->generateMipmap(*cmdQueue_);

  // Wait for completion
  CommandBufferDesc desc = {};
  std::shared_ptr<ICommandBuffer> cmdBuf = cmdQueue_->createCommandBuffer(desc, &res);
  id<MTLCommandBuffer> mtlCmdBuf = static_cast<igl::metal::CommandBuffer&>(*cmdBuf).get();
  [mtlCmdBuf commit];
  cmdBuf->waitUntilCompleted();

  ASSERT_EQ(mtlTexture->getNumMipLevels(), targetlevel); // Should get log(16) - 1 = 3
}

// Test conversion from MTLTextureType to IGL TextureType
TEST_F(TextureMTLTest, ToTextureType) {
  std::vector<std::pair<MTLTextureType, TextureType>> inputAndExpectdList = {
      std::make_pair(MTLTextureType2D, TextureType::TwoD),
      std::make_pair(MTLTextureType2DMultisample, TextureType::TwoD),
      std::make_pair(MTLTextureType2DArray, TextureType::TwoDArray),
      std::make_pair(MTLTextureType3D, TextureType::ThreeD),
      std::make_pair(MTLTextureTypeCube, TextureType::Cube),
      std::make_pair(MTLTextureTypeCubeArray, TextureType::Invalid),
  };
  if (@available(macOS 10.14, iOS 14.0, *)) {
    inputAndExpectdList.emplace_back(MTLTextureType2DMultisampleArray, TextureType::TwoDArray);
  }

  for (auto inputAndExpectd : inputAndExpectdList) {
    auto input = inputAndExpectd.first;
    auto expected = inputAndExpectd.second;
    auto result = igl::metal::Texture::convertType(input);
    ASSERT_EQ(expected, result);
  }
}

// Test conversion from IGL TextureType to MTLTextureType
// Current IGL makes the following assumptions, which may not be valid in the future
//   * Falls back to MTLTextureType1D for TextureType::Invalid
TEST_F(TextureMTLTest, ToMTLTextureType) {
  std::vector<std::tuple<TextureType, size_t, MTLTextureType>> inputAndExpectdList = {
      std::make_tuple(TextureType::Invalid, 1, MTLTextureType1D),
      std::make_tuple(TextureType::TwoD, 1, MTLTextureType2D),
      std::make_tuple(TextureType::TwoD, 2, MTLTextureType2DMultisample),
      std::make_tuple(TextureType::TwoDArray, 1, MTLTextureType2DArray),
      std::make_tuple(TextureType::ThreeD, 1, MTLTextureType3D),
      std::make_tuple(TextureType::Cube, 1, MTLTextureTypeCube)};
  if (@available(macOS 10.14, iOS 14.0, *)) {
    inputAndExpectdList.emplace_back(TextureType::TwoDArray, 2, MTLTextureType2DMultisampleArray);
  }

  for (auto inputAndExpectd : inputAndExpectdList) {
    auto input = std::get<0>(inputAndExpectd);
    auto numSamples = std::get<1>(inputAndExpectd);
    auto expected = std::get<2>(inputAndExpectd);
    auto result = igl::metal::Texture::convertType(input, numSamples);
    ASSERT_EQ(expected, result);
  }
}

static std::shared_ptr<igl::ITexture> createCVPixelBufferTextureWithSize(
    igl::TextureFormat format,
    const size_t width,
    const size_t height,
    const std::shared_ptr<igl::IDevice>& device,
    Result& outResult) {
  igl::BackendType backend = device->getBackendType();
  CVPixelBufferRef pixelBuffer = nullptr;
  NSDictionary* bufferAttributes = @{
    (NSString*)kCVPixelBufferIOSurfacePropertiesKey : @{},
    (NSString*)kCVPixelBufferMetalCompatibilityKey : @(backend == igl::BackendType::Metal),
  };

  CVReturn result = CVPixelBufferCreate(kCFAllocatorDefault,
                                        width,
                                        height,
                                        kCVPixelFormatType_32BGRA,
                                        (__bridge CFDictionaryRef)(bufferAttributes),
                                        &pixelBuffer);
  if (result != kCVReturnSuccess) {
    Result::setResult(&outResult,
                      Result::Code::RuntimeError,
                      "CVPixelBufferCreate failed to create pixel buffer");
    return nullptr;
  }

  auto platformDevice = device->getPlatformDevice<igl::metal::PlatformDevice>();
  std::shared_ptr<igl::ITexture> texture =
      platformDevice->createTextureFromNativePixelBuffer(pixelBuffer, format, 0, &outResult);
  if (!outResult.isOk()) {
    return nullptr;
  }
  if (texture == nullptr) {
    Result::setResult(
        &outResult, Result::Code::RuntimeError, "failed to create igl texture from CVPixelBuffer");
    return nullptr;
  }
  CVPixelBufferRelease(pixelBuffer);
  Result::setOk(&outResult);
  return texture;
}

TEST_F(TextureFormatMTLTest, createTextureFromNativePixelBufferSuccess) {
  Result result;
  auto texture =
      createCVPixelBufferTextureWithSize(TextureFormat::BGRA_UNorm8, 100, 100, iglDev_, result);
  ASSERT_EQ(result.isOk(), true) << result.message.c_str();
  testUsage(texture,
            TextureDesc::TextureUsageBits::Sampled | TextureDesc::TextureUsageBits::Attachment,
            "SampledAttachment");
}

TEST_F(TextureMTLTest, createTextureFromNativePixelBufferWithInvalidFormat) {
  Result result;
  auto texture =
      createCVPixelBufferTextureWithSize(TextureFormat::Invalid, 100, 100, device_, result);
  ASSERT_EQ(result.isOk(), false) << result.message.c_str();
}

TEST_F(TextureMTLTest, ConvertTextureFormats) {
  std::vector<TextureFormat> inputFormats = {
    TextureFormat::A_UNorm8,
    TextureFormat::R_UNorm8,
    TextureFormat::R_F16,
    TextureFormat::R_UInt16,
#if !IGL_PLATFORM_MACOS
    TextureFormat::B5G5R5A1_UNorm,
#endif
#if !(IGL_PLATFORM_MACOS || IGL_PLATFORM_IOS_SIMULATOR)
    // https://developer.apple.com/documentation/metal/developing_metal_apps_that_run_in_simulator
    TextureFormat::B5G6R5_UNorm,
    TextureFormat::ABGR_UNorm4,
#endif
    TextureFormat::RG_UNorm8,
    TextureFormat::R4G2B2_UNorm_Apple,
    TextureFormat::R4G2B2_UNorm_Rev_Apple,
    TextureFormat::RGBA_UNorm8,
    TextureFormat::BGRA_UNorm8,
    TextureFormat::RG_F16,
    TextureFormat::RG_UInt16,
    TextureFormat::RGB10_A2_UNorm_Rev,
    TextureFormat::RGB10_A2_Uint_Rev,
    TextureFormat::BGR10_A2_Unorm,
    TextureFormat::R_F32,
    TextureFormat::RGBA_F16,
    TextureFormat::RGBA_UInt32,
    TextureFormat::RGBA_F32,
#if !IGL_PLATFORM_MACOS
    TextureFormat::RGBA_ASTC_4x4,
    TextureFormat::SRGB8_A8_ASTC_4x4,
    TextureFormat::RGBA_ASTC_5x4,
    TextureFormat::SRGB8_A8_ASTC_5x4,
    TextureFormat::RGBA_ASTC_5x5,
    TextureFormat::SRGB8_A8_ASTC_5x5,
    TextureFormat::RGBA_ASTC_6x5,
    TextureFormat::SRGB8_A8_ASTC_6x5,
    TextureFormat::RGBA_ASTC_6x6,
    TextureFormat::SRGB8_A8_ASTC_6x6,
    TextureFormat::RGBA_ASTC_8x5,
    TextureFormat::SRGB8_A8_ASTC_8x5,
    TextureFormat::RGBA_ASTC_8x6,
    TextureFormat::SRGB8_A8_ASTC_8x6,
    TextureFormat::RGBA_ASTC_8x8,
    TextureFormat::SRGB8_A8_ASTC_8x8,
    TextureFormat::RGBA_ASTC_10x5,
    TextureFormat::SRGB8_A8_ASTC_10x5,
    TextureFormat::RGBA_ASTC_10x6,
    TextureFormat::SRGB8_A8_ASTC_10x6,
    TextureFormat::RGBA_ASTC_10x8,
    TextureFormat::SRGB8_A8_ASTC_10x8,
    TextureFormat::RGBA_ASTC_10x10,
    TextureFormat::SRGB8_A8_ASTC_10x10,
    TextureFormat::RGBA_ASTC_12x10,
    TextureFormat::SRGB8_A8_ASTC_12x10,
    TextureFormat::RGBA_ASTC_12x12,
    TextureFormat::SRGB8_A8_ASTC_12x12,
    TextureFormat::RGBA_PVRTC_2BPPV1,
    TextureFormat::RGB_PVRTC_2BPPV1,
    TextureFormat::RGBA_PVRTC_4BPPV1,
    TextureFormat::RGB_PVRTC_4BPPV1,
    TextureFormat::RGB8_ETC1,
    TextureFormat::RGB8_ETC2,
    TextureFormat::SRGB8_ETC2,
    TextureFormat::RGBA8_EAC_ETC2,
    TextureFormat::SRGB8_A8_EAC_ETC2,
    TextureFormat::RG_EAC_UNorm,
    TextureFormat::RG_EAC_SNorm,
    TextureFormat::R_EAC_UNorm,
    TextureFormat::R_EAC_SNorm,
#endif
    TextureFormat::Z_UNorm16,
    TextureFormat::Z_UNorm24,
    TextureFormat::Z_UNorm32,
    TextureFormat::S8_UInt_Z24_UNorm,
    TextureFormat::S8_UInt_Z32_UNorm,
    TextureFormat::S_UInt8
  };

  std::vector<TextureFormat> invalidTextureFormats = {
    TextureFormat::Invalid,
    TextureFormat::L_UNorm8,
#if IGL_PLATFORM_MACOS
    TextureFormat::B5G5R5A1_UNorm,
#endif
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_IOS_SIMULATOR
    TextureFormat::B5G6R5_UNorm,
    TextureFormat::ABGR_UNorm4,
#endif
    TextureFormat::LA_UNorm8,
    TextureFormat::R5G5B5A1_UNorm,
    TextureFormat::RGBX_UNorm8,
    TextureFormat::BGRA_UNorm8_Rev,
    TextureFormat::RGB_F16,
    TextureFormat::RGB_F32,
#if IGL_PLATFORM_MACOS
    TextureFormat::RGBA_ASTC_4x4,
    TextureFormat::SRGB8_A8_ASTC_4x4,
    TextureFormat::RGBA_ASTC_5x4,
    TextureFormat::SRGB8_A8_ASTC_5x4,
    TextureFormat::RGBA_ASTC_5x5,
    TextureFormat::SRGB8_A8_ASTC_5x5,
    TextureFormat::RGBA_ASTC_6x5,
    TextureFormat::SRGB8_A8_ASTC_6x5,
    TextureFormat::RGBA_ASTC_6x6,
    TextureFormat::SRGB8_A8_ASTC_6x6,
    TextureFormat::RGBA_ASTC_8x5,
    TextureFormat::SRGB8_A8_ASTC_8x5,
    TextureFormat::RGBA_ASTC_8x6,
    TextureFormat::SRGB8_A8_ASTC_8x6,
    TextureFormat::RGBA_ASTC_8x8,
    TextureFormat::SRGB8_A8_ASTC_8x8,
    TextureFormat::RGBA_ASTC_10x5,
    TextureFormat::SRGB8_A8_ASTC_10x5,
    TextureFormat::RGBA_ASTC_10x6,
    TextureFormat::SRGB8_A8_ASTC_10x6,
    TextureFormat::RGBA_ASTC_10x8,
    TextureFormat::SRGB8_A8_ASTC_10x8,
    TextureFormat::RGBA_ASTC_10x10,
    TextureFormat::SRGB8_A8_ASTC_10x10,
    TextureFormat::RGBA_ASTC_12x10,
    TextureFormat::SRGB8_A8_ASTC_12x10,
    TextureFormat::RGBA_ASTC_12x12,
    TextureFormat::SRGB8_A8_ASTC_12x12,
    TextureFormat::RGBA_PVRTC_2BPPV1,
    TextureFormat::RGB_PVRTC_2BPPV1,
    TextureFormat::RGBA_PVRTC_4BPPV1,
    TextureFormat::RGB_PVRTC_4BPPV1,
    TextureFormat::RGB8_ETC1,
    TextureFormat::RGB8_ETC2,
    TextureFormat::SRGB8_ETC2,
#endif
    TextureFormat::RGB8_Punchthrough_A1_ETC2,
    TextureFormat::SRGB8_Punchthrough_A1_ETC2,
#if IGL_PLATFORM_MACOS
    TextureFormat::RGBA8_EAC_ETC2,
    TextureFormat::SRGB8_A8_EAC_ETC2,
    TextureFormat::RG_EAC_UNorm,
    TextureFormat::RG_EAC_SNorm,
    TextureFormat::R_EAC_UNorm,
    TextureFormat::R_EAC_SNorm,
#endif
  };

  for (auto format : invalidTextureFormats) {
    ASSERT_EQ(true,
              igl::metal::Texture::textureFormatToMTLPixelFormat(format) == MTLPixelFormatInvalid);
  }
}

} // namespace tests
} // namespace igl
