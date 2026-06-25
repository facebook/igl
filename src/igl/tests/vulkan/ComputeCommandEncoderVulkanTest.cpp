/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../data/ShaderData.h"
#include "../util/TestDevice.h"

#include <cstring>
#include <vector>
#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/ComputePipelineState.h>
#include <igl/Device.h>
#include <igl/ShaderCreator.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class ComputeCommandEncoderVulkanTest : public ::testing::Test {
 public:
  ComputeCommandEncoderVulkanTest() = default;
  ~ComputeCommandEncoderVulkanTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";

    Result ret;
    cmdQueue_ = iglDev_->createCommandQueue(CommandQueueDesc{}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_NE(cmdQueue_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(ComputeCommandEncoderVulkanTest, CreateAndEndEncoding) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  auto encoder = cmdBuf->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

TEST_F(ComputeCommandEncoderVulkanTest, BindPushConstants) {
  // NOTE: bindPushConstants requires a compute pipeline to be bound first.
  // Since creating a compute pipeline requires a compute shader, and this is
  // just testing the encoder API, we skip this test. The push constants
  // functionality is tested through integration tests that have full pipelines.
  GTEST_SKIP() << "bindPushConstants requires a bound compute pipeline with a shader";
}

TEST_F(ComputeCommandEncoderVulkanTest, DebugGroupLabels) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  auto encoder = cmdBuf->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  encoder->pushDebugGroupLabel("ComputeGroup", Color(0.0f, 1.0f, 0.0f, 1.0f));
  encoder->popDebugGroupLabel();

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
}

TEST_F(ComputeCommandEncoderVulkanTest, DispatchThreadGroupsIndirect) {
  Result ret;

  const std::string computeSource(data::shader::kVulkanSimpleComputeShader);
  auto computeModule =
      ShaderModuleCreator::fromStringInput(*iglDev_,
                                           computeSource.c_str(),
                                           {.stage = ShaderStage::Compute, .entryPoint = "main"},
                                           "TestIndirectDispatch",
                                           &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto stages = ShaderStagesCreator::fromComputeModule(*iglDev_, computeModule, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  const ComputePipelineDesc pipelineDesc{
      .buffersMap =
          {
              {data::shader::kSimpleComputeInputIndex,
               IGL_NAMEHANDLE(data::shader::kSimpleComputeInput)},
              {data::shader::kSimpleComputeOutputIndex,
               IGL_NAMEHANDLE(data::shader::kSimpleComputeOutput)},
          },
      .shaderStages = std::move(stages),
      .debugName = "TestIndirectDispatch",
  };

  auto pipeline = iglDev_->createComputePipeline(pipelineDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(pipeline, nullptr);

  const std::vector<float> inputData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
  const BufferDesc inputBufferDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .data = inputData.data(),
      .length = inputData.size() * sizeof(float),
      .storage = ResourceStorage::Shared,
  };
  auto inputBuffer = iglDev_->createBuffer(inputBufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  const BufferDesc outputBufferDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .length = inputData.size() * sizeof(float),
      .storage = ResourceStorage::Shared,
  };
  auto outputBuffer = iglDev_->createBuffer(outputBufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  struct DispatchIndirectArgs {
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
  };
  const DispatchIndirectArgs indirectArgs{.groupCountX = 1, .groupCountY = 1, .groupCountZ = 1};
  const BufferDesc indirectBufferDesc{
      .type = BufferDesc::BufferTypeBits::Indirect,
      .data = &indirectArgs,
      .length = sizeof(indirectArgs),
      .storage = ResourceStorage::Shared,
  };
  auto indirectBuffer = iglDev_->createBuffer(indirectBufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto cmdBuffer = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto computeEncoder = cmdBuffer->createComputeCommandEncoder();
  ASSERT_NE(computeEncoder, nullptr);

  computeEncoder->bindComputePipelineState(pipeline);
  computeEncoder->bindBuffer(data::shader::kSimpleComputeInputIndex, inputBuffer.get());
  computeEncoder->bindBuffer(data::shader::kSimpleComputeOutputIndex, outputBuffer.get());

  Dependencies deps{};
  deps.buffers[0] = indirectBuffer.get();
  computeEncoder->dispatchThreadGroupsIndirect(*indirectBuffer, 0, Dimensions(6, 1, 1), deps);
  computeEncoder->endEncoding();

  cmdQueue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  const auto range = BufferRange(inputData.size() * sizeof(float), 0);
  auto* data = outputBuffer->map(range, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(data, nullptr);

  std::vector<float> outputData(inputData.size());
  std::memcpy(outputData.data(), data, inputData.size() * sizeof(float));
  outputBuffer->unmap();

  for (size_t i = 0; i < inputData.size(); i++) {
    EXPECT_FLOAT_EQ(outputData[i], inputData[i] * 2.0f);
  }
}

TEST_F(ComputeCommandEncoderVulkanTest, DirectDispatch) {
  Result ret;

  const std::string computeSource(data::shader::kVulkanSimpleComputeShader);
  auto computeModule =
      ShaderModuleCreator::fromStringInput(*iglDev_,
                                           computeSource.c_str(),
                                           {.stage = ShaderStage::Compute, .entryPoint = "main"},
                                           "TestDirectDispatch",
                                           &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto stages = ShaderStagesCreator::fromComputeModule(*iglDev_, computeModule, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  const ComputePipelineDesc pipelineDesc{
      .buffersMap =
          {
              {data::shader::kSimpleComputeInputIndex,
               IGL_NAMEHANDLE(data::shader::kSimpleComputeInput)},
              {data::shader::kSimpleComputeOutputIndex,
               IGL_NAMEHANDLE(data::shader::kSimpleComputeOutput)},
          },
      .shaderStages = std::move(stages),
      .debugName = "TestDirectDispatch",
  };

  auto pipeline = iglDev_->createComputePipeline(pipelineDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(pipeline, nullptr);

  const std::vector<float> inputData = {10.0f, 20.0f, 30.0f, 40.0f};
  const BufferDesc inputBufferDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .data = inputData.data(),
      .length = inputData.size() * sizeof(float),
      .storage = ResourceStorage::Shared,
  };
  auto inputBuffer = iglDev_->createBuffer(inputBufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  const BufferDesc outputBufferDesc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .length = inputData.size() * sizeof(float),
      .storage = ResourceStorage::Shared,
  };
  auto outputBuffer = iglDev_->createBuffer(outputBufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto cmdBuffer = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  auto computeEncoder = cmdBuffer->createComputeCommandEncoder();
  ASSERT_NE(computeEncoder, nullptr);

  computeEncoder->bindComputePipelineState(pipeline);
  computeEncoder->bindBuffer(data::shader::kSimpleComputeInputIndex, inputBuffer.get());
  computeEncoder->bindBuffer(data::shader::kSimpleComputeOutputIndex, outputBuffer.get());

  computeEncoder->dispatchThreadGroups(Dimensions(1, 1, 1), Dimensions(4, 1, 1));
  computeEncoder->endEncoding();

  cmdQueue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();

  const auto range = BufferRange(inputData.size() * sizeof(float), 0);
  auto* data = outputBuffer->map(range, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(data, nullptr);

  std::vector<float> outputData(inputData.size());
  std::memcpy(outputData.data(), data, inputData.size() * sizeof(float));
  outputBuffer->unmap();

  for (size_t i = 0; i < inputData.size(); i++) {
    EXPECT_FLOAT_EQ(outputData[i], inputData[i] * 2.0f);
  }
}

TEST_F(ComputeCommandEncoderVulkanTest, NestedDebugGroupLabels) {
  Result ret;
  auto cmdBuf = cmdQueue_->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());

  auto encoder = cmdBuf->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  encoder->pushDebugGroupLabel("Outer", Color(1.0f, 0.0f, 0.0f, 1.0f));
  encoder->pushDebugGroupLabel("Inner", Color(0.0f, 1.0f, 0.0f, 1.0f));
  encoder->popDebugGroupLabel();
  encoder->popDebugGroupLabel();

  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
