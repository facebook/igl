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

TEST(CommandQueueStatisticsTest, DefaultConstructionZero) {
  const CommandQueueStatistics stats;
  EXPECT_EQ(stats.currentDrawCount, 0u);
  EXPECT_EQ(stats.lastFrameDrawCount, 0u);
}

} // namespace igl::tests
