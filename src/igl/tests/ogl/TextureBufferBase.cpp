/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/TestDevice.h"

#include <cmath>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/opengl/CommandQueue.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/TextureBufferBase.h>
#include <string>

namespace igl::tests {

#ifndef GL_TEXTURE_BINDING_RECTANGLE
#define GL_TEXTURE_BINDING_RECTANGLE 0x84F6
#endif

// Picking this to check mipmap validity, max mipmap level = log(16) - 1 = 3
#define MIPMAP_TEX_WIDTH_16 16
#define MIPMAP_TEX_HEIGHT_16 16
#define MIPMAP_TEX_WIDTH_1023 1023
#define MIPMAP_TEX_HEIGHT_1023 1023
#define OFFSCREEN_TEX_WIDTH 2
#define OFFSCREEN_TEX_HEIGHT 2
//
// TextureBufferBaseOGLTest
//
// Unit tests for igl::opengl::TextureBufferBase.
// Covers code paths that may not be hit by top level texture calls from device.
//
class TextureBufferBaseOGLTest : public ::testing::Test {
 private:
 public:
  TextureBufferBaseOGLTest() = default;
  ~TextureBufferBaseOGLTest() override = default;

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
  std::shared_ptr<::igl::IDevice> device_;
};

class TextureBufferBaseMock : public igl::opengl::TextureBufferBase {
 public:
  explicit TextureBufferBaseMock(igl::opengl::IContext& context) :
    igl::opengl::TextureBufferBase(context, TextureFormat::RGBA_UNorm8) {}

  using igl::opengl::TextureBufferBase::setTextureBufferProperties;
  using igl::opengl::TextureBufferBase::setUsage;

  // Used by TextureBufferBase::attachAsColor()
  [[nodiscard]] uint32_t getSamples() const override {
    return num_samples;
  }

