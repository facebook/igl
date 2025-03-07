/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/TestDevice.h"

#include <cmath>
#include <gtest/gtest.h>
#include <igl/opengl/CommandQueue.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/TextureBuffer.h>
#include <string>

namespace igl::tests {

// Picking this just to match the texture we will use. If you use a different
// size texture, then you will have to either create a new offscreenTexture_
// and the framebuffer object in your test, so know exactly what the end result
// would be after sampling
#define OFFSCREEN_TEX_WIDTH 2
#define OFFSCREEN_TEX_HEIGHT 2

// Picking this to check mipmap validity, max mipmap level = log(16) - 1 = 3
#define MIPMAP_TEX_WIDTH 16
#define MIPMAP_TEX_HEIGHT 16

struct TextureFormatToGL {
  TextureFormat texFormatInput = TextureFormat::Invalid;

  // Expected conversion to GL
  GLuint glInternalFormat = 0x0000;
  GLuint glFormat = 0x0000;
  GLuint glType = 0x0000;
};

//
// TextureBufferOGLTest
//
// Unit tests for igl::opengl::TextureBuffer.
// Covers code paths that may not be hit by top level texture calls from device.
//
class TextureBufferOGLTest : public ::testing::Test {
 private:
 public:
  TextureBufferOGLTest() = default;
  ~TextureBufferOGLTest() override = default;

  void SetUp() override {
    // Turn off debug breaks, only use in debug mode
    igl::setDebugBreakEnabled(false);

    device_ = util::createTestDevice();
    ASSERT_TRUE(device_ != nullptr);
    context_ = &static_cast<opengl::Device&>(*device_).getContext();
    ASSERT_TRUE(context_ != nullptr);
  }

  void TearDown() override {}

  // Member variables
 public:
  opengl::IContext* context_{};
  std::shared_ptr<IDevice> device_;
};

//
// Texture Creation Paths Test
//
// This tests all failure and success paths for TextureBuffer::create.
// Also covers private function createTexture which is called within create.
// See tests for Texture.cpp for cases covering cases specific to base class.
//
TEST_F(TextureBufferOGLTest, TextureCreation) {
  std::unique_ptr<igl::opengl::TextureBuffer> textureBuffer_;
  Result ret;
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::ABGR_UNorm4,
                                           OFFSCREEN_TEX_WIDTH,
                                           OFFSCREEN_TEX_HEIGHT,
                                           TextureDesc::TextureUsageBits::Sampled);

  // Correct usage of TextureBuffer::create
  textureBuffer_ = std::make_unique<igl::opengl::TextureBuffer>(*context_, texDesc.format);
  ret = textureBuffer_->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  // kRenderTarget not supported by TextureBuffer
  texDesc.usage = TextureDesc::TextureUsageBits::Attachment;
  textureBuffer_ = std::make_unique<igl::opengl::TextureBuffer>(*context_, texDesc.format);
  ret = textureBuffer_->create(texDesc, false);
  ASSERT_FALSE(ret.isOk());

  texDesc.usage = TextureDesc::TextureUsageBits::Sampled;

  // Incorrect texture format
  texDesc.format = TextureFormat::Invalid;
  textureBuffer_ = std::make_unique<igl::opengl::TextureBuffer>(*context_, texDesc.format);
  ret = textureBuffer_->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

//
// TextureBuffer Mipmap generation Test
//
// Tests expected behavior for Texture mipmap for supported GL formats.
// Test paths are tested through TextureBuffer::create
//
TEST_F(TextureBufferOGLTest, TextureMipmapGen) {
  std::unique_ptr<igl::opengl::TextureBuffer> textureBuffer_;
  Result ret;
  // Generate mipmap and correct query of initial count
  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           MIPMAP_TEX_WIDTH,
                                           MIPMAP_TEX_HEIGHT,
                                           TextureDesc::TextureUsageBits::Sampled);

  const size_t maxDim = std::max<size_t>(texDesc.width, texDesc.height);
  const int targetlevel = std::floor(log2(maxDim)) + 1;

  texDesc.numMipLevels = targetlevel; // log(16) + 1
  textureBuffer_ = std::make_unique<igl::opengl::TextureBuffer>(*context_, texDesc.format);
  ret = textureBuffer_->create(texDesc, false);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  igl::opengl::CommandQueue queue;
  textureBuffer_->generateMipmap(queue);
  ASSERT_EQ(textureBuffer_->getNumMipLevels(), targetlevel);
}

} // namespace igl::tests
