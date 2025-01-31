/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/TestDevice.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/TextureBuffer.h>
#include <igl/opengl/TextureTarget.h>
#include <string>

namespace igl::tests {

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
#define OFFSCREEN_TEX_WIDTH 2
#define OFFSCREEN_TEX_HEIGHT 2

struct TextureFormatData {
  GLuint glTexInternalFormat = 0x0000;
  GLuint glTexFormat = 0x0000;
  GLuint glTexType = 0x0000;
  TextureFormat texFormatOutput = TextureFormat::Invalid;
};

//
// OGLTextureTest
//
// Unit tests for OGL Texture, TextureTarget, and TextureBuffer.
// Covers code paths that may not be hit by top level texture calls from device.
//
class TextureOGLTest : public ::testing::Test {
 private:
 public:
  TextureOGLTest() = default;
  ~TextureOGLTest() override = default;

  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);
    device_ = util::createTestDevice();
    context_ = &static_cast<opengl::Device&>(*device_).getContext();
    ASSERT_TRUE(context_ != nullptr);
  }

  void TearDown() override {}

  // Member variables
 public:
  opengl::IContext* context_{};
  std::shared_ptr<::igl::IDevice> device_;
};

//
// Texture Creation Paths Test
//
// This tests all failure and success paths during texture creation specific
// to the base class igl::opengl::Texture.
//
TEST_F(TextureOGLTest, TextureCreation) {
  std::unique_ptr<igl::opengl::TextureTarget> textureTarget_;
  std::unique_ptr<igl::opengl::TextureBuffer> textureBuffer_;
  Result ret;
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           OFFSCREEN_TEX_WIDTH,
                                           OFFSCREEN_TEX_HEIGHT,
                                           TextureDesc::TextureUsageBits::Storage);

  { // Storage not supported by OGL Texture via TextureTarget API
    textureTarget_ = std::make_unique<igl::opengl::TextureTarget>(*context_, texDesc.format);
    ret = textureTarget_->create(texDesc, false);
    ASSERT_EQ(ret.code, Result::Code::Unsupported);
    Result::setOk(&ret);
  }
  { // Storage supported by OGL Texture via igl::Device API only if TexStorage is supported.
    auto texture = device_->createTexture(texDesc, &ret);
    ASSERT_EQ(
        ret.isOk(),
        context_->deviceFeatures().hasInternalFeature(igl::opengl::InternalFeatures::TexStorage));
    Result::setOk(&ret);
    if (context_->deviceFeatures().hasInternalFeature(igl::opengl::InternalFeatures::TexStorage)) {
      ASSERT_NE(texture, nullptr);
    } else {
      ASSERT_EQ(texture, nullptr);
    }
  }

  texDesc.usage = TextureDesc::TextureUsageBits::Sampled;

  // Sampled and hasStorageAlready not supported in OGL Texture
  textureBuffer_ = std::make_unique<igl::opengl::TextureBuffer>(*context_, texDesc.format);
  ret = textureBuffer_->create(texDesc, true);
  ASSERT_EQ(ret.code, Result::Code::Unsupported);

  // Correct usage of TextureBuffer::create
  textureBuffer_ = std::make_unique<igl::opengl::TextureBuffer>(*context_, texDesc.format);
  ret = textureBuffer_->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  // Cannot create the texture again after it has already been created
  ret = textureBuffer_->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::InvalidOperation);

  texDesc.usage = TextureDesc::TextureUsageBits::Attachment;

  // Correct usage of TextureTarget::create
  textureTarget_ = std::make_unique<igl::opengl::TextureTarget>(*context_, texDesc.format);
  ret = textureTarget_->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  // Cannot create the texture again after it has already been created
  ret = textureTarget_->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::InvalidOperation);
}

