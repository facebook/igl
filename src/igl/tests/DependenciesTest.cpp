/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/CommandEncoder.h>
#include <igl/CommandQueue.h>

namespace igl::tests {

static_assert(std::size(Dependencies{}.textures) == Dependencies::kIglMaxTextureDependencies,
              "textures array size must match kIglMaxTextureDependencies");
static_assert(std::size(Dependencies{}.buffers) == Dependencies::kIglMaxBufferDependencies,
              "buffers array size must match kIglMaxBufferDependencies");

TEST(DependenciesTest, DefaultConstructionTexturesNull) {
  const Dependencies deps;
  for (const auto& texture : deps.textures) {
    EXPECT_EQ(texture, nullptr);
  }
}

TEST(DependenciesTest, DefaultConstructionBuffersNull) {
  const Dependencies deps;
  for (const auto& buffer : deps.buffers) {
    EXPECT_EQ(buffer, nullptr);
  }
}

TEST(DependenciesTest, DefaultConstructionNextNull) {
  const Dependencies deps;
  EXPECT_EQ(deps.next, nullptr);
}

TEST(DependenciesTest, ChainViaNext) {
  Dependencies inner;
  Dependencies outer;
  outer.next = &inner;
  EXPECT_EQ(outer.next, &inner);
  EXPECT_EQ(inner.next, nullptr);
}

// ---------------------------------------------------------------------------
// BindGroupTextureDesc
// ---------------------------------------------------------------------------

TEST(BindGroupTextureDescTest, DefaultConstruction) {
  const BindGroupTextureDesc desc;
  EXPECT_TRUE(desc.debugName.empty());
  for (uint32_t i = 0; i < IGL_TEXTURE_SAMPLERS_MAX; ++i) {
    EXPECT_EQ(desc.textures[i], nullptr);
    EXPECT_EQ(desc.samplers[i], nullptr);
  }
}

// ---------------------------------------------------------------------------
// BindGroupBufferDesc
// ---------------------------------------------------------------------------

TEST(BindGroupBufferDescTest, DefaultConstruction) {
  const BindGroupBufferDesc desc;
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.isDynamicBufferMask, 0u);
  for (uint32_t i = 0; i < IGL_UNIFORM_BLOCKS_BINDING_MAX; ++i) {
    EXPECT_EQ(desc.buffers[i], nullptr);
    EXPECT_EQ(desc.offset[i], 0u);
    EXPECT_EQ(desc.size[i], 0u);
  }
}

// ---------------------------------------------------------------------------
// CommandQueueStatistics
// ---------------------------------------------------------------------------

TEST(CommandQueueStatisticsTest, DefaultConstruction) {
  const CommandQueueStatistics stats;
  EXPECT_EQ(stats.currentDrawCount, 0u);
  EXPECT_EQ(stats.lastFrameDrawCount, 0u);
}

} // namespace igl::tests
