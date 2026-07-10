/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/CommandEncoder.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// Dependencies — constants and default construction
// ---------------------------------------------------------------------------

TEST(DependenciesDefaultTest, Constants) {
  EXPECT_EQ(Dependencies::kIglMaxTextureDependencies, 8u);
  EXPECT_EQ(Dependencies::kIglMaxBufferDependencies, 8u);
}

TEST(DependenciesDefaultTest, DefaultValues) {
  const Dependencies deps;
  for (auto* texture : deps.textures) {
    EXPECT_EQ(texture, nullptr);
  }
  for (auto* buffer : deps.buffers) {
    EXPECT_EQ(buffer, nullptr);
  }
  EXPECT_EQ(deps.next, nullptr);
}

// ---------------------------------------------------------------------------
// BindGroupBufferDesc — default construction
// ---------------------------------------------------------------------------

TEST(BindGroupBufferDescDefaultTest, DefaultValues) {
  const BindGroupBufferDesc desc;
  EXPECT_EQ(desc.isDynamicBufferMask, 0u);
  EXPECT_TRUE(desc.debugName.empty());
  for (const auto& buffer : desc.buffers) {
    EXPECT_EQ(buffer, nullptr);
  }
  for (const auto& offset : desc.offset) {
    EXPECT_EQ(offset, 0u);
  }
  for (const auto& size : desc.size) {
    EXPECT_EQ(size, 0u);
  }
}

// ---------------------------------------------------------------------------
// BindGroupTextureDesc — default construction
// ---------------------------------------------------------------------------

TEST(BindGroupTextureDescDefaultTest, DefaultValues) {
  const BindGroupTextureDesc desc;
  EXPECT_TRUE(desc.debugName.empty());
  for (const auto& texture : desc.textures) {
    EXPECT_EQ(texture, nullptr);
  }
  for (const auto& sampler : desc.samplers) {
    EXPECT_EQ(sampler, nullptr);
  }
}

} // namespace igl::tests
