/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/CommandQueue.h>

#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl::tests {

class CommandQueueTest : public ::testing::Test {
 public:
  CommandQueueTest() = default;
  ~CommandQueueTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);

    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);
  }
  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  CommandBufferDesc cbDesc_ = {};
  std::shared_ptr<ICommandBuffer> cmdBuf_;
};

// Test Cases
TEST_F(CommandQueueTest, CreateCommandBuffer) {
  Result ret;
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);
}

TEST_F(CommandQueueTest, Submit) {
  Result ret;
  cmdBuf_ = cmdQueue_->createCommandBuffer(cbDesc_, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_TRUE(cmdBuf_ != nullptr);
  cmdQueue_->submit(*(cmdBuf_));
}
} // namespace igl::tests
