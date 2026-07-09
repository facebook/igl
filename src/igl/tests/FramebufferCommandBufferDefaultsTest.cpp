/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/CommandBuffer.h>
#include <igl/Framebuffer.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// FramebufferMode
// ---------------------------------------------------------------------------

TEST(FramebufferModeTest, ValuesAreDistinct) {
  // FramebufferMode ordinals are language-assigned declaration order, not an API
  // contract; verify the enumerators are distinct rather than pinning integer values.
  EXPECT_NE(FramebufferMode::Mono, FramebufferMode::Stereo);
  EXPECT_NE(FramebufferMode::Mono, FramebufferMode::Multiview);
  EXPECT_NE(FramebufferMode::Stereo, FramebufferMode::Multiview);
}

// ---------------------------------------------------------------------------
// FramebufferDesc
// ---------------------------------------------------------------------------

TEST(FramebufferDescTest, DefaultConstructionScalarFields) {
  const FramebufferDesc desc;
  EXPECT_EQ(desc.mode, FramebufferMode::Mono);
  EXPECT_TRUE(desc.debugName.empty());
}

TEST(FramebufferDescTest, DefaultConstructionAttachmentsNull) {
  const FramebufferDesc desc;
  for (const auto& attachment : desc.colorAttachments) {
    EXPECT_EQ(attachment.texture, nullptr);
    EXPECT_EQ(attachment.resolveTexture, nullptr);
  }
  EXPECT_EQ(desc.depthAttachment.texture, nullptr);
  EXPECT_EQ(desc.depthAttachment.resolveTexture, nullptr);
  EXPECT_EQ(desc.stencilAttachment.texture, nullptr);
  EXPECT_EQ(desc.stencilAttachment.resolveTexture, nullptr);
}

// ---------------------------------------------------------------------------
// CommandBufferDesc
// ---------------------------------------------------------------------------

TEST(CommandBufferDescTest, DefaultConstructionValues) {
  const CommandBufferDesc desc;
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.timer, nullptr);
  EXPECT_EQ(desc.timestampQueries, nullptr);
}

// ---------------------------------------------------------------------------
// CommandBufferStatistics
// ---------------------------------------------------------------------------

TEST(CommandBufferStatisticsTest, DefaultConstructionDrawCountZero) {
  const CommandBufferStatistics stats;
  EXPECT_EQ(stats.currentDrawCount, 0u);
}

} // namespace igl::tests
