/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/CommandEncoder.h>

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

} // namespace igl::tests