//
// Supported Texture Formats Test
//
// Tests expected behavior for IGL supported texture format checks
//
TEST_F(TextureOGLTest, TextureFormats) {
  // Set up inputs and expected outputs for Texture::toTextureFormat
  // {glTexInternalFormat, glTexFormat, glTexType, expected output TextureFormat}
  const std::vector<TextureFormatData> texFormats{
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_4x4_KHR, 0, 0, TextureFormat::RGBA_ASTC_4x4},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_4x4},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_5x4_KHR, 0, 0, TextureFormat::RGBA_ASTC_5x4},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_5x4},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_5x5_KHR, 0, 0, TextureFormat::RGBA_ASTC_5x5},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_5x5},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_6x5},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_6x6_KHR, 0, 0, TextureFormat::RGBA_ASTC_6x6},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_6x6},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_8x5_KHR, 0, 0, TextureFormat::RGBA_ASTC_8x5},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_8x5},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_8x6_KHR, 0, 0, TextureFormat::RGBA_ASTC_8x6},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_8x6},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_8x8_KHR, 0, 0, TextureFormat::RGBA_ASTC_8x8},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_8x8},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_10x5_KHR, 0, 0, TextureFormat::RGBA_ASTC_10x5},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_10x5},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_10x6_KHR, 0, 0, TextureFormat::RGBA_ASTC_10x6},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_10x6},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_10x8_KHR, 0, 0, TextureFormat::RGBA_ASTC_10x8},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_10x8},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_10x10_KHR, 0, 0, TextureFormat::RGBA_ASTC_10x10},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_10x10},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_12x10_KHR, 0, 0, TextureFormat::RGBA_ASTC_12x10},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_12x10},
      TextureFormatData{GL_COMPRESSED_RGBA_ASTC_12x12_KHR, 0, 0, TextureFormat::RGBA_ASTC_12x12},
      TextureFormatData{
          GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, 0, 0, TextureFormat::SRGB8_A8_ASTC_12x12},
      TextureFormatData{
          GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, 0, 0, TextureFormat::RGBA_PVRTC_2BPPV1},
      TextureFormatData{GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG, 0, 0, TextureFormat::RGB_PVRTC_2BPPV1},
      TextureFormatData{
          GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, 0, 0, TextureFormat::RGBA_PVRTC_4BPPV1},
      TextureFormatData{GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, 0, 0, TextureFormat::RGB_PVRTC_4BPPV1},
      TextureFormatData{GL_ETC1_RGB8_OES, 0, 0, TextureFormat::RGB8_ETC1},
      TextureFormatData{GL_COMPRESSED_RGB8_ETC2, 0, 0, TextureFormat::RGB8_ETC2},
      TextureFormatData{GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
                        0,
                        0,
                        TextureFormat::RGB8_Punchthrough_A1_ETC2},
      TextureFormatData{GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, TextureFormat::RGBA8_EAC_ETC2},
      TextureFormatData{GL_RED, GL_RED, GL_UNSIGNED_BYTE, TextureFormat::R_UNorm8},
      TextureFormatData{GL_RED, GL_RED, 0, TextureFormat::Invalid},
      TextureFormatData{GL_RG, GL_RG, GL_UNSIGNED_BYTE, TextureFormat::RG_UNorm8},
      TextureFormatData{GL_RG, GL_RG, 0, TextureFormat::Invalid},
      TextureFormatData{GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, TextureFormat::RGBX_UNorm8},
      TextureFormatData{GL_RG, GL_RG, 0, TextureFormat::Invalid},
      TextureFormatData{GL_BGR, GL_BGR, GL_UNSIGNED_SHORT_5_6_5, TextureFormat::B5G6R5_UNorm},
      TextureFormatData{GL_BGR, GL_BGR, 0, TextureFormat::Invalid},
      TextureFormatData{GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, TextureFormat::RGBA_UNorm8},
      TextureFormatData{
          GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, TextureFormat::RGB10_A2_UNorm_Rev},
      TextureFormatData{GL_RGB10_A2UI,
                        GL_RGBA_INTEGER,
                        GL_UNSIGNED_INT_2_10_10_10_REV,
                        TextureFormat::RGB10_A2_Uint_Rev},
      TextureFormatData{GL_RGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, TextureFormat::R5G5B5A1_UNorm},
      TextureFormatData{GL_RGBA8, 0, 0, TextureFormat::RGBA_UNorm8},
      TextureFormatData{GL_RGBA, GL_RG, GL_UNSIGNED_BYTE, TextureFormat::Invalid},
      TextureFormatData{GL_BGRA, GL_BGRA, GL_UNSIGNED_BYTE, TextureFormat::BGRA_UNorm8},
      TextureFormatData{GL_BGRA, GL_BGRA, GL_UNSIGNED_SHORT_5_5_5_1, TextureFormat::B5G5R5A1_UNorm},
      TextureFormatData{GL_BGRA, GL_BGRA, 0, TextureFormat::Invalid},
      TextureFormatData{GL_RGBA4, 0, 0, TextureFormat::ABGR_UNorm4},
      TextureFormatData{GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, TextureFormat::A_UNorm8},
      TextureFormatData{GL_ALPHA, GL_ALPHA, 0, TextureFormat::Invalid},
      TextureFormatData{GL_R16F, 0, 0, TextureFormat::R_F16},
      TextureFormatData{GL_R16UI, 0, 0, TextureFormat::R_UInt16},
      TextureFormatData{GL_R16, 0, 0, TextureFormat::R_UNorm16},
      TextureFormatData{GL_R32F, 0, 0, TextureFormat::R_F32},
      TextureFormatData{GL_R32UI, 0, 0, TextureFormat::R_UInt32},
      TextureFormatData{GL_RG16F, 0, 0, TextureFormat::RG_F16},
      TextureFormatData{GL_RG16, 0, 0, TextureFormat::RG_UNorm16},
      TextureFormatData{GL_RG16UI, 0, 0, TextureFormat::RG_UInt16},
      TextureFormatData{GL_RG32F, 0, 0, TextureFormat::RG_F32},
      TextureFormatData{GL_RGB16F, 0, 0, TextureFormat::RGB_F16},
      TextureFormatData{GL_RGBA16F, 0, 0, TextureFormat::RGBA_F16},
      TextureFormatData{GL_RGB32F, 0, 0, TextureFormat::RGB_F32},
      TextureFormatData{GL_RGBA32F, 0, 0, TextureFormat::RGBA_F32},
      TextureFormatData{GL_RGBA32UI, 0, 0, TextureFormat::RGBA_UInt32},
      TextureFormatData{
          GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, TextureFormat::Z_UNorm16},
      TextureFormatData{
          GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, TextureFormat::Z_UNorm32},
      TextureFormatData{GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, 0, TextureFormat::Invalid},
      TextureFormatData{
          GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, TextureFormat::Z_UNorm16},
      TextureFormatData{GL_DEPTH_COMPONENT16, 0, 0, TextureFormat::Z_UNorm16},
      TextureFormatData{
          GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, TextureFormat::Z_UNorm24},
      TextureFormatData{GL_DEPTH_COMPONENT24, 0, 0, TextureFormat::Z_UNorm24},
      TextureFormatData{
          GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, TextureFormat::Z_UNorm32},
      TextureFormatData{GL_DEPTH_COMPONENT32, 0, 0, TextureFormat::Z_UNorm32},
      TextureFormatData{GL_DEPTH_STENCIL,
                        GL_DEPTH_STENCIL,
                        GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
                        TextureFormat::S8_UInt_Z32_UNorm},
      TextureFormatData{GL_DEPTH_STENCIL, GL_DEPTH_STENCIL, 0, TextureFormat::Invalid},
      TextureFormatData{GL_DEPTH24_STENCIL8,
                        GL_DEPTH_STENCIL,
                        GL_UNSIGNED_INT_24_8,
                        TextureFormat::S8_UInt_Z24_UNorm},
      TextureFormatData{GL_DEPTH24_STENCIL8, 0, 0, TextureFormat::S8_UInt_Z24_UNorm},
      TextureFormatData{
          GL_STENCIL_INDEX, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, TextureFormat::S_UInt8},
      TextureFormatData{GL_STENCIL_INDEX8, 0, 0, TextureFormat::S_UInt8},
      TextureFormatData{GL_STENCIL_INDEX, GL_STENCIL_INDEX, 0, TextureFormat::Invalid},
      TextureFormatData{0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, TextureFormat::Invalid},
  };

  for (auto data : texFormats) {
    const TextureFormat output = igl::opengl::Texture::glInternalFormatToTextureFormat(
        data.glTexInternalFormat, data.glTexFormat, data.glTexType);
    ASSERT_EQ(output, data.texFormatOutput)
        << "IGL Format: "
        << igl::TextureFormatProperties::fromTextureFormat(data.texFormatOutput).name
        << " internalformat: 0x" << std::hex << data.glTexInternalFormat << " format: 0x"
        << std::hex << data.glTexFormat << " type: 0x" << std::hex << data.glTexType;
  }
}

