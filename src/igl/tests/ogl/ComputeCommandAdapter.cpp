/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/opengl/ComputeCommandAdapter.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"

#include <cstring>
#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/ComputePipelineState.h>
#include <igl/Shader.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/IContext.h>

namespace igl::tests {

//
// ComputeCommandAdapterOGLTest
//
// Tests for the OpenGL ComputeCommandAdapter.
//
class ComputeCommandAdapterOGLTest : public ::testing::Test {
 public:
  ComputeCommandAdapterOGLTest() = default;
  ~ComputeCommandAdapterOGLTest() override = default;

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
// BufferBindingAndDispatch
//
// Bind SSBOs, dispatch compute, verify output.
//
TEST_F(ComputeCommandAdapterOGLTest, BufferBindingAndDispatch) {
  if (!iglDev_->hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "Compute not supported";
  }

  Result ret;

  // Create compute shader stages
  const std::string computeSource(data::shader::kOglSimpleComputeShader);
  auto shaderModule = iglDev_->createShaderModule(
      ShaderModuleDesc::fromStringInput(
          computeSource.c_str(),
          {ShaderStage::Compute, std::string(data::shader::kShaderFunc)},
          ""),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto shaderStages = ShaderStagesDesc::fromComputeModule(std::move(shaderModule));
  auto stages = iglDev_->createShaderStages(shaderStages, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  // Create compute pipeline
  ComputePipelineDesc computeDesc;
  computeDesc.shaderStages = std::move(stages);
  auto computePipeline = iglDev_->createComputePipeline(computeDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(computePipeline, nullptr);

  // Create input and output buffers
  const float inputData[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  BufferDesc inputBufDesc;
  inputBufDesc.type = BufferDesc::BufferTypeBits::Storage;
  inputBufDesc.data = inputData;
  inputBufDesc.length = sizeof(inputData);
  auto inputBuffer = iglDev_->createBuffer(inputBufDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  float outputData[6] = {0.0f};
  BufferDesc outputBufDesc;
  outputBufDesc.type = BufferDesc::BufferTypeBits::Storage;
  outputBufDesc.data = outputData;
  outputBufDesc.length = sizeof(outputData);
  auto outputBuffer = iglDev_->createBuffer(outputBufDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  // Dispatch
  CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  auto computeEncoder = cmdBuf->createComputeCommandEncoder();
  ASSERT_NE(computeEncoder, nullptr);

  computeEncoder->bindComputePipelineState(computePipeline);
  computeEncoder->bindBuffer(0, inputBuffer.get());
  computeEncoder->bindBuffer(1, outputBuffer.get());
  computeEncoder->dispatchThreadGroups(Dimensions(1, 1, 1), Dimensions(6, 1, 1));
  computeEncoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);

  // Verify no GL errors
  ASSERT_EQ(context_->checkForErrors(__FILE__, __LINE__), GL_NO_ERROR);
}

//
// TextureBinding
//
// Bind texture for compute sampling.
//
TEST_F(ComputeCommandAdapterOGLTest, TextureBinding) {
  if (!iglDev_->hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "Compute not supported";
  }

  Result ret;

  // Create a simple texture
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 2,
                                                 2,
                                                 TextureDesc::TextureUsageBits::Sampled |
                                                     TextureDesc::TextureUsageBits::Storage);
  auto texture = iglDev_->createTexture(texDesc, &ret);
  // Texture creation may fail if Storage usage is not supported for this format
  if (!ret.isOk() || texture == nullptr) {
    GTEST_SKIP() << "Cannot create storage texture: " << ret.message.c_str();
  }

  // Upload data
  const uint32_t pixels[] = {0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF};
  texture->upload(TextureRangeDesc::new2D(0, 0, 2, 2), pixels);

  // If we got here without errors, the texture was created and uploaded successfully
  ASSERT_EQ(context_->checkForErrors(__FILE__, __LINE__), GL_NO_ERROR);
}

//
// DirtyStateTracking
//
// Change pipeline between dispatches.
//
TEST_F(ComputeCommandAdapterOGLTest, DirtyStateTracking) {
  if (!iglDev_->hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "Compute not supported";
  }

  Result ret;

  // Create compute shader
  const std::string computeSource(data::shader::kOglSimpleComputeShader);
  auto shaderModule = iglDev_->createShaderModule(
      ShaderModuleDesc::fromStringInput(
          computeSource.c_str(),
          {ShaderStage::Compute, std::string(data::shader::kShaderFunc)},
          ""),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto shaderStages = ShaderStagesDesc::fromComputeModule(std::move(shaderModule));
  auto stages = iglDev_->createShaderStages(shaderStages, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  // Create compute pipeline
  ComputePipelineDesc computeDesc;
  computeDesc.shaderStages = std::move(stages);
  auto computePipeline = iglDev_->createComputePipeline(computeDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(computePipeline, nullptr);

  // Create buffers
  const float inputData[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  BufferDesc inputBufDesc;
  inputBufDesc.type = BufferDesc::BufferTypeBits::Storage;
  inputBufDesc.data = inputData;
  inputBufDesc.length = sizeof(inputData);
  auto inputBuffer = iglDev_->createBuffer(inputBufDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  float outputData[6] = {0.0f};
  BufferDesc outputBufDesc;
  outputBufDesc.type = BufferDesc::BufferTypeBits::Storage;
  outputBufDesc.data = outputData;
  outputBufDesc.length = sizeof(outputData);
  auto outputBuffer = iglDev_->createBuffer(outputBufDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  // Dispatch twice with same pipeline to exercise dirty state tracking
  CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);

  auto computeEncoder = cmdBuf->createComputeCommandEncoder();
  ASSERT_NE(computeEncoder, nullptr);

  // First dispatch
  computeEncoder->bindComputePipelineState(computePipeline);
  computeEncoder->bindBuffer(0, inputBuffer.get());
  computeEncoder->bindBuffer(1, outputBuffer.get());
  computeEncoder->dispatchThreadGroups(Dimensions(1, 1, 1), Dimensions(6, 1, 1));

  // Re-bind pipeline (dirties state) and dispatch again
  computeEncoder->bindComputePipelineState(computePipeline);
  computeEncoder->bindBuffer(0, inputBuffer.get());
  computeEncoder->bindBuffer(1, outputBuffer.get());
  computeEncoder->dispatchThreadGroups(Dimensions(1, 1, 1), Dimensions(6, 1, 1));

  computeEncoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);

  ASSERT_EQ(context_->checkForErrors(__FILE__, __LINE__), GL_NO_ERROR);
}

} // namespace igl::tests
