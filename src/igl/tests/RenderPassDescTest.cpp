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

TEST(RenderPassDescTest, ColorAttachmentWithClearColor) {
  RenderPassDesc desc;
  desc.colorAttachments.resize(1);
  desc.colorAttachments[0].loadAction = LoadAction::Clear;
  desc.colorAttachments[0].storeAction = StoreAction::Store;
  desc.colorAttachments[0].clearColor = {1.0f, 0.5f, 0.25f, 1.0f};

  EXPECT_EQ(desc.colorAttachments.size(), 1u);
  EXPECT_EQ(desc.colorAttachments[0].loadAction, LoadAction::Clear);
  EXPECT_EQ(desc.colorAttachments[0].storeAction, StoreAction::Store);
  EXPECT_FLOAT_EQ(desc.colorAttachments[0].clearColor.r, 1.0f);
  EXPECT_FLOAT_EQ(desc.colorAttachments[0].clearColor.g, 0.5f);
  EXPECT_FLOAT_EQ(desc.colorAttachments[0].clearColor.b, 0.25f);
  EXPECT_FLOAT_EQ(desc.colorAttachments[0].clearColor.a, 1.0f);
}

TEST(RenderPassDescTest, MultipleColorAttachments) {
  RenderPassDesc desc;
  desc.colorAttachments.resize(2);
  desc.colorAttachments[0].loadAction = LoadAction::Clear;
  desc.colorAttachments[1].loadAction = LoadAction::Load;

  EXPECT_EQ(desc.colorAttachments.size(), 2u);
  EXPECT_EQ(desc.colorAttachments[0].loadAction, LoadAction::Clear);
  EXPECT_EQ(desc.colorAttachments[1].loadAction, LoadAction::Load);
}

TEST(RenderPassDescTest, DepthClearValue) {
  RenderPassDesc desc;
  desc.depthAttachment.loadAction = LoadAction::Clear;
  desc.depthAttachment.storeAction = StoreAction::Store;
  desc.depthAttachment.clearDepth = 0.0f;

  EXPECT_EQ(desc.depthAttachment.loadAction, LoadAction::Clear);
  EXPECT_EQ(desc.depthAttachment.storeAction, StoreAction::Store);
  EXPECT_FLOAT_EQ(desc.depthAttachment.clearDepth, 0.0f);
}

TEST(AttachmentDescTest, MipLevelAndLayer) {
  RenderPassDesc::AttachmentDesc desc;
  desc.mipLevel = 3;
  desc.layer = 5;
  desc.face = 2;

  EXPECT_EQ(desc.mipLevel, 3u);
  EXPECT_EQ(desc.layer, 5u);
  EXPECT_EQ(desc.face, 2u);
}

TEST(LoadActionTest, AllValuesDistinct) {
  EXPECT_NE(LoadAction::DontCare, LoadAction::Load);
  EXPECT_NE(LoadAction::DontCare, LoadAction::Clear);
  EXPECT_NE(LoadAction::Load, LoadAction::Clear);
}

TEST(StoreActionTest, AllValuesDistinct) {
  EXPECT_NE(StoreAction::DontCare, StoreAction::Store);
  EXPECT_NE(StoreAction::DontCare, StoreAction::MsaaResolve);
  EXPECT_NE(StoreAction::Store, StoreAction::MsaaResolve);
}

TEST(AttachmentDescTest, DesignatedInitializer) {
  const RenderPassDesc::AttachmentDesc desc{
      .loadAction = LoadAction::Clear,
      .storeAction = StoreAction::MsaaResolve,
      .face = 2,
      .mipLevel = 3,
      .layer = 4,
      .clearColor = {1.0f, 0.5f, 0.25f, 0.75f},
      .clearDepth = 0.5f,
      .clearStencil = 128,
  };
  EXPECT_EQ(desc.loadAction, LoadAction::Clear);
  EXPECT_EQ(desc.storeAction, StoreAction::MsaaResolve);
  EXPECT_EQ(desc.face, 2u);
  EXPECT_EQ(desc.mipLevel, 3u);
  EXPECT_EQ(desc.layer, 4u);
  EXPECT_FLOAT_EQ(desc.clearColor.r, 1.0f);
  EXPECT_FLOAT_EQ(desc.clearColor.g, 0.5f);
  EXPECT_FLOAT_EQ(desc.clearColor.b, 0.25f);
  EXPECT_FLOAT_EQ(desc.clearColor.a, 0.75f);
  EXPECT_FLOAT_EQ(desc.clearDepth, 0.5f);
  EXPECT_EQ(desc.clearStencil, 128u);
}

TEST(RenderPassDescTest, StencilClearValue) {
  RenderPassDesc desc;
  desc.stencilAttachment.loadAction = LoadAction::Clear;
  desc.stencilAttachment.storeAction = StoreAction::Store;
  desc.stencilAttachment.clearStencil = 255;

  EXPECT_EQ(desc.stencilAttachment.loadAction, LoadAction::Clear);
  EXPECT_EQ(desc.stencilAttachment.storeAction, StoreAction::Store);
  EXPECT_EQ(desc.stencilAttachment.clearStencil, 255u);
}

TEST(RenderPassDescTest, ColorAttachmentDefaultsAfterResize) {
  RenderPassDesc desc;
  desc.colorAttachments.resize(3);
  ASSERT_EQ(desc.colorAttachments.size(), 3u);

  for (const auto& attachment : desc.colorAttachments) {
    EXPECT_EQ(attachment.loadAction, LoadAction::DontCare);
    EXPECT_EQ(attachment.storeAction, StoreAction::Store);
    EXPECT_EQ(attachment.face, 0u);
    EXPECT_EQ(attachment.mipLevel, 0u);
    EXPECT_EQ(attachment.layer, 0u);
    EXPECT_FLOAT_EQ(attachment.clearDepth, 1.0f);
    EXPECT_EQ(attachment.clearStencil, 0u);
  }
}

TEST(RenderPassDescTest, DepthAndStencilShareDefaults) {
  const RenderPassDesc desc;
  EXPECT_EQ(desc.depthAttachment.loadAction, desc.stencilAttachment.loadAction);
  EXPECT_EQ(desc.depthAttachment.storeAction, desc.stencilAttachment.storeAction);
  EXPECT_EQ(desc.depthAttachment.face, desc.stencilAttachment.face);
  EXPECT_EQ(desc.depthAttachment.mipLevel, desc.stencilAttachment.mipLevel);
  EXPECT_EQ(desc.depthAttachment.layer, desc.stencilAttachment.layer);
  EXPECT_FLOAT_EQ(desc.depthAttachment.clearDepth, desc.stencilAttachment.clearDepth);
  EXPECT_EQ(desc.depthAttachment.clearStencil, desc.stencilAttachment.clearStencil);
}

TEST(TimestampQueryDescTest, CustomSlotIndex) {
  RenderPassDesc::TimestampQueryDesc tqd;
  tqd.slotIndex = 42;
  EXPECT_EQ(tqd.slotIndex, 42u);
  EXPECT_EQ(tqd.queries, nullptr);
}

} // namespace igl::tests
