/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/metal/ComputeCommandEncoder.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/CommandBuffer.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalComputeCommandEncoderTest
//
// This test covers igl::metal::ComputeCommandEncoder.
// Tests creation and dispatch operations of Metal compute command encoders.
//
class MetalComputeCommandEncoderTest : public ::testing::Test {
 public:
  MetalComputeCommandEncoderTest() = default;
  ~MetalComputeCommandEncoderTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);

    Result res;
    const CommandBufferDesc desc;
    commandBuffer_ = cmdQueue_->createCommandBuffer(desc, &res);
    ASSERT_TRUE(res.isOk()) << res.message;
    ASSERT_NE(commandBuffer_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> commandBuffer_;
};

//
// CreateEncoder
//
// Test creating a compute command encoder from a command buffer.
//
TEST_F(MetalComputeCommandEncoderTest, CreateEncoder) {
  auto encoder = commandBuffer_->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  // MTLCommandEncoder must always call endEncoding before being released.
  encoder->endEncoding();
}

//
// DispatchNoError
//
// Test dispatching threadgroups on a compute command encoder without crash.
//
TEST_F(MetalComputeCommandEncoderTest, DispatchNoError) {
  Result res;

  // Create compute shader module
  const auto computeSource = std::string(data::shader::kMtlSimpleComputeShader);
  auto computeModule = device_->createShaderModule(
      ShaderModuleDesc::fromStringInput(
          computeSource.c_str(),
          {ShaderStage::Compute, std::string(data::shader::kSimpleComputeFunc)},
          "computeModule"),
      &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  auto stagesDesc = ShaderStagesDesc::fromComputeModule(std::move(computeModule));
  auto stages = device_->createShaderStages(stagesDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  ComputePipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(stages);
  auto pipeline = device_->createComputePipeline(pipelineDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(pipeline, nullptr);

  // Create input/output buffers
  constexpr size_t kNumElements = 6;
  float inputData[kNumElements] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  BufferDesc inputBufDesc(
      BufferDesc::BufferTypeBits::Storage, inputData, sizeof(inputData), ResourceStorage::Shared);
  auto inputBuffer = device_->createBuffer(inputBufDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  BufferDesc outputBufDesc(
      BufferDesc::BufferTypeBits::Storage, nullptr, sizeof(inputData), ResourceStorage::Shared);
  auto outputBuffer = device_->createBuffer(outputBufDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  auto encoder = commandBuffer_->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  encoder->bindComputePipelineState(pipeline);
  encoder->bindBuffer(0, inputBuffer.get(), 0, sizeof(inputData));
  encoder->bindBuffer(1, outputBuffer.get(), 0, sizeof(inputData));

  encoder->dispatchThreadGroups(
      Dimensions(1, 1, 1), Dimensions(kNumElements, 1, 1), Dependencies());

  encoder->endEncoding();
}

} // namespace igl::tests