  bool getFormatDetails(TextureFormat textureFormat,
                        igl::TextureDesc::TextureUsage usage,
                        GLint& internalFormat,
                        GLenum& format,
                        GLenum& type) const {
    FormatDescGL formatGL;
    const auto result = toFormatDescGL(textureFormat, usage, formatGL);
    internalFormat = formatGL.internalFormat;
    format = formatGL.format;
    type = formatGL.type;
    return result;
  }
  uint32_t num_samples = 1;
};

//
// getType function Tests.
//
// This tests TextureBufferBase::getType(),
//
TEST_F(TextureBufferBaseOGLTest, TextureGetType) {
  std::unique_ptr<TextureBufferBaseMock> textureBufferBase_;
  textureBufferBase_ = std::make_unique<TextureBufferBaseMock>(*context_);
  textureBufferBase_->setUsage(TextureDesc::TextureUsageBits::Sampled);

  textureBufferBase_->setTextureBufferProperties(0, GL_TEXTURE_CUBE_MAP);
  ASSERT_EQ(TextureType::Cube, textureBufferBase_->getType());

  textureBufferBase_->setTextureBufferProperties(0, GL_TEXTURE_2D);
  ASSERT_EQ(TextureType::TwoD, textureBufferBase_->getType());

  // Unsupported Type
  textureBufferBase_->setTextureBufferProperties(0, GL_TEXTURE_BINDING_RECTANGLE);
  ASSERT_EQ(TextureType::Invalid, textureBufferBase_->getType());
}

//
// Bind function Tests.
//
// This tests TextureBufferBase::bind(),
//                               unbind(),
//
TEST_F(TextureBufferBaseOGLTest, TextureBindAndUnbind) {
  std::unique_ptr<TextureBufferBaseMock> textureBufferBase_;
  textureBufferBase_ = std::make_unique<TextureBufferBaseMock>(*context_);
  textureBufferBase_->setUsage(TextureDesc::TextureUsageBits::Sampled);

  GLuint textureID;
  context_->genTextures(1, &textureID);
  textureBufferBase_->setTextureBufferProperties(textureID, GL_TEXTURE_2D);

  GLint value;
  textureBufferBase_->bind();
  // Get binding and check it is non-zero
  context_->getIntegerv(GL_TEXTURE_BINDING_2D, &value);
  ASSERT_EQ(value, textureID);

  textureBufferBase_->unbind();
  // Get binding and check it is zero
  context_->getIntegerv(GL_TEXTURE_BINDING_2D, &value);
  ASSERT_EQ(value, GL_ZERO);

  context_->deleteTextures({textureID});
}

//
// Attach function Tests.
//
// This tests TextureBufferBase::attachAsColor(),
//                               attachAsDepth(),
//                               attachAsStencil(),
//
TEST_F(TextureBufferBaseOGLTest, TextureAttach) {
  std::unique_ptr<TextureBufferBaseMock> textureBufferBase_;
  textureBufferBase_ = std::make_unique<TextureBufferBaseMock>(*context_);
  textureBufferBase_->setUsage(TextureDesc::TextureUsageBits::Sampled);

  GLuint textureID;
  context_->genTextures(1, &textureID);

  GLuint tmpFb;
  context_->genFramebuffers(1, &tmpFb);
  context_->bindFramebuffer(GL_FRAMEBUFFER, tmpFb);

  GLint type = -1234;
  // === No target texture, nothing happens ===
  textureBufferBase_->attachAsColor(0, opengl::Texture::AttachmentParams{});
  context_->getFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
  ASSERT_EQ(type, GL_NONE);
  textureBufferBase_->attachAsDepth(opengl::Texture::AttachmentParams{});
  context_->getFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
  ASSERT_EQ(type, GL_NONE);
  textureBufferBase_->attachAsStencil(opengl::Texture::AttachmentParams{});
  context_->getFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
  ASSERT_EQ(type, GL_NONE);

  // === With target texture ===
  textureBufferBase_->setTextureBufferProperties(textureID, GL_TEXTURE_2D);
  textureBufferBase_->bind();
  textureBufferBase_->attachAsColor(0, opengl::Texture::AttachmentParams{});
  context_->getFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
  ASSERT_EQ(0, context_->getError());
  ASSERT_EQ(type, GL_TEXTURE);

  // Multiple Render Targets
  if (context_->deviceFeatures().hasFeature(DeviceFeatures::MultipleRenderTargets)) {
    const GLuint colorAttachment1 = GL_COLOR_ATTACHMENT1;
    textureBufferBase_->attachAsColor(1, opengl::Texture::AttachmentParams{});
    context_->getFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, colorAttachment1, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    ASSERT_EQ(type, GL_TEXTURE);
    ASSERT_EQ(0, context_->getError());
  }

  textureBufferBase_->attachAsDepth(opengl::Texture::AttachmentParams{});
  context_->getFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
  ASSERT_EQ(0, context_->getError());
  ASSERT_EQ(type, GL_TEXTURE);

  textureBufferBase_->attachAsStencil(opengl::Texture::AttachmentParams{});
  context_->getFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
  ASSERT_EQ(type, GL_TEXTURE);

  // Must initialize texture for multisample functionality to work
  GLint internalFormat;
  GLenum format;
  GLenum textureType;
  textureBufferBase_->setTextureBufferProperties(textureID, GL_TEXTURE_2D);
  ASSERT_TRUE(textureBufferBase_->getFormatDetails(TextureFormat::RGBA_UNorm8,
                                                   TextureDesc::TextureUsageBits::Sampled,
                                                   internalFormat,
                                                   format,
                                                   textureType));

  context_->texImage2D(GL_TEXTURE_2D,
                       0,
                       internalFormat,
                       OFFSCREEN_TEX_WIDTH,
                       OFFSCREEN_TEX_HEIGHT,
                       0,
                       format,
                       textureType,
                       nullptr);

  textureBufferBase_->num_samples = 123;
  textureBufferBase_->attachAsColor(0, opengl::Texture::AttachmentParams{});
  context_->getFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
  ASSERT_EQ(0, context_->getError());
  ASSERT_EQ(type, GL_TEXTURE);

  context_->deleteTextures({textureID});
}

//
// Basic Mipmap level Tests for TextureBufferBase
//
// This tests TextureBufferBase::generateMipmap(),
//                               getNumMipLevels(),
//
TEST_F(TextureBufferBaseOGLTest, TextureMipmapGen) {
  std::unique_ptr<TextureBufferBaseMock> textureBufferBase_, textureBufferBase2_;
  textureBufferBase_ = std::make_unique<TextureBufferBaseMock>(*context_);
  textureBufferBase_->setUsage(TextureDesc::TextureUsageBits::Sampled);
  textureBufferBase2_ = std::make_unique<TextureBufferBaseMock>(*context_);
  textureBufferBase2_->setUsage(TextureDesc::TextureUsageBits::Sampled);

  GLuint textureIDs[2];
  context_->genTextures(2, textureIDs);
  textureBufferBase_->setTextureBufferProperties(textureIDs[0], GL_TEXTURE_2D);
  textureBufferBase2_->setTextureBufferProperties(textureIDs[1], GL_TEXTURE_2D);

  // Generate mipmap and correct query of initial count
  TextureDesc texDesc16 = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                             MIPMAP_TEX_WIDTH_16,
                                             MIPMAP_TEX_HEIGHT_16,
                                             TextureDesc::TextureUsageBits::Sampled);
  TextureDesc texDesc1023 = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                               MIPMAP_TEX_WIDTH_1023,
                                               MIPMAP_TEX_HEIGHT_1023,
                                               TextureDesc::TextureUsageBits::Sampled);

  // By default numMipLevels = 1, which is the base texture
  // 16x16
  size_t maxDim = std::max<size_t>(texDesc16.width, texDesc16.height);
  int targetlevel = std::floor(log2(maxDim)) + 1;

  texDesc16.numMipLevels = targetlevel;
  Result ret = textureBufferBase_->create(texDesc16, false);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  igl::opengl::CommandQueue queue;
  textureBufferBase_->generateMipmap(queue);
  ASSERT_EQ(textureBufferBase_->getNumMipLevels(), targetlevel);

  // 1023x1023
  maxDim = std::max<size_t>(texDesc1023.width, texDesc1023.height);
  targetlevel = std::floor(log2(maxDim)) + 1;

  texDesc1023.numMipLevels = targetlevel;
  ret = textureBufferBase2_->create(texDesc1023, false);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  textureBufferBase2_->generateMipmap(queue);
  ASSERT_EQ(textureBufferBase2_->getNumMipLevels(), targetlevel);
}

} // namespace igl::tests
