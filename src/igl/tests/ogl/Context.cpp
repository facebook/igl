/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/TestDevice.h"

#include <gtest/gtest.h>
#include <igl/Macros.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/Texture.h>

#define DUMMY_FILE_NAME "dummy_file_name"
#define DUMMY_LINE_NUM 0

namespace igl::tests {

class ContextOGLTest : public ::testing::Test {
 public:
  ContextOGLTest() = default;
  ~ContextOGLTest() override = default;

  void SetUp() override {
    // We will be purposely tripping a few ASSERT conditions
    igl::setDebugBreakEnabled(false);

    device_ = util::createTestDevice();
    ASSERT_TRUE(device_ != nullptr);
    context_ = &static_cast<opengl::Device&>(*device_).getContext();
    ASSERT_TRUE(context_ != nullptr);

    // Need to do this to support the CheckForError tests, otherwise the error
    // code will get reset before we read it
    context_->enableAutomaticErrorCheck(false);
  }

  void TearDown() override {}

 public:
  opengl::IContext* context_{};
  std::shared_ptr<::igl::IDevice> device_;
};

/// Test basic functionality for binding GL_FRAMEBUFFER.
TEST_F(ContextOGLTest, GlBindFramebuffer) {
  GLuint framebufferId;
  context_->genFramebuffers(1, &framebufferId);

  context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferId);

  GLint retrievedFramebuffer = -1;
  context_->getIntegerv(GL_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
  ASSERT_EQ(framebufferId, retrievedFramebuffer);

  // Clean up
  context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
  context_->deleteFramebuffers(1, &framebufferId);
}

/// On platforms that support GL_READ_FRAMEBUFFER and GL_DRAW_FRAMEBUFFER, binding GL_FRAMEBUFFER
/// should be equivalent to binding both of them to the same value.
TEST_F(ContextOGLTest, GlFramebufferBindSetsBothDrawFramebufferAndReadFramebuffer) {
  // This doesn't apply on platforms with no support for GL_READ_FRAMEBUFFER/GL_DRAW_FRAMEBUFFER
  if (context_->deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    GLuint framebufferIds[2];
    context_->genFramebuffers(2, framebufferIds);

    context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferIds[0]);

    GLint retrievedFramebuffer = -1;
    context_->getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
    ASSERT_EQ(framebufferIds[0], retrievedFramebuffer);

    retrievedFramebuffer = -1;
    context_->getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
    ASSERT_EQ(framebufferIds[0], retrievedFramebuffer);

    // Clean up
    context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
    context_->deleteFramebuffers(2, framebufferIds);
  }
}

/// We have to make sure our state cache works properly with the combination of GL_FRAMEBUFFER and
/// GL_DRAW_FRAMEBUFFER. If we bind GL_DRAW_FRAMEBUFFER to value A, then GL_FRAMEBUFFER to a
/// different value B, we have to make sure the state cache reflects the fact that
/// GL_DRAW_FRAMEBUFFER is now bound to B. Binding GL_DRAW_FRAMEBUFFER to A again should NOT be
/// handled just in the cache layer, but should actually be sent through to OpenGL.
TEST_F(ContextOGLTest, StateCacheUpdatesGLDrawFramebufferCacheEvenWhenSettingGLFramebuffer) {
  // This doesn't apply on platforms with no support for GL_DRAW_FRAMEBUFFER
  if (context_->deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    GLuint framebufferIds[2];
    context_->genFramebuffers(2, framebufferIds);

    // Validate that the state cache doesn't get confused when switching between GL_FRAMEBUFFER
    // and GL_DRAW_FRAMEBUFFER
    context_->bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferIds[0]);
    context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferIds[1]);
    // This should still set the value, assuming our cache is working properly
    context_->bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferIds[0]);

    GLint retrievedFramebuffer = -1;
    context_->getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
    ASSERT_EQ(framebufferIds[0], retrievedFramebuffer);

    retrievedFramebuffer = -1;
    context_->getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
    ASSERT_EQ(framebufferIds[1], retrievedFramebuffer);

    // Clean up
    context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
    context_->deleteFramebuffers(2, framebufferIds);
  }
}

/// We have to make sure our state cache works properly with the combination of GL_FRAMEBUFFER and
/// GL_READ_FRAMEBUFFER. If we bind GL_READ_FRAMEBUFFER to value A, then GL_FRAMEBUFFER to a
/// different value B, we have to make sure the state cache reflects the fact that
/// GL_READ_FRAMEBUFFER is now bound to B. Binding GL_READ_FRAMEBUFFER to A again should NOT be
/// handled just in the cache layer, but should actually be sent through to OpenGL.
TEST_F(ContextOGLTest, StateCacheUpdatesGLReadFramebufferCacheEvenWhenSettingGLFramebuffer) {
  // This doesn't apply on platforms with no support for GL_READ_FRAMEBUFFER
  if (context_->deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    GLuint framebufferIds[2];
    context_->genFramebuffers(2, framebufferIds);

    // Validate that the state cache doesn't get confused when switching between GL_FRAMEBUFFER
    // and GL_READ_FRAMEBUFFER
    context_->bindFramebuffer(GL_READ_FRAMEBUFFER, framebufferIds[0]);
    context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferIds[1]);
    // This should still set the value, assuming our cache is working properly
    context_->bindFramebuffer(GL_READ_FRAMEBUFFER, framebufferIds[0]);

    GLint retrievedFramebuffer = -1;
    context_->getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
    ASSERT_EQ(framebufferIds[0], retrievedFramebuffer);

    retrievedFramebuffer = -1;
    context_->getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
    ASSERT_EQ(framebufferIds[1], retrievedFramebuffer);

    // Clean up
    context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
    context_->deleteFramebuffers(2, framebufferIds);
  }
}

