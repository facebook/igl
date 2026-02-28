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
// MetalRenderPipelineCreationTest
//
// This test covers creation of Metal render pipeline states.
// Tests various configurations including blending, null stages, and depth formats.
//
class MetalRenderPipelineCreationTest : public ::testing::Test {
 public:
  MetalRenderPipelineCreationTest() = default;
  ~MetalRenderPipelineCreationTest() override = default;

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
// CreateBasicPipeline
//
// Test creating a basic render pipeline with simple vertex and fragment shaders.
//
TEST_F(MetalRenderPipelineCreationTest, CreateBasicPipeline) {
  Result res;

  std::unique_ptr<IShaderStages> stages;
  util::createSimpleShaderStages(device_, stages);
  ASSERT_NE(stages, nullptr);

  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(stages);
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;

  auto pipeline = device_->createRenderPipeline(pipelineDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(pipeline, nullptr);
}

//
// CreatePipelineWithBlending
//
// Test creating a render pipeline with alpha blending enabled.
//
TEST_F(MetalRenderPipelineCreationTest, CreatePipelineWithBlending) {
  Result res;

  std::unique_ptr<IShaderStages> stages;
  util::createSimpleShaderStages(device_, stages);
  ASSERT_NE(stages, nullptr);

  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(stages);
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
  pipelineDesc.targetDesc.colorAttachments[0].blendEnabled = true;
  pipelineDesc.targetDesc.colorAttachments[0].srcRGBBlendFactor = BlendFactor::SrcAlpha;
  pipelineDesc.targetDesc.colorAttachments[0].dstRGBBlendFactor = BlendFactor::OneMinusSrcAlpha;
  pipelineDesc.targetDesc.colorAttachments[0].srcAlphaBlendFactor = BlendFactor::One;
  pipelineDesc.targetDesc.colorAttachments[0].dstAlphaBlendFactor = BlendFactor::Zero;

  auto pipeline = device_->createRenderPipeline(pipelineDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(pipeline, nullptr);
}

//
// CreatePipelineNullStages
//
// Test that creating a pipeline with null shader stages returns an error.
//
TEST_F(MetalRenderPipelineCreationTest, CreatePipelineNullStages) {
  Result res;

  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = nullptr;
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;

  auto pipeline = device_->createRenderPipeline(pipelineDesc, &res);
  ASSERT_FALSE(res.isOk());
}

//
// CreatePipelineWithDepthFormat
//
// Test creating a render pipeline with a depth attachment format.
//
TEST_F(MetalRenderPipelineCreationTest, CreatePipelineWithDepthFormat) {
  Result res;

  std::unique_ptr<IShaderStages> stages;
  util::createSimpleShaderStages(device_, stages);
  ASSERT_NE(stages, nullptr);

  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(stages);
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
  pipelineDesc.targetDesc.depthAttachmentFormat = TextureFormat::Z_UNorm32;

  auto pipeline = device_->createRenderPipeline(pipelineDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(pipeline, nullptr);
}

} // namespace igl::tests
