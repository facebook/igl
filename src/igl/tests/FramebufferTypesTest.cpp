/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Framebuffer.h>

namespace igl::tests {

TEST(FramebufferModeTest, EnumValues) {
  EXPECT_EQ(static_cast<int>(FramebufferMode::Mono), 0);
  EXPECT_EQ(static_cast<int>(FramebufferMode::Stereo), 1);
  EXPECT_EQ(static_cast<int>(FramebufferMode::Multiview), 2);
}

TEST(FramebufferDescTest, DefaultMode) {
  const FramebufferDesc desc;
  EXPECT_EQ(desc.mode, FramebufferMode::Mono);
}

TEST(FramebufferDescTest, DefaultDebugNameEmpty) {
  const FramebufferDesc desc;
  EXPECT_TRUE(desc.debugName.empty());
}

TEST(FramebufferDescTest, DefaultColorAttachmentsNull) {
  const FramebufferDesc desc;
  for (const auto& attachment : desc.colorAttachments) {
    EXPECT_EQ(attachment.texture, nullptr);
    EXPECT_EQ(attachment.resolveTexture, nullptr);
  }
}

TEST(FramebufferDescTest, DefaultDepthAttachmentNull) {
  const FramebufferDesc desc;
  EXPECT_EQ(desc.depthAttachment.texture, nullptr);
  EXPECT_EQ(desc.depthAttachment.resolveTexture, nullptr);
}

TEST(FramebufferDescTest, DefaultStencilAttachmentNull) {
  const FramebufferDesc desc;
  EXPECT_EQ(desc.stencilAttachment.texture, nullptr);
  EXPECT_EQ(desc.stencilAttachment.resolveTexture, nullptr);
}

} // namespace igl::tests