//
// Texture Alignment Test
//
// This tests that alignment calculations are done correctly.
//
TEST_F(TextureOGLTest, TextureAlignment) {
  {
    constexpr size_t width = 128;
    constexpr size_t bytesPerPixel = 4;
    Result ret;

    const TextureDesc texDesc = TextureDesc::new2D(
        TextureFormat::RGBA_UNorm8, width, width, TextureDesc::TextureUsageBits::Sampled);

    auto texture = std::make_unique<igl::opengl::TextureTarget>(*context_, texDesc.format);
    ret = texture->create(texDesc, false);

    ASSERT_EQ(texture->getAlignment((width >> 0) * bytesPerPixel), 8);

    ASSERT_EQ(texture->getAlignment((width >> 0) * bytesPerPixel, 0), 8);
    ASSERT_EQ(texture->getAlignment((width >> 1) * bytesPerPixel, 1), 8);
    ASSERT_EQ(texture->getAlignment((width >> 2) * bytesPerPixel, 2), 8);
    ASSERT_EQ(texture->getAlignment((width >> 3) * bytesPerPixel, 3), 8);
    ASSERT_EQ(texture->getAlignment((width >> 4) * bytesPerPixel, 4), 8);
    ASSERT_EQ(texture->getAlignment((width >> 5) * bytesPerPixel, 5), 8);
    ASSERT_EQ(texture->getAlignment((width >> 6) * bytesPerPixel, 6), 8);
    ASSERT_EQ(texture->getAlignment((width >> 7) * bytesPerPixel, 7), 4);

    ASSERT_EQ(texture->getAlignment((width >> 0) * bytesPerPixel, 0, width >> 0), 8);
    ASSERT_EQ(texture->getAlignment((width >> 1) * bytesPerPixel, 1, width >> 1), 8);
    ASSERT_EQ(texture->getAlignment((width >> 2) * bytesPerPixel, 2, width >> 2), 8);
    ASSERT_EQ(texture->getAlignment((width >> 3) * bytesPerPixel, 3, width >> 3), 8);
    ASSERT_EQ(texture->getAlignment((width >> 4) * bytesPerPixel, 4, width >> 4), 8);
    ASSERT_EQ(texture->getAlignment((width >> 5) * bytesPerPixel, 5, width >> 5), 8);
    ASSERT_EQ(texture->getAlignment((width >> 6) * bytesPerPixel, 6, width >> 6), 8);
    ASSERT_EQ(texture->getAlignment((width >> 7) * bytesPerPixel, 7, width >> 7), 4);

    ASSERT_EQ(texture->getAlignment((width >> 1) * bytesPerPixel, 0, width >> 1), 8);
    ASSERT_EQ(texture->getAlignment((width >> 2) * bytesPerPixel, 1, width >> 2), 8);
    ASSERT_EQ(texture->getAlignment((width >> 3) * bytesPerPixel, 2, width >> 3), 8);
    ASSERT_EQ(texture->getAlignment((width >> 4) * bytesPerPixel, 3, width >> 4), 8);
    ASSERT_EQ(texture->getAlignment((width >> 5) * bytesPerPixel, 4, width >> 5), 8);
    ASSERT_EQ(texture->getAlignment((width >> 6) * bytesPerPixel, 5, width >> 6), 8);
    ASSERT_EQ(texture->getAlignment((width >> 7) * bytesPerPixel, 6, width >> 7), 4);
  }

  {
    constexpr size_t width = 24;
    constexpr size_t bytesPerPixel = 4;
    Result ret;

    const TextureDesc texDesc = TextureDesc::new2D(
        TextureFormat::RGBA_UNorm8, width, width, TextureDesc::TextureUsageBits::Sampled);

    auto texture = std::make_unique<igl::opengl::TextureTarget>(*context_, texDesc.format);
    ret = texture->create(texDesc, false);

    ASSERT_EQ(texture->getAlignment((width >> 0) * bytesPerPixel), 8);

    ASSERT_EQ(texture->getAlignment((width >> 0) * bytesPerPixel, 0), 8);
    ASSERT_EQ(texture->getAlignment((width >> 1) * bytesPerPixel, 1), 8);
    ASSERT_EQ(texture->getAlignment((width >> 2) * bytesPerPixel, 2), 8);
    ASSERT_EQ(texture->getAlignment((width >> 3) * bytesPerPixel, 3), 4);
    ASSERT_EQ(texture->getAlignment((width >> 4) * bytesPerPixel, 4), 4);

    ASSERT_EQ(texture->getAlignment((width >> 0) * bytesPerPixel, 0, width >> 0), 8);
    ASSERT_EQ(texture->getAlignment((width >> 1) * bytesPerPixel, 1, width >> 1), 8);
    ASSERT_EQ(texture->getAlignment((width >> 2) * bytesPerPixel, 2, width >> 2), 8);
    ASSERT_EQ(texture->getAlignment((width >> 3) * bytesPerPixel, 3, width >> 3), 4);
    ASSERT_EQ(texture->getAlignment((width >> 4) * bytesPerPixel, 4, width >> 4), 4);

    ASSERT_EQ(texture->getAlignment((width >> 1) * bytesPerPixel, 0, width >> 1), 8);
    ASSERT_EQ(texture->getAlignment((width >> 2) * bytesPerPixel, 1, width >> 2), 8);
    ASSERT_EQ(texture->getAlignment((width >> 3) * bytesPerPixel, 2, width >> 3), 4);
    ASSERT_EQ(texture->getAlignment((width >> 4) * bytesPerPixel, 3, width >> 4), 4);
  }
}

} // namespace igl::tests
