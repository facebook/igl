/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <string>
#include <igl/opengl/Device.h>
#include <igl/opengl/TextureBuffer.h>
#include <igl/opengl/TextureTarget.h>

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
 protected:
  opengl::IContext* context_{};
  std::shared_ptr<IDevice> device_;
};

//
// Texture Creation Paths Test
//
// This tests all failure and success paths during texture creation specific
// to the base class igl::opengl::Texture.
//
TEST_F(TextureOGLTest, TextureCreation) {
  std::unique_ptr<igl::opengl::TextureTarget> textureTarget;
  std::unique_ptr<igl::opengl::TextureBuffer> textureBuffer;
  Result ret;
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           OFFSCREEN_TEX_WIDTH,
                                           OFFSCREEN_TEX_HEIGHT,
                                           TextureDesc::TextureUsageBits::Storage);

  { // Storage not supported by OGL Texture via TextureTarget API
    textureTarget = std::make_unique<igl::opengl::TextureTarget>(*context_, texDesc.format);
    ret = textureTarget->create(texDesc, false);
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
  textureBuffer = std::make_unique<igl::opengl::TextureBuffer>(*context_, texDesc.format);
  ret = textureBuffer->create(texDesc, true);
  ASSERT_EQ(ret.code, Result::Code::Unsupported);

  // Correct usage of TextureBuffer::create
  textureBuffer = std::make_unique<igl::opengl::TextureBuffer>(*context_, texDesc.format);
  ret = textureBuffer->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  // Cannot create the texture again after it has already been created
  ret = textureBuffer->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::InvalidOperation);

  texDesc.usage = TextureDesc::TextureUsageBits::Attachment;

  // Correct usage of TextureTarget::create
  textureTarget = std::make_unique<igl::opengl::TextureTarget>(*context_, texDesc.format);
  ret = textureTarget->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  // Cannot create the texture again after it has already been created
  ret = textureTarget->create(texDesc, false);
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
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_4x4_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_4x4},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_4x4},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_5x4_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_5x4},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_5x4},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_5x5_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_5x5},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_5x5},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_6x5},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_6x6_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_6x6},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_6x6},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_8x5_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_8x5},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_8x5},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_8x6_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_8x6},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_8x6},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_8x8_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_8x8},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_8x8},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_10x5_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_10x5},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_10x5},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_10x6_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_10x6},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_10x6},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_10x8_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_10x8},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_10x8},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_10x10},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_10x10},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_12x10_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_12x10},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_12x10},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_ASTC_12x12},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::SRGB8_A8_ASTC_12x12},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_PVRTC_2BPPV1},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGB_PVRTC_2BPPV1},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_PVRTC_4BPPV1},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGB_PVRTC_4BPPV1},
      TextureFormatData{.glTexInternalFormat = GL_ETC1_RGB8_OES,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGB8_ETC1},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGB8_ETC2,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGB8_ETC2},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGB8_Punchthrough_A1_ETC2},
      TextureFormatData{.glTexInternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA8_EAC_ETC2},
      TextureFormatData{.glTexInternalFormat = GL_RED,
                        .glTexFormat = GL_RED,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::R_UNorm8},
      TextureFormatData{.glTexInternalFormat = GL_RED,
                        .glTexFormat = GL_RED,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_RG,
                        .glTexFormat = GL_RG,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::RG_UNorm8},
      TextureFormatData{.glTexInternalFormat = GL_RG,
                        .glTexFormat = GL_RG,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_RGB,
                        .glTexFormat = GL_RGB,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::RGBX_UNorm8},
      TextureFormatData{.glTexInternalFormat = GL_RG,
                        .glTexFormat = GL_RG,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_BGR,
                        .glTexFormat = GL_BGR,
                        .glTexType = GL_UNSIGNED_SHORT_5_6_5,
                        .texFormatOutput = TextureFormat::B5G6R5_UNorm},
      TextureFormatData{.glTexInternalFormat = GL_BGR,
                        .glTexFormat = GL_BGR,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_RGBA,
                        .glTexFormat = GL_RGBA,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::RGBA_UNorm8},
      TextureFormatData{.glTexInternalFormat = GL_RGB10_A2,
                        .glTexFormat = GL_RGBA,
                        .glTexType = GL_UNSIGNED_INT_2_10_10_10_REV,
                        .texFormatOutput = TextureFormat::RGB10_A2_UNorm_Rev},
      TextureFormatData{.glTexInternalFormat = GL_RGB10_A2UI,
                        .glTexFormat = GL_RGBA_INTEGER,
                        .glTexType = GL_UNSIGNED_INT_2_10_10_10_REV,
                        .texFormatOutput = TextureFormat::RGB10_A2_Uint_Rev},
      TextureFormatData{.glTexInternalFormat = GL_RGBA,
                        .glTexFormat = GL_RGBA,
                        .glTexType = GL_UNSIGNED_SHORT_5_5_5_1,
                        .texFormatOutput = TextureFormat::R5G5B5A1_UNorm},
      TextureFormatData{.glTexInternalFormat = GL_RGBA8,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_UNorm8},
      TextureFormatData{.glTexInternalFormat = GL_RGBA,
                        .glTexFormat = GL_RG,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_BGRA,
                        .glTexFormat = GL_BGRA,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::BGRA_UNorm8},
      TextureFormatData{.glTexInternalFormat = GL_BGRA,
                        .glTexFormat = GL_BGRA,
                        .glTexType = GL_UNSIGNED_SHORT_5_5_5_1,
                        .texFormatOutput = TextureFormat::B5G5R5A1_UNorm},
      TextureFormatData{.glTexInternalFormat = GL_BGRA,
                        .glTexFormat = GL_BGRA,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_RGBA4,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::ABGR_UNorm4},
      TextureFormatData{.glTexInternalFormat = GL_ALPHA,
                        .glTexFormat = GL_ALPHA,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::A_UNorm8},
      TextureFormatData{.glTexInternalFormat = GL_ALPHA,
                        .glTexFormat = GL_ALPHA,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_R16F,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::R_F16},
      TextureFormatData{.glTexInternalFormat = GL_R16UI,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::R_UInt16},
      TextureFormatData{.glTexInternalFormat = GL_R16,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::R_UNorm16},
      TextureFormatData{.glTexInternalFormat = GL_R32F,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::R_F32},
      TextureFormatData{.glTexInternalFormat = GL_R32UI,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::R_UInt32},
      TextureFormatData{.glTexInternalFormat = GL_RG16F,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RG_F16},
      TextureFormatData{.glTexInternalFormat = GL_RG16,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RG_UNorm16},
      TextureFormatData{.glTexInternalFormat = GL_RG16UI,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RG_UInt16},
      TextureFormatData{.glTexInternalFormat = GL_RG32F,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RG_F32},
      TextureFormatData{.glTexInternalFormat = GL_RGB16F,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGB_F16},
      TextureFormatData{.glTexInternalFormat = GL_RGBA16F,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_F16},
      TextureFormatData{.glTexInternalFormat = GL_RGB32F,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGB_F32},
      TextureFormatData{.glTexInternalFormat = GL_RGBA32F,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_F32},
      TextureFormatData{.glTexInternalFormat = GL_RGBA32UI,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::RGBA_UInt32},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT,
                        .glTexFormat = GL_DEPTH_COMPONENT,
                        .glTexType = GL_UNSIGNED_SHORT,
                        .texFormatOutput = TextureFormat::Z_UNorm16},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT,
                        .glTexFormat = GL_DEPTH_COMPONENT,
                        .glTexType = GL_UNSIGNED_INT,
                        .texFormatOutput = TextureFormat::Z_UNorm32},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT,
                        .glTexFormat = GL_DEPTH_COMPONENT,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT16,
                        .glTexFormat = GL_DEPTH_COMPONENT,
                        .glTexType = GL_UNSIGNED_SHORT,
                        .texFormatOutput = TextureFormat::Z_UNorm16},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT16,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Z_UNorm16},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT24,
                        .glTexFormat = GL_DEPTH_COMPONENT,
                        .glTexType = GL_UNSIGNED_INT,
                        .texFormatOutput = TextureFormat::Z_UNorm24},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT24,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Z_UNorm24},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT32,
                        .glTexFormat = GL_DEPTH_COMPONENT,
                        .glTexType = GL_UNSIGNED_INT,
                        .texFormatOutput = TextureFormat::Z_UNorm32},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_COMPONENT32,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Z_UNorm32},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_STENCIL,
                        .glTexFormat = GL_DEPTH_STENCIL,
                        .glTexType = GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
                        .texFormatOutput = TextureFormat::S8_UInt_Z32_UNorm},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH_STENCIL,
                        .glTexFormat = GL_DEPTH_STENCIL,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH24_STENCIL8,
                        .glTexFormat = GL_DEPTH_STENCIL,
                        .glTexType = GL_UNSIGNED_INT_24_8,
                        .texFormatOutput = TextureFormat::S8_UInt_Z24_UNorm},
      TextureFormatData{.glTexInternalFormat = GL_DEPTH24_STENCIL8,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::S8_UInt_Z24_UNorm},
      TextureFormatData{.glTexInternalFormat = GL_STENCIL_INDEX,
                        .glTexFormat = GL_STENCIL_INDEX,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::S_UInt8},
      TextureFormatData{.glTexInternalFormat = GL_STENCIL_INDEX8,
                        .glTexFormat = 0,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::S_UInt8},
      TextureFormatData{.glTexInternalFormat = GL_STENCIL_INDEX,
                        .glTexFormat = GL_STENCIL_INDEX,
                        .glTexType = 0,
                        .texFormatOutput = TextureFormat::Invalid},
      TextureFormatData{.glTexInternalFormat = 0,
                        .glTexFormat = GL_STENCIL_INDEX,
                        .glTexType = GL_UNSIGNED_BYTE,
                        .texFormatOutput = TextureFormat::Invalid},
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

//
// Mipmap Generation Flag Initialization Test
//
// This tests that the mipmapGeneration_ flag is properly initialized to Manual
//
TEST_F(TextureOGLTest, MipmapGenerationFlagInitialization) {
  Result ret;
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           OFFSCREEN_TEX_WIDTH,
                                           OFFSCREEN_TEX_HEIGHT,
                                           TextureDesc::TextureUsageBits::Sampled);

  auto texture = device_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(texture, nullptr);

  // Cast to OpenGL texture to access the getMipmapGeneration method
  auto* oglTexture = static_cast<opengl::Texture*>(texture.get());
  ASSERT_NE(oglTexture, nullptr);

  // Test that the mipmapGeneration flag is initialized to Manual by default
  ASSERT_EQ(oglTexture->getMipmapGeneration(), TextureDesc::TextureMipmapGeneration::Manual);
}

} // namespace igl::tests
