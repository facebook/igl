/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/RenderPass.h>

namespace igl::tests {

TEST(LoadActionTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(LoadAction::DontCare), 0u);
  EXPECT_EQ(static_cast<uint8_t>(LoadAction::Load), 1u);
  EXPECT_EQ(static_cast<uint8_t>(LoadAction::Clear), 2u);
}

TEST(StoreActionTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(StoreAction::DontCare), 0u);
  EXPECT_EQ(static_cast<uint8_t>(StoreAction::Store), 1u);
  EXPECT_EQ(static_cast<uint8_t>(StoreAction::MsaaResolve), 2u);
}

TEST(AttachmentDescTest, DefaultConstruction) {
  const RenderPassDesc::AttachmentDesc desc;
  EXPECT_EQ(desc.loadAction, LoadAction::DontCare);
  EXPECT_EQ(desc.storeAction, StoreAction::Store);
  EXPECT_EQ(desc.face, 0u);
  EXPECT_EQ(desc.mipLevel, 0u);
  EXPECT_EQ(desc.layer, 0u);
  EXPECT_FLOAT_EQ(desc.clearColor.r, 0.0f);
  EXPECT_FLOAT_EQ(desc.clearColor.g, 0.0f);
  EXPECT_FLOAT_EQ(desc.clearColor.b, 0.0f);
  EXPECT_FLOAT_EQ(desc.clearColor.a, 0.0f);
  EXPECT_FLOAT_EQ(desc.clearDepth, 1.0f);
  EXPECT_EQ(desc.clearStencil, 0u);
}

TEST(RenderPassDescTest, DefaultConstruction) {
  const RenderPassDesc desc;
  EXPECT_TRUE(desc.colorAttachments.empty());

  EXPECT_EQ(desc.depthAttachment.loadAction, LoadAction::Clear);
  EXPECT_EQ(desc.depthAttachment.storeAction, StoreAction::DontCare);
  EXPECT_EQ(desc.depthAttachment.face, 0u);
  EXPECT_EQ(desc.depthAttachment.mipLevel, 0u);
  EXPECT_EQ(desc.depthAttachment.layer, 0u);
  EXPECT_FLOAT_EQ(desc.depthAttachment.clearDepth, 1.0f);
  EXPECT_EQ(desc.depthAttachment.clearStencil, 0u);

  EXPECT_EQ(desc.stencilAttachment.loadAction, LoadAction::Clear);
  EXPECT_EQ(desc.stencilAttachment.storeAction, StoreAction::DontCare);
  EXPECT_EQ(desc.stencilAttachment.face, 0u);
  EXPECT_EQ(desc.stencilAttachment.mipLevel, 0u);
  EXPECT_EQ(desc.stencilAttachment.layer, 0u);
  EXPECT_FLOAT_EQ(desc.stencilAttachment.clearDepth, 1.0f);
  EXPECT_EQ(desc.stencilAttachment.clearStencil, 0u);

  EXPECT_EQ(desc.timestampQuery.queries, nullptr);
  EXPECT_EQ(desc.timestampQuery.slotIndex, 0u);
}

TEST(TimestampQueryDescTest, DefaultConstruction) {
  const RenderPassDesc::TimestampQueryDesc tqd;
  EXPECT_EQ(tqd.queries, nullptr);
  EXPECT_EQ(tqd.slotIndex, 0u);
}

} // namespace igl::tests