/// This test is to make sure that we properly invalidate the GL_FRAMEBUFFER
/// binding point when the GL_DRAW_FRAMEBUFFER binding points is used
TEST_F(ContextOGLTest, StateCacheInvalidateFramebufferCacheWhenSettingGLWriteFramebuffer) {
  // Doesn't apply to platforms with no GL_DRAW_FRAMEBUFFER support
  if (context_->deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    GLuint framebufferIds[2];
    context_->genFramebuffers(2, framebufferIds);

    context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferIds[0]);
    context_->bindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferIds[1]);

    // This should result in a call to OpenGL because the cache should have
    // been reset
    context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferIds[0]);

    GLint retrievedFramebuffer = -1;
    context_->getIntegerv(GL_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
    ASSERT_EQ(framebufferIds[0], retrievedFramebuffer);

    // Clean up
    context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
    context_->deleteFramebuffers(2, framebufferIds);
  }
}

/// This test is to make sure that we properly invalidate the GL_FRAMEBUFFER
/// binding point when the GL_READ_FRAMEBUFFER binding points is used
TEST_F(ContextOGLTest, StateCacheInvalidateFramebufferCacheWhenSettingGLReadFramebuffer) {
  // Doesn't apply to platforms with no GL_READ_FRAMEBUFFER support
  if (context_->deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    GLuint framebufferIds[2];
    context_->genFramebuffers(2, framebufferIds);

    context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferIds[0]);
    context_->bindFramebuffer(GL_READ_FRAMEBUFFER, framebufferIds[1]);

    // This should result in a call to OpenGL because the cache should have
    // been reset
    context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferIds[0]);

    GLint retrievedFramebuffer = -1;
    context_->getIntegerv(GL_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
    ASSERT_EQ(framebufferIds[0], retrievedFramebuffer);

    // Clean up
    context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
    context_->deleteFramebuffers(2, framebufferIds);
  }
}

/// This test is a sanity check that we should not have a GL error out of
/// the blue.
TEST_F(ContextOGLTest, CheckForErrorsNoError) {
  const GLenum ret = context_->checkForErrors(DUMMY_FILE_NAME, DUMMY_LINE_NUM);
  ASSERT_EQ(ret, GL_NO_ERROR);
}

TEST_F(ContextOGLTest, CheckForErrorsInvalidEnum) {
  // GL_INVALID_ENUM
  context_->activeTexture(GL_SRC_ALPHA);

  const GLenum ret = context_->checkForErrors(DUMMY_FILE_NAME, DUMMY_LINE_NUM);
  ASSERT_EQ(ret, GL_INVALID_ENUM);
}

#ifndef GL_UNSIGNED_SHORT_4_4_4_4
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#endif
/// This test purposely triggers the Invalid Operation error and check
/// to see that the right error code is returned.
TEST_F(ContextOGLTest, CheckForErrorsInvalidOperation) {
  GLuint textureMap;
  context_->genTextures(1, &textureMap);
  context_->bindTexture(GL_TEXTURE_2D, textureMap);
  context_->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  context_->bindTexture(GL_TEXTURE_2D, 0);

  GLuint framebufferId;
  context_->genFramebuffers(1, &framebufferId);

  context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferId);
  context_->framebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureMap, 0);

  GLint retrievedFramebuffer = -1;
  context_->getIntegerv(GL_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
  ASSERT_EQ(framebufferId, retrievedFramebuffer);

  // GL_INVALID_OPERATION is generated if type is GL_UNSIGNED_SHORT_4_4_4_4 and format is not
  // GL_RGBA.
  char data[100];
  context_->readPixels(1, 1, 1, 1, GL_RED, GL_UNSIGNED_SHORT_4_4_4_4, data);

  const GLenum ret = context_->checkForErrors(DUMMY_FILE_NAME, DUMMY_LINE_NUM);
  ASSERT_EQ(ret, GL_INVALID_OPERATION);

  // Clean up
  context_->bindTexture(GL_TEXTURE_2D, textureMap);
  std::vector<GLuint> textures;
  textures.push_back(textureMap);
  context_->deleteTextures(textures);

  context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
  context_->deleteFramebuffers(1, &framebufferId);
}

