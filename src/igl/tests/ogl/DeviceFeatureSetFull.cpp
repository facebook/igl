/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/Common.h"

#include <igl/opengl/Device.h>
#include <igl/opengl/DeviceFeatureSet.h>
#include <igl/opengl/IContext.h>

namespace igl::tests {

//
// DeviceFeatureSetFullOGLTest
//
// Tests for the full DeviceFeatureSet API in OpenGL.
//
class DeviceFeatureSetFullOGLTest : public ::testing::Test {
 public:
  DeviceFeatureSetFullOGLTest() = default;
  ~DeviceFeatureSetFullOGLTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);

    context_ = &static_cast<opengl::Device&>(*iglDev_).getContext();
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  opengl::IContext* context_ = nullptr;
};

//
// HasFeature
//
// Verify that hasFeature returns a boolean without crashing for various features.
//
TEST_F(DeviceFeatureSetFullOGLTest, HasFeature) {
  const auto& features = context_->deviceFeatures();

  // These should all return a boolean without crashing
  bool compute = features.hasFeature(DeviceFeatures::Compute);
  bool multiview = features.hasFeature(DeviceFeatures::Multiview);
  bool texture3d = features.hasFeature(DeviceFeatures::Texture3D);
  bool msaa = features.hasFeature(DeviceFeatures::MultiSample);
  bool uniformBlocks = features.hasFeature(DeviceFeatures::UniformBlocks);
  bool storageBuffers = features.hasFeature(DeviceFeatures::StorageBuffers);
  bool timers = features.hasFeature(DeviceFeatures::Timers);
  bool drawInstanced = features.hasFeature(DeviceFeatures::DrawInstanced);
  bool bindUniform = features.hasFeature(DeviceFeatures::BindUniform);
  bool mrt = features.hasFeature(DeviceFeatures::MultipleRenderTargets);

  // Suppress unused variable warnings
  (void)compute;
  (void)multiview;
  (void)texture3d;
  (void)msaa;
  (void)uniformBlocks;
  (void)storageBuffers;
  (void)timers;
  (void)drawInstanced;
  (void)bindUniform;
  (void)mrt;

  // The test passes if no crash occurred
  SUCCEED();
}

//
// HasExtension
//
// Verify that hasExtension returns a boolean without crashing.
//
TEST_F(DeviceFeatureSetFullOGLTest, HasExtension) {
  const auto& features = context_->deviceFeatures();

  bool timerQuery = features.hasExtension(opengl::Extensions::TimerQuery);
  bool vao = features.hasExtension(opengl::Extensions::VertexArrayObject);
  bool fbBlit = features.hasExtension(opengl::Extensions::FramebufferBlit);
  bool mapBuf = features.hasExtension(opengl::Extensions::MapBuffer);
  bool depth24 = features.hasExtension(opengl::Extensions::Depth24);
  bool sync = features.hasExtension(opengl::Extensions::Sync);
  bool srgb = features.hasExtension(opengl::Extensions::Srgb);
  bool debug = features.hasExtension(opengl::Extensions::Debug);

  (void)timerQuery;
  (void)vao;
  (void)fbBlit;
  (void)mapBuf;
  (void)depth24;
  (void)sync;
  (void)srgb;
  (void)debug;

  SUCCEED();
}

//
// HasInternalFeature
//
// Verify that hasInternalFeature returns a boolean without crashing.
//
TEST_F(DeviceFeatureSetFullOGLTest, HasInternalFeature) {
  const auto& features = context_->deviceFeatures();

  bool fbBlit = features.hasInternalFeature(opengl::InternalFeatures::FramebufferBlit);
  bool fbObject = features.hasInternalFeature(opengl::InternalFeatures::FramebufferObject);
  bool vao = features.hasInternalFeature(opengl::InternalFeatures::VertexArrayObject);
  bool mapBuf = features.hasInternalFeature(opengl::InternalFeatures::MapBuffer);
  bool unmapBuf = features.hasInternalFeature(opengl::InternalFeatures::UnmapBuffer);
  bool texStorage = features.hasInternalFeature(opengl::InternalFeatures::TexStorage);
  bool sync = features.hasInternalFeature(opengl::InternalFeatures::Sync);
  bool debugMsg = features.hasInternalFeature(opengl::InternalFeatures::DebugMessage);
  bool invalidateFb = features.hasInternalFeature(opengl::InternalFeatures::InvalidateFramebuffer);
  bool polyFill = features.hasInternalFeature(opengl::InternalFeatures::PolygonFillMode);

  (void)fbBlit;
  (void)fbObject;
  (void)vao;
  (void)mapBuf;
  (void)unmapBuf;
  (void)texStorage;
  (void)sync;
  (void)debugMsg;
  (void)invalidateFb;
  (void)polyFill;

  SUCCEED();
}

//
// GetFeatureLimits_MaxTextureSize
//
// Verify getFeatureLimits returns a sensible value for MaxTextureSize.
//
TEST_F(DeviceFeatureSetFullOGLTest, GetFeatureLimits_MaxTextureSize) {
  size_t maxTextureSize = 0;
  bool hasLimit =
      iglDev_->getFeatureLimits(DeviceFeatureLimits::MaxTextureDimension1D2D, maxTextureSize);
  ASSERT_TRUE(hasLimit);
  // Max texture size should be at least 64 (even the smallest GL implementations support this)
  ASSERT_GE(maxTextureSize, 64u);
}

} // namespace igl::tests
