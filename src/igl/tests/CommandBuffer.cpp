/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/CommandBuffer.h>

#include "util/Common.h"

namespace igl::tests {

namespace {
const char* kDebugName = "CommandBufferTest";
}

//
// CommandBufferTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class CommandBufferTest : public ::testing::Test {
 public:
  CommandBufferTest() = default;
  ~CommandBufferTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    Result result;
    CommandBufferDesc desc{.debugName = kDebugName};
    cmdBuf_ = cmdQueue_->createCommandBuffer(desc, &result);
    ASSERT_EQ(result.code, Result::Code::Ok);
    ASSERT_TRUE(cmdBuf_ != nullptr);
  }

  void TearDown() override {}

  // Member variables
 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> cmdBuf_;
};

//
// Check pushDebugGroupLabel and popDebugGroupLabel
//
// These functions sadly don't do anything without IGL_API_LOG and IGL_DEBUG defined,
// this is only for coverage to smoke test.
//
TEST_F(CommandBufferTest, pushPopDebugGroupLabel) {
  // API Logging can't be tested in it's current state, so we just check that it worked.
  cmdBuf_->pushDebugGroupLabel("TEST");
  cmdBuf_->popDebugGroupLabel();
}

TEST_F(CommandBufferTest, debugName) {
  ASSERT_EQ(cmdBuf_->desc.debugName, kDebugName);
}

TEST_F(CommandBufferTest, SubmitEmptyCommandBuffer) {
  cmdQueue_->submit(*cmdBuf_);
}

TEST_F(CommandBufferTest, CreateMultipleCommandBuffers) {
  Result result;

  const CommandBufferDesc desc1{.debugName = "CmdBuf1"};
  auto cmdBuf1 = cmdQueue_->createCommandBuffer(desc1, &result);
  ASSERT_EQ(result.code, Result::Code::Ok);
  ASSERT_NE(cmdBuf1, nullptr);
  EXPECT_EQ(cmdBuf1->desc.debugName, "CmdBuf1");

  const CommandBufferDesc desc2{.debugName = "CmdBuf2"};
  auto cmdBuf2 = cmdQueue_->createCommandBuffer(desc2, &result);
  ASSERT_EQ(result.code, Result::Code::Ok);
  ASSERT_NE(cmdBuf2, nullptr);
  EXPECT_EQ(cmdBuf2->desc.debugName, "CmdBuf2");
}

} // namespace igl::tests