/// This test purposely triggers the Invalid Value error and check
/// to see that the right error code is returned.
TEST_F(ContextOGLTest, CheckForErrorsInvalidValue) {
  GLuint textureMap;
  context_->genTextures(1, &textureMap);
  context_->bindTexture(GL_TEXTURE_2D, textureMap);
  context_->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  context_->bindTexture(GL_TEXTURE_2D, 0);

  GLuint framebufferId;
  context_->genFramebuffers(1, &framebufferId);

  context_->bindFramebuffer(GL_FRAMEBUFFER, framebufferId);
  context_->framebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureMap, 0);

  GLint retrievedFramebuffer = -1;
  context_->getIntegerv(GL_FRAMEBUFFER_BINDING, &retrievedFramebuffer);
  ASSERT_EQ(framebufferId, retrievedFramebuffer);
  // GL_INVALID_VALUE is generated if any bit other than the eligible bits is set in mask.
  context_->clear(0XFFFFFFFF);

  const GLenum ret = context_->checkForErrors(DUMMY_FILE_NAME, DUMMY_LINE_NUM);
  ASSERT_EQ(ret, GL_INVALID_VALUE);

  // Clean up
  context_->bindTexture(GL_TEXTURE_2D, textureMap);
  std::vector<GLuint> textures;
  textures.push_back(textureMap);
  context_->deleteTextures(textures);

  context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
  context_->deleteFramebuffers(1, &framebufferId);
}

/// This test purposely triggers the Invalid Framebuffer error and check
/// to see that the right error code is returned.
TEST_F(ContextOGLTest, CheckForErrorsInvalidFrameBufferOperation) {
  unsigned int frameBuffer;
  context_->genFramebuffers(1, &frameBuffer);
  context_->bindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

  // Make sure frame buffer is not complete yet, so glClear generates
  // GL_INVALID_FRAMEBUFFER_OPERATION.
  if (context_->checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    context_->clear(GL_COLOR_BUFFER_BIT);

    const GLenum ret = context_->checkForErrors(DUMMY_FILE_NAME, DUMMY_LINE_NUM);
    ASSERT_EQ(ret, GL_INVALID_FRAMEBUFFER_OPERATION);
  }

  // Clean up
  context_->bindFramebuffer(GL_FRAMEBUFFER, 0);
  context_->deleteFramebuffers(1, &frameBuffer);
}

/// Verify that an object is visible across contexts in the same sharegroup
TEST_F(ContextOGLTest, BasicSharedContexts) {
#if IGL_PLATFORM_WIN && !IGL_ANGLE
  GTEST_SKIP() << "Context sharing not implemented in opengl::wgl";
#endif
  // Setup is three contexts, (1) and (2) part of the same sharegroup and (3) not.
  igl::Result result;
  auto sharedContext = context_->createShareContext(&result);
  ASSERT_TRUE(result.isOk());

  auto unsharedDevice = util::createTestDevice();
  ASSERT_TRUE(unsharedDevice != nullptr);
  auto* unsharedContext = &static_cast<opengl::Device&>(*unsharedDevice).getContext();
  ASSERT_TRUE(unsharedDevice != nullptr);

  // Create texture from context (1)
  context_->setCurrent();

  ASSERT_TRUE(context_->isCurrentContext());
  ASSERT_FALSE(sharedContext->isCurrentContext());
  ASSERT_FALSE(unsharedContext->isCurrentContext());

  ASSERT_TRUE(context_->isCurrentSharegroup());
  ASSERT_TRUE(sharedContext->isCurrentSharegroup());
  ASSERT_FALSE(unsharedContext->isCurrentSharegroup());

  const igl::TextureDesc textureDesc = igl::TextureDesc::new2D(
      igl::TextureFormat::RGBA_UNorm8, 16, 16, igl::TextureDesc::TextureUsageBits::Sampled);
  auto texture = device_->createTexture(textureDesc, &result);
  ASSERT_TRUE(result.isOk());

  const uint64_t glTextureId = static_cast<opengl::Texture*>(texture.get())->getId();
  context_->flush(); // Required for texture to be visible from other contexts

  // Confirm that texture is visible from context (2)
  sharedContext->setCurrent();

  ASSERT_FALSE(context_->isCurrentContext());
  ASSERT_TRUE(sharedContext->isCurrentContext());
  ASSERT_FALSE(unsharedContext->isCurrentContext());

  ASSERT_TRUE(context_->isCurrentSharegroup());
  ASSERT_TRUE(sharedContext->isCurrentSharegroup());
  ASSERT_FALSE(unsharedContext->isCurrentSharegroup());

  ASSERT_TRUE(sharedContext->isTexture(glTextureId));

  // Confirm that texture is not visible from context (3)
  unsharedContext->setCurrent();

  ASSERT_FALSE(context_->isCurrentContext());
  ASSERT_FALSE(sharedContext->isCurrentContext());
  ASSERT_TRUE(unsharedContext->isCurrentContext());

  ASSERT_FALSE(context_->isCurrentSharegroup());
  ASSERT_FALSE(sharedContext->isCurrentSharegroup());
  ASSERT_TRUE(unsharedContext->isCurrentSharegroup());

  ASSERT_FALSE(unsharedContext->isTexture(glTextureId));
}

} // namespace igl::tests
