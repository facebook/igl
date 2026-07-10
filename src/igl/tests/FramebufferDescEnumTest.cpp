/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Framebuffer.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// FramebufferMode
// ---------------------------------------------------------------------------

TEST(FramebufferModeEnumTest, ValuesAreDistinct) {
  // FramebufferMode ordinals are language-assigned declaration order, not an API
  // contract; verify the enumerators are distinct rather than pinning integer values.
  EXPECT_NE(FramebufferMode::Mono, FramebufferMode::Stereo);
  EXPECT_NE(FramebufferMode::Mono, FramebufferMode::Multiview);
  EXPECT_NE(FramebufferMode::Stereo, FramebufferMode::Multiview);
}

// ---------------------------------------------------------------------------
// FramebufferDesc — default construction
// ---------------------------------------------------------------------------

TEST(FramebufferDescDefaultTest, DefaultValues) {
  const FramebufferDesc desc;
  EXPECT_EQ(desc.mode, FramebufferMode::Mono);
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.depthAttachment.texture, nullptr);
  EXPECT_EQ(desc.depthAttachment.resolveTexture, nullptr);
  EXPECT_EQ(desc.stencilAttachment.texture, nullptr);
  EXPECT_EQ(desc.stencilAttachment.resolveTexture, nullptr);
}

TEST(FramebufferDescDefaultTest, ColorAttachmentsAreNull) {
  const FramebufferDesc desc;
  for (const auto& colorAttachment : desc.colorAttachments) {
    EXPECT_EQ(colorAttachment.texture, nullptr);
    EXPECT_EQ(colorAttachment.resolveTexture, nullptr);
  }
}

} // namespace igl::tests
