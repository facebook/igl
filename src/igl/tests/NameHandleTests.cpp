/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <set>
#include <igl/NameHandle.h>

namespace igl::tests {
namespace {
NameHandle a = IGL_NAMEHANDLE("a");
NameHandle b = IGL_NAMEHANDLE("b");
NameHandle c = IGL_NAMEHANDLE("c");
NameHandle someLongerString = IGL_NAMEHANDLE("someLongerString");
} // namespace

TEST(NameHandleTests, string) {
  EXPECT_EQ(a.toString(), "a");
  EXPECT_EQ(b.toString(), "b");
  EXPECT_EQ(c.toString(), "c");
  EXPECT_EQ(someLongerString.toString(), "someLongerString");
}

TEST(NameHandleTests, crc32) {
  EXPECT_EQ(a.getCrc32(), 3904355907);
  EXPECT_EQ(b.getCrc32(), 1908338681);
  EXPECT_EQ(c.getCrc32(), 112844655);
  EXPECT_EQ(someLongerString.getCrc32(), 3994903871);
}

TEST(NameHandleTests, equality) {
  EXPECT_EQ(a, a);
  EXPECT_EQ(b, b);

  EXPECT_NE(a, b);
}

TEST(NameHandleTests, ordering) {
  EXPECT_GT(a, b);
  EXPECT_GT(b, c);

  EXPECT_GE(a, a);
  EXPECT_GE(a, b);
  EXPECT_GE(b, c);

  EXPECT_LT(c, b);
  EXPECT_LT(b, a);

  EXPECT_LE(c, c);
  EXPECT_LE(c, b);
  EXPECT_LE(b, a);
}

TEST(NameHandleTests, set) {
  std::set s{a, b};
  EXPECT_EQ(s.size(), 2);
  EXPECT_NE(s.find(a), s.end());
  EXPECT_NE(s.find(b), s.end());
  EXPECT_EQ(s.find(c), s.end());
}

} // namespace igl::tests
