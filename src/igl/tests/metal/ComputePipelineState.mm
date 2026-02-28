/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/metal/ComputePipelineState.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalComputePipelineStateTest
//
// This test covers igl::metal::ComputePipelineState.
// Tests creation and error paths of Metal compute pipeline states.
//
class MetalComputePipelineStateTest : public ::testing::Test {
 public:
  MetalComputePipelineStateTest() = default;
  ~MetalComputePipelineStateTest() override = default;

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
// CreateComputePipeline
//
// Test creating a compute pipeline with a valid compute shader.
//
TEST_F(MetalComputePipelineStateTest, CreateComputePipeline) {
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
  ASSERT_NE(computeModule, nullptr);

  // Create shader stages
  auto stagesDesc = ShaderStagesDesc::fromComputeModule(std::move(computeModule));
  auto stages = device_->createShaderStages(stagesDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(stages, nullptr);

  // Create compute pipeline
  ComputePipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(stages);
  pipelineDesc.debugName = "testComputePipeline";

  auto pipeline = device_->createComputePipeline(pipelineDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(pipeline, nullptr);
}

//
// CreateComputePipelineNullStages
//
// Test that creating a compute pipeline with null shader stages returns an error.
//
TEST_F(MetalComputePipelineStateTest, CreateComputePipelineNullStages) {
  Result res;

  ComputePipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = nullptr;
  pipelineDesc.debugName = "testNullStages";

  auto pipeline = device_->createComputePipeline(pipelineDesc, &res);
  ASSERT_FALSE(res.isOk());
}

} // namespace igl::tests
