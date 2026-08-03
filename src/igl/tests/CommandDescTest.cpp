/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <type_traits>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/Framebuffer.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// CommandBufferDesc
// ---------------------------------------------------------------------------

TEST(CommandBufferDescTest, DefaultConstruction) {
  const CommandBufferDesc desc;
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.timer, nullptr);
  EXPECT_EQ(desc.timestampQueries, nullptr);
}

TEST(CommandBufferDescTest, DebugName) {
  CommandBufferDesc desc;
  desc.debugName = "mainPass";
  EXPECT_EQ(desc.debugName, "mainPass");
}

// ---------------------------------------------------------------------------
// CommandBufferStatistics
// ---------------------------------------------------------------------------

TEST(CommandBufferStatisticsTest, DefaultConstruction) {
  const CommandBufferStatistics stats;
  EXPECT_EQ(stats.currentDrawCount, 0u);
}

TEST(CommandBufferStatisticsTest, IncrementDrawCount) {
  CommandBufferStatistics stats;
  ++stats.currentDrawCount;
  ++stats.currentDrawCount;
  EXPECT_EQ(stats.currentDrawCount, 2u);
}

// ---------------------------------------------------------------------------
// CommandQueueDesc
// ---------------------------------------------------------------------------

TEST(CommandDescTestCommandQueueDesc, DefaultConstruction) {
  const CommandQueueDesc desc;
  (void)desc;
}

// ---------------------------------------------------------------------------
// CommandQueueStatistics
// ---------------------------------------------------------------------------

TEST(CommandQueueStatisticsDescTest, DefaultConstruction) {
  const CommandQueueStatistics stats;
  EXPECT_EQ(stats.currentDrawCount, 0u);
  EXPECT_EQ(stats.lastFrameDrawCount, 0u);
}

TEST(CommandQueueStatisticsDescTest, TrackDrawCounts) {
  CommandQueueStatistics stats;
  stats.currentDrawCount = 100;
  stats.lastFrameDrawCount = 80;
  EXPECT_EQ(stats.currentDrawCount, 100u);
  EXPECT_EQ(stats.lastFrameDrawCount, 80u);
}

// ---------------------------------------------------------------------------
// FramebufferMode
// ---------------------------------------------------------------------------

TEST(CommandDescTestFramebufferMode, EnumValues) {
  EXPECT_NE(FramebufferMode::Mono, FramebufferMode::Stereo);
  EXPECT_NE(FramebufferMode::Stereo, FramebufferMode::Multiview);
  EXPECT_NE(FramebufferMode::Mono, FramebufferMode::Multiview);
}

// ---------------------------------------------------------------------------
// FramebufferDesc
// ---------------------------------------------------------------------------

TEST(FramebufferDescTest, DefaultConstruction) {
  const FramebufferDesc desc;
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.mode, FramebufferMode::Mono);
  EXPECT_EQ(desc.depthAttachment.texture, nullptr);
  EXPECT_EQ(desc.depthAttachment.resolveTexture, nullptr);
  EXPECT_EQ(desc.stencilAttachment.texture, nullptr);
  EXPECT_EQ(desc.stencilAttachment.resolveTexture, nullptr);
}

TEST(FramebufferDescTest, ColorAttachmentsDefaultNull) {
  const FramebufferDesc desc;
  for (const auto& colorAttachment : desc.colorAttachments) {
    EXPECT_EQ(colorAttachment.texture, nullptr);
    EXPECT_EQ(colorAttachment.resolveTexture, nullptr);
  }
}

TEST(FramebufferDescTest, DebugNameAndMode) {
  FramebufferDesc desc;
  desc.debugName = "offscreen";
  desc.mode = FramebufferMode::Stereo;
  EXPECT_EQ(desc.debugName, "offscreen");
  EXPECT_EQ(desc.mode, FramebufferMode::Stereo);
}

// ---------------------------------------------------------------------------
// SubmitHandle
// ---------------------------------------------------------------------------

TEST(CommandDescTestSubmitHandle, IsUint64) {
  static_assert(std::is_same_v<SubmitHandle, uint64_t>, "SubmitHandle must be uint64_t");
  const SubmitHandle handle = 0;
  EXPECT_EQ(handle, 0u);
  const SubmitHandle maxHandle = UINT64_MAX;
  EXPECT_EQ(maxHandle, UINT64_MAX);
}

} // namespace igl::tests
