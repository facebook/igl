/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/CommandEncoder.h>
#include <igl/Framebuffer.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// BindGroupTextureDesc
// ---------------------------------------------------------------------------

TEST(BindGroupTextureDescTest, DefaultConstructionValues) {
  const BindGroupTextureDesc desc;
  for (const auto& texture : desc.textures) {
    EXPECT_EQ(texture, nullptr);
  }
  for (const auto& sampler : desc.samplers) {
    EXPECT_EQ(sampler, nullptr);
  }
  EXPECT_TRUE(desc.debugName.empty());
}

TEST(BindGroupTextureDescTest, ArraySizeMatchesMax) {
  EXPECT_EQ(std::size(BindGroupTextureDesc{}.textures), IGL_TEXTURE_SAMPLERS_MAX);
  EXPECT_EQ(std::size(BindGroupTextureDesc{}.samplers), IGL_TEXTURE_SAMPLERS_MAX);
}

TEST(BindGroupTextureDescTest, DesignatedInitializerDebugName) {
  const BindGroupTextureDesc desc{
      .debugName = "myTextureGroup",
  };
  EXPECT_EQ(desc.debugName, "myTextureGroup");
}

// ---------------------------------------------------------------------------
// BindGroupBufferDesc
// ---------------------------------------------------------------------------

TEST(BindGroupBufferDescTest, DefaultConstructionValues) {
  const BindGroupBufferDesc desc;
  for (const auto& buffer : desc.buffers) {
    EXPECT_EQ(buffer, nullptr);
  }
  for (const auto& offset : desc.offset) {
    EXPECT_EQ(offset, 0u);
  }
  for (const auto& size : desc.size) {
    EXPECT_EQ(size, 0u);
  }
  EXPECT_EQ(desc.isDynamicBufferMask, 0u);
  EXPECT_TRUE(desc.debugName.empty());
}

TEST(BindGroupBufferDescTest, ArraySizeMatchesMax) {
  EXPECT_EQ(std::size(BindGroupBufferDesc{}.buffers), IGL_UNIFORM_BLOCKS_BINDING_MAX);
  EXPECT_EQ(std::size(BindGroupBufferDesc{}.offset), IGL_UNIFORM_BLOCKS_BINDING_MAX);
  EXPECT_EQ(std::size(BindGroupBufferDesc{}.size), IGL_UNIFORM_BLOCKS_BINDING_MAX);
}

TEST(BindGroupBufferDescTest, DesignatedInitializerDebugName) {
  const BindGroupBufferDesc desc{
      .debugName = "myBufferGroup",
  };
  EXPECT_EQ(desc.debugName, "myBufferGroup");
}

TEST(BindGroupBufferDescTest, DynamicBufferMaskBits) {
  BindGroupBufferDesc desc;
  desc.isDynamicBufferMask = (1u << 0) | (1u << 3) | (1u << 7);
  EXPECT_TRUE(desc.isDynamicBufferMask & (1u << 0));
  EXPECT_FALSE(desc.isDynamicBufferMask & (1u << 1));
  EXPECT_FALSE(desc.isDynamicBufferMask & (1u << 2));
  EXPECT_TRUE(desc.isDynamicBufferMask & (1u << 3));
  EXPECT_TRUE(desc.isDynamicBufferMask & (1u << 7));
}

// ---------------------------------------------------------------------------
// FramebufferMode
// ---------------------------------------------------------------------------

TEST(FramebufferModeTest, AllValuesDistinct) {
  EXPECT_NE(FramebufferMode::Mono, FramebufferMode::Stereo);
  EXPECT_NE(FramebufferMode::Mono, FramebufferMode::Multiview);
  EXPECT_NE(FramebufferMode::Stereo, FramebufferMode::Multiview);
}

// ---------------------------------------------------------------------------
// FramebufferDesc
// ---------------------------------------------------------------------------

TEST(FramebufferDescTest, DefaultScalarFieldValues) {
  const FramebufferDesc desc;
  EXPECT_EQ(desc.mode, FramebufferMode::Mono);
  EXPECT_TRUE(desc.debugName.empty());
}

TEST(FramebufferDescTest, DefaultAttachmentsAreNull) {
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

TEST(FramebufferDescTest, DesignatedInitializer) {
  const FramebufferDesc desc{
      .debugName = "stereoFb",
      .mode = FramebufferMode::Stereo,
  };
  EXPECT_EQ(desc.debugName, "stereoFb");
  EXPECT_EQ(desc.mode, FramebufferMode::Stereo);
}

} // namespace igl::tests
