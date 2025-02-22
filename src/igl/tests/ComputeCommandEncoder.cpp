/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <functional>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <memory>
#include <utility>
#include <vector>

#include "data/ShaderData.h"
#include "igl/Buffer.h"
#include "util/Common.h"

namespace igl::tests {

static std::vector<float> dataIn = {1.0, 2.0, 3.0, 4.0, 5.0f, 6.0f};

/**
 * @brief ComputeCommandEncoderTest is a test fixture for all the tests in this file.
 * It takes care of common initialization and allocating of common resources.
 */
class ComputeCommandEncoderTest : public ::testing::Test {
 private:
 public:
  ComputeCommandEncoderTest() = default;
  ~ComputeCommandEncoderTest() override = default;

  /**
   * @brief This function sets up compute buffers and compiles the compute shader.
   */
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_TRUE(iglDev_ != nullptr);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    if (!iglDev_->hasFeature(DeviceFeatures::Compute)) {
      return;
    }

    const BufferDesc vbInDesc = BufferDesc(
        BufferDesc::BufferTypeBits::Storage, dataIn.data(), sizeof(float) * dataIn.size());
    bufferIn_ = iglDev_->createBuffer(vbInDesc, nullptr);
    ASSERT_TRUE(bufferIn_ != nullptr);
    const BufferDesc bufferOutDesc =
        BufferDesc(BufferDesc::BufferTypeBits::Storage, nullptr, sizeof(float) * dataIn.size());
    bufferOut0_ = iglDev_->createBuffer(bufferOutDesc, nullptr);
    ASSERT_TRUE(bufferOut0_ != nullptr);
    bufferOut1_ = iglDev_->createBuffer(bufferOutDesc, nullptr);
    ASSERT_TRUE(bufferOut1_ != nullptr);
    bufferOut2_ = iglDev_->createBuffer(bufferOutDesc, nullptr);
    ASSERT_TRUE(bufferOut2_ != nullptr);

    { // Compile CS
      const char* entryName = nullptr;
      const char* source = nullptr;
      if (iglDev_->getBackendType() == igl::BackendType::OpenGL) {
        source = igl::tests::data::shader::OGL_SIMPLE_COMPUTE_SHADER;
        entryName = igl::tests::data::shader::simpleComputeFunc;
      } else if (iglDev_->getBackendType() == igl::BackendType::Vulkan) {
        source = igl::tests::data::shader::VULKAN_SIMPLE_COMPUTE_SHADER;
        entryName = "main"; // entry point is not pipelined. Hardcoding to main for now.
      } else if (iglDev_->getBackendType() == igl::BackendType::Metal) {
        source = igl::tests::data::shader::MTL_SIMPLE_COMPUTE_SHADER;
        entryName = igl::tests::data::shader::simpleComputeFunc;
      } else {
        IGL_DEBUG_ASSERT_NOT_REACHED();
      }

      igl::Result ret;
      computeStages_ =
          ShaderStagesCreator::fromModuleStringInput(*iglDev_, source, entryName, "", &ret);
      ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
      ASSERT_TRUE(computeStages_ != nullptr);
    }
  }

  /**
   * @brief This function binds bufferIn and bufferOut to a new computePipelineState and encodes the
   * computePipelineState to a new computeCommandEncoder.
   */
  void encodeCompute(const std::shared_ptr<igl::ICommandBuffer>& cmdBuffer,
                     const std::shared_ptr<igl::IBuffer>& bufferIn,
                     const std::shared_ptr<igl::IBuffer>& bufferOut,
                     std::shared_ptr<IComputePipelineState>& ret) {
    ASSERT_TRUE(computeStages_ != nullptr);
    ComputePipelineDesc computeDesc;
    computeDesc.shaderStages = computeStages_;
    computeDesc.buffersMap[igl::tests::data::shader::simpleComputeInputIndex] =
        genNameHandle(igl::tests::data::shader::simpleComputeInput);
    computeDesc.buffersMap[igl::tests::data::shader::simpleComputeOutputIndex] =
        genNameHandle(igl::tests::data::shader::simpleComputeOutput);
    auto computePipelineState = iglDev_->createComputePipeline(computeDesc, nullptr);
    ASSERT_TRUE(computePipelineState != nullptr);

    auto computeEncoder = cmdBuffer->createComputeCommandEncoder();
    ASSERT_TRUE(computeEncoder != nullptr);

    computeEncoder->insertDebugEventLabel("Running ComputeCommandEncoderTest...");

    computeEncoder->bindComputePipelineState(computePipelineState);
    computeEncoder->bindBuffer(igl::tests::data::shader::simpleComputeInputIndex, bufferIn.get());
    computeEncoder->bindBuffer(igl::tests::data::shader::simpleComputeOutputIndex, bufferOut.get());

    const Dimensions threadgroupSize(dataIn.size(), 1, 1);
    const Dimensions threadgroupCount(1, 1, 1);
    computeEncoder->dispatchThreadGroups(
        threadgroupCount, threadgroupSize, {.buffers = {bufferIn.get()}});
    computeEncoder->endEncoding();
    ret = computePipelineState;
  }

