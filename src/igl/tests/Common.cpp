/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <string>

namespace igl::tests {

TEST(CommonTest, BackendTypeToStringTest) {
  ASSERT_EQ(BackendTypeToString(BackendType::OpenGL), "OpenGL");
  ASSERT_EQ(BackendTypeToString(BackendType::Metal), "Metal");
  ASSERT_EQ(BackendTypeToString(BackendType::Vulkan), "Vulkan");
  // @fb-only
};

} // namespace igl::tests
