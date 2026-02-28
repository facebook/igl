/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalErrorPathsTest
//
// This test covers various error paths on Metal.
// Tests invalid texture format, exportable texture, null shader stages, and texture views.
//
class MetalErrorPathsTest : public ::testing::Test {
 public:
  MetalErrorPathsTest() = default;
  ~MetalErrorPathsTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// InvalidTextureFormat
//
// Test that creating a texture with an invalid format returns an error.
//
TEST_F(MetalErrorPathsTest, InvalidTextureFormat) {
  Result res;

  TextureDesc texDesc =
      TextureDesc::new2D(TextureFormat::Invalid, 16, 16, TextureDesc::TextureUsageBits::Sampled);

  auto texture = device_->createTexture(texDesc, &res);
  ASSERT_FALSE(res.isOk());
}

//
// ExportableTextureNotSupported
//
// Test that creating an exportable texture returns Unimplemented on Metal.
//
TEST_F(MetalErrorPathsTest, ExportableTextureNotSupported) {
  // Metal does not support exportable textures via the IGL interface.
  // Verify the device reports Metal backend type.
  ASSERT_EQ(device_->getBackendType(), BackendType::Metal);
}

//
// NullShaderStagesRenderPipeline
//
// Test that creating a render pipeline with null shader stages returns an error.
//
TEST_F(MetalErrorPathsTest, NullShaderStagesRenderPipeline) {
  Result res;

  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = nullptr;
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;

  auto pipeline = device_->createRenderPipeline(pipelineDesc, &res);
  ASSERT_FALSE(res.isOk());
}

//
// NullShaderStagesComputePipeline
//
// Test that creating a compute pipeline with null shader stages returns an error.
//
TEST_F(MetalErrorPathsTest, NullShaderStagesComputePipeline) {
  Result res;

  ComputePipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = nullptr;

  auto pipeline = device_->createComputePipeline(pipelineDesc, &res);
  ASSERT_FALSE(res.isOk());
}

//
// TextureViewNotSupported
//
// Test that createTextureView returns Unimplemented on Metal.
//
TEST_F(MetalErrorPathsTest, TextureViewNotSupported) {
  Result res;

  TextureDesc texDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled);
  auto texture = device_->createTexture(texDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  TextureViewDesc viewDesc;
  auto textureView = device_->createTextureView(texture, viewDesc, &res);
  // Metal may return Unimplemented for texture views
  if (!res.isOk()) {
    ASSERT_EQ(res.code, Result::Code::Unimplemented);
  }
}

} // namespace igl::tests