  void TearDown() override {}

 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;

  std::shared_ptr<IShaderStages> computeStages_;

  std::shared_ptr<IBuffer> bufferIn_, bufferOut0_, bufferOut1_, bufferOut2_;
  std::shared_ptr<IComputePipelineState> cps1_, cps2_, cps3_;
};

TEST_F(ComputeCommandEncoderTest, canEncodeBasicBufferOperation) {
#if IGL_PLATFORM_LINUX && !IGL_PLATFORM_LINUX_USE_EGL
  GTEST_SKIP() << "Fix this test on Linux";
#endif
  if (!iglDev_->hasFeature(DeviceFeatures::Compute)) {
    return;
  }

  ASSERT_TRUE(cmdQueue_ != nullptr);

  const CommandBufferDesc cbDesc;
  auto cmdBuffer = cmdQueue_->createCommandBuffer(cbDesc, nullptr);
  ASSERT_TRUE(cmdBuffer != nullptr);

  encodeCompute(cmdBuffer, bufferIn_, bufferOut0_, cps1_);

  ASSERT_TRUE(cmdQueue_ != nullptr);
  cmdQueue_->submit(*cmdBuffer);

  cmdBuffer->waitUntilCompleted();

  std::vector<float> bytes(dataIn.size());
  auto range = BufferRange(sizeof(float) * dataIn.size(), 0);
  igl::Result ret;
  auto* data = bufferOut0_->map(range, &ret);
  ASSERT_TRUE(data != nullptr);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  memcpy(bytes.data(), data, sizeof(float) * dataIn.size());
  ASSERT_EQ(dataIn.size() > 0, true);
  for (int i = 0; i < dataIn.size(); i++) {
    ASSERT_EQ(dataIn[i] * 2.0f, bytes[i]);
  }
  bufferOut0_->unmap();
}

TEST_F(ComputeCommandEncoderTest, bindImageTexture) {
  if (!iglDev_->hasFeature(DeviceFeatures::Compute)) {
    return;
  }

  auto cmdBuffer = cmdQueue_->createCommandBuffer({}, nullptr);
  ASSERT_TRUE(cmdBuffer != nullptr);

  auto computeCommandEncoder = cmdBuffer->createComputeCommandEncoder();
  computeCommandEncoder->bindImageTexture(0, nullptr, TextureFormat::Invalid);
  cmdQueue_->submit(*cmdBuffer);
  cmdBuffer->waitUntilCompleted();
}

TEST_F(ComputeCommandEncoderTest, canUseOutputBufferFromOnePassAsInputToNext) {
#if IGL_PLATFORM_LINUX && !IGL_PLATFORM_LINUX_USE_EGL
  GTEST_SKIP() << "Fix this test on Linux";
#endif
  if (!iglDev_->hasFeature(DeviceFeatures::Compute)) {
    return;
  }

  ASSERT_TRUE(cmdQueue_ != nullptr);

  {
    const CommandBufferDesc cbDesc;
    auto cmdBuffer = cmdQueue_->createCommandBuffer(cbDesc, nullptr);
    ASSERT_TRUE(cmdBuffer != nullptr);

    encodeCompute(cmdBuffer, bufferIn_, bufferOut0_, cps1_);

    cmdQueue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
  }
  {
    const CommandBufferDesc cbDesc;
    auto cmdBuffer = cmdQueue_->createCommandBuffer(cbDesc, nullptr);
    ASSERT_TRUE(cmdBuffer != nullptr);

    encodeCompute(cmdBuffer, bufferOut0_, bufferOut1_, cps2_);
    cmdQueue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
  }
  {
    const CommandBufferDesc cbDesc;
    auto cmdBuffer = cmdQueue_->createCommandBuffer(cbDesc, nullptr);
    ASSERT_TRUE(cmdBuffer != nullptr);

    encodeCompute(cmdBuffer, bufferOut1_, bufferOut2_, cps3_);
    cmdQueue_->submit(*cmdBuffer);
    cmdBuffer->waitUntilCompleted();
  }

  std::vector<float> bytes(dataIn.size());
  auto range = BufferRange(sizeof(float) * dataIn.size(), 0);
  igl::Result ret;
  auto* data = bufferOut2_->map(range, &ret);
  ASSERT_TRUE(data != nullptr);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  memcpy(bytes.data(), data, sizeof(float) * dataIn.size());
  ASSERT_EQ(dataIn.size() > 0, true);
  for (int i = 0; i < dataIn.size(); i++) {
    // Compute pass ran 3 times
    ASSERT_EQ(dataIn[i] * 2.0f * 2.0f * 2.0f, bytes[i]);
  }
  bufferOut2_->unmap();
}

} // namespace igl::tests
