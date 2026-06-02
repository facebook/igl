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

#include <cstring>
#include <igl/Buffer.h>
#include <igl/IGL.h>
#include <igl/metal/CommandBuffer.h>

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
  auto computeModule =
      device_->createShaderModule(ShaderModuleDesc::fromStringInput(
                                      computeSource.c_str(),
                                      {.stage = ShaderStage::Compute,
                                       .entryPoint = std::string(data::shader::kSimpleComputeFunc)},
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

//
// DispatchIndirect
//
// Test dispatching via dispatchThreadGroupsIndirect() and verifying correct output.
//
TEST_F(MetalComputeCommandEncoderTest, DispatchIndirect) {
  Result res;

  const auto computeSource = std::string(data::shader::kMtlSimpleComputeShader);
  auto computeModule =
      device_->createShaderModule(ShaderModuleDesc::fromStringInput(
                                      computeSource.c_str(),
                                      {.stage = ShaderStage::Compute,
                                       .entryPoint = std::string(data::shader::kSimpleComputeFunc)},
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

  constexpr size_t kNumElements = 6;
  const float inputData[kNumElements] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  const BufferDesc inputBufDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .data = inputData,
      .length = sizeof(inputData),
      .storage = ResourceStorage::Shared,
  };
  auto inputBuffer = device_->createBuffer(inputBufDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  const BufferDesc outputBufDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .length = sizeof(inputData),
      .storage = ResourceStorage::Shared,
  };
  auto outputBuffer = device_->createBuffer(outputBufDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  struct DispatchIndirectArgs {
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
  };
  const DispatchIndirectArgs indirectArgs{.groupCountX = 1, .groupCountY = 1, .groupCountZ = 1};
  const BufferDesc indirectBufDesc{
      .type = BufferDesc::BufferTypeBits::Indirect,
      .data = &indirectArgs,
      .length = sizeof(indirectArgs),
      .storage = ResourceStorage::Shared,
  };
  auto indirectBuffer = device_->createBuffer(indirectBufDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  auto encoder = commandBuffer_->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  encoder->bindComputePipelineState(pipeline);
  encoder->bindBuffer(0, inputBuffer.get(), 0, sizeof(inputData));
  encoder->bindBuffer(1, outputBuffer.get(), 0, sizeof(inputData));

  encoder->dispatchThreadGroupsIndirect(
      *indirectBuffer, 0, Dimensions(kNumElements, 1, 1), Dependencies());

  encoder->endEncoding();

  cmdQueue_->submit(*commandBuffer_);
  commandBuffer_->waitUntilCompleted();

  auto* data = outputBuffer->map(BufferRange(sizeof(inputData), 0), &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(data, nullptr);

  float outputData[kNumElements];
  std::memcpy(outputData, data, sizeof(inputData));
  outputBuffer->unmap();

  for (size_t i = 0; i < kNumElements; i++) {
    EXPECT_FLOAT_EQ(outputData[i], inputData[i] * 2.0f);
  }
}

} // namespace igl::tests
