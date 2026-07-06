/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/CommandQueue.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// CommandQueueStatistics
// ---------------------------------------------------------------------------

TEST(CommandQueueStatisticsTest, DefaultValues) {
  const CommandQueueStatistics stats;
  EXPECT_EQ(stats.currentDrawCount, 0u);
  EXPECT_EQ(stats.lastFrameDrawCount, 0u);
}

TEST(CommandQueueStatisticsTest, MutateCurrentDrawCount) {
  CommandQueueStatistics stats;
  stats.currentDrawCount = 42u;
  EXPECT_EQ(stats.currentDrawCount, 42u);
  EXPECT_EQ(stats.lastFrameDrawCount, 0u);
}

TEST(CommandQueueStatisticsTest, MutateBothFields) {
  CommandQueueStatistics stats;
  stats.currentDrawCount = 10u;
  stats.lastFrameDrawCount = 5u;
  EXPECT_EQ(stats.currentDrawCount, 10u);
  EXPECT_EQ(stats.lastFrameDrawCount, 5u);
}

// ---------------------------------------------------------------------------
// CommandQueueDesc
// ---------------------------------------------------------------------------

TEST(CommandQueueDescTest, DefaultConstruction) {
  const CommandQueueDesc desc;
  (void)desc;
}

// ---------------------------------------------------------------------------
// ICommandQueue — endFrame / getLastFrameDrawCount via concrete test subclass
// ---------------------------------------------------------------------------

class TestCommandQueue : public ICommandQueue {
 public:
  std::shared_ptr<ICommandBuffer> createCommandBuffer(
      [[maybe_unused]] const CommandBufferDesc& desc,
      [[maybe_unused]] Result* outResult) override {
    return nullptr;
  }

  SubmitHandle submit([[maybe_unused]] const ICommandBuffer& commandBuffer,
                      [[maybe_unused]] bool endOfFrame) override {
    return 0;
  }

  void addDrawCalls(uint32_t count) {
    incrementDrawCount(count);
  }
};

TEST(CommandQueueEndFrameTest, InitialLastFrameDrawCountIsZero) {
  const TestCommandQueue queue;
  EXPECT_EQ(queue.getLastFrameDrawCount(), 0u);
}

TEST(CommandQueueEndFrameTest, EndFrameWithNoDraws) {
  TestCommandQueue queue;
  queue.endFrame();
  EXPECT_EQ(queue.getLastFrameDrawCount(), 0u);
}

TEST(CommandQueueEndFrameTest, EndFrameTransfersDrawCount) {
  TestCommandQueue queue;
  queue.addDrawCalls(7);
  queue.endFrame();
  EXPECT_EQ(queue.getLastFrameDrawCount(), 7u);
}

TEST(CommandQueueEndFrameTest, EndFrameResetsCurrentDrawCount) {
  TestCommandQueue queue;
  queue.addDrawCalls(5);
  queue.endFrame();
  queue.endFrame();
  EXPECT_EQ(queue.getLastFrameDrawCount(), 0u);
}

TEST(CommandQueueEndFrameTest, MultipleFramesAccumulate) {
  TestCommandQueue queue;
  queue.addDrawCalls(3);
  queue.endFrame();
  EXPECT_EQ(queue.getLastFrameDrawCount(), 3u);

  queue.addDrawCalls(10);
  queue.endFrame();
  EXPECT_EQ(queue.getLastFrameDrawCount(), 10u);

  queue.addDrawCalls(1);
  queue.addDrawCalls(2);
  queue.endFrame();
  EXPECT_EQ(queue.getLastFrameDrawCount(), 3u);
}

TEST(CommandQueueEndFrameTest, IncrementDrawCountAccumulates) {
  TestCommandQueue queue;
  queue.addDrawCalls(2);
  queue.addDrawCalls(3);
  queue.addDrawCalls(5);
  queue.endFrame();
  EXPECT_EQ(queue.getLastFrameDrawCount(), 10u);
}

} // namespace igl::tests
