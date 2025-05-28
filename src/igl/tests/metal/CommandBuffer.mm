/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/CommandBuffer.h>

#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl::tests {

//
// CommandBufferMTLTest
//
// This test covers igl::metal::CommandBuffer.
// Most of the testing revolves confirming successful instantiation
// and creation of encoders.
//
class CommandBufferMTLTest : public ::testing::Test {
 public:
  CommandBufferMTLTest() = default;
  ~CommandBufferMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(device_, commandQueue_);

    // Test instantiation and constructor of CommandBuffer.
    Result res;
    const CommandBufferDesc desc;
    commandBuffer_ = commandQueue_->createCommandBuffer(desc, &res);
    ASSERT_TRUE(res.isOk());
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> commandQueue_;
  std::shared_ptr<ICommandBuffer> commandBuffer_;
};

//
// CommandBufferCreation
//
// Test successful creation of MTLCommandBuffer.
//
TEST_F(CommandBufferMTLTest, CommandBufferCreation) {
  auto mtlBuffer = static_cast<metal::CommandBuffer&>(*commandBuffer_).get();
  ASSERT_NE(mtlBuffer, nullptr);
}

//
// CreateComputeCommandEncoder
//
// Exercise the function createComputeCommandEncoder,
// test successful creation of IComputeCommandEncoder object.
//
TEST_F(CommandBufferMTLTest, CreateComputeCommandEncoder) {
  auto encoder = commandBuffer_->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);

  // MTLCommandEncoder must always call endEncoding before being released.
  encoder->endEncoding();
}

//
// CreateRenderCommandEncoderSimple
//
// Exercise the function createRenderCommandEncoder,
// test successful creation of IRenderCommandEncoder object.
//
TEST_F(CommandBufferMTLTest, CreateRenderCommandEncoderSimple) {
  Result res;

  const FramebufferDesc fbDesc;
  auto frameBuffer = device_->createFramebuffer(fbDesc, &res);
  ASSERT_TRUE(res.isOk());

  const RenderPassDesc rpDesc;
  auto encoder = commandBuffer_->createRenderCommandEncoder(rpDesc, frameBuffer);
  ASSERT_NE(encoder, nullptr);

  // MTLCommandEncoder must always call endEncoding before being released.
  encoder->endEncoding();
}
} // namespace igl::tests
