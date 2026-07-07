/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/RenderCommandEncoder.h>

namespace igl::tests {

TEST(BindTargetTest, ConstantValues) {
  EXPECT_EQ(BindTarget::kVertex, 0x0001u);
  EXPECT_EQ(BindTarget::kFragment, 0x0002u);
  EXPECT_EQ(BindTarget::kAllGraphics, 0x0003u);
  EXPECT_EQ(BindTarget::kTask, 0x0004u);
  EXPECT_EQ(BindTarget::kMesh, 0x0008u);
}

TEST(BindTargetTest, AllGraphicsIsVertexOrFragment) {
  EXPECT_EQ(BindTarget::kAllGraphics,
            static_cast<uint8_t>(BindTarget::kVertex | BindTarget::kFragment));
}

TEST(BindTargetTest, IndividualBitsAreDistinct) {
  EXPECT_EQ(BindTarget::kVertex & BindTarget::kFragment, 0u);
  EXPECT_EQ(BindTarget::kTask & BindTarget::kVertex, 0u);
  EXPECT_EQ(BindTarget::kTask & BindTarget::kFragment, 0u);
  EXPECT_EQ(BindTarget::kMesh & BindTarget::kVertex, 0u);
  EXPECT_EQ(BindTarget::kMesh & BindTarget::kFragment, 0u);
  EXPECT_EQ(BindTarget::kMesh & BindTarget::kTask, 0u);
}

TEST(BindTargetTest, BitwiseCombinations) {
  const uint8_t vertexTask = static_cast<uint8_t>(BindTarget::kVertex | BindTarget::kTask);
  EXPECT_NE(vertexTask, BindTarget::kVertex);
  EXPECT_NE(vertexTask, BindTarget::kTask);
  EXPECT_TRUE(vertexTask & BindTarget::kVertex);
  EXPECT_TRUE(vertexTask & BindTarget::kTask);
  EXPECT_FALSE(vertexTask & BindTarget::kMesh);
}

} // namespace igl::tests
