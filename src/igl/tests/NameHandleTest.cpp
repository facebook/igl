/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/NameHandle.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace igl::tests {

TEST(NameHandleTest, DefaultConstruction) {
  const NameHandle nh;
  EXPECT_EQ(nh.getCrc32(), 0u);
  EXPECT_STREQ(nh.c_str(), "");
  EXPECT_TRUE(nh.toString().empty());
}

TEST(NameHandleTest, ConstructFromStringAndCrc) {
  const std::string name = "myUniform";
  const uint32_t crc = iglCrc32(name.c_str(), name.length());
  const NameHandle nh(name, crc);
  EXPECT_STREQ(nh.c_str(), "myUniform");
  EXPECT_EQ(nh.getCrc32(), crc);
}

TEST(NameHandleTest, ConstructFromCharPtrAndCrc) {
  const char* name = "position";
  const uint32_t crc = iglCrc32(name, 8);
  const NameHandle nh(name, crc);
  EXPECT_STREQ(nh.c_str(), "position");
  EXPECT_EQ(nh.getCrc32(), crc);
}

TEST(NameHandleTest, ConstructFromStringViewAndCrc) {
  const std::string_view name = "texCoord";
  const uint32_t crc = iglCrc32ConstExpr(name);
  const NameHandle nh(name, crc);
  EXPECT_EQ(nh.toString(), "texCoord");
  EXPECT_EQ(nh.getCrc32(), crc);
}

TEST(NameHandleTest, CopyConstruction) {
  const NameHandle original = IGL_NAMEHANDLE("color");
  const NameHandle copy(original);
  EXPECT_EQ(copy, original);
  EXPECT_STREQ(copy.c_str(), "color");
  EXPECT_EQ(copy.getCrc32(), original.getCrc32());
}

TEST(NameHandleTest, MoveConstruction) {
  NameHandle original = IGL_NAMEHANDLE("normal");
  const uint32_t crc = original.getCrc32();
  const NameHandle moved(std::move(original));
  EXPECT_STREQ(moved.c_str(), "normal");
  EXPECT_EQ(moved.getCrc32(), crc);
}

TEST(NameHandleTest, CopyAssignment) {
  const NameHandle src = IGL_NAMEHANDLE("alpha");
  NameHandle dst = IGL_NAMEHANDLE("beta");
  EXPECT_NE(src, dst);
  dst = src;
  EXPECT_EQ(dst, src);
  EXPECT_STREQ(dst.c_str(), "alpha");
}

TEST(NameHandleTest, CopyAssignmentSameCrcSkipsUpdate) {
  const NameHandle nh = IGL_NAMEHANDLE("gamma");
  NameHandle copy = nh;
  copy = nh;
  EXPECT_EQ(copy, nh);
}

TEST(NameHandleTest, MoveAssignment) {
  NameHandle src = IGL_NAMEHANDLE("delta");
  const uint32_t crc = src.getCrc32();
  NameHandle dst;
  dst = std::move(src);
  EXPECT_STREQ(dst.c_str(), "delta");
  EXPECT_EQ(dst.getCrc32(), crc);
}

TEST(NameHandleTest, EqualityReflexive) {
  const NameHandle nh = IGL_NAMEHANDLE("test");
  EXPECT_EQ(nh, nh);
}

TEST(NameHandleTest, EqualitySameString) {
  const NameHandle a = IGL_NAMEHANDLE("uniform");
  const NameHandle b = IGL_NAMEHANDLE("uniform");
  EXPECT_EQ(a, b);
}

TEST(NameHandleTest, InequalityDifferentStrings) {
  const NameHandle a = IGL_NAMEHANDLE("foo");
  const NameHandle b = IGL_NAMEHANDLE("bar");
  EXPECT_NE(a, b);
}

TEST(NameHandleTest, LessThanOperator) {
  const NameHandle a = IGL_NAMEHANDLE("aaa");
  const NameHandle b = IGL_NAMEHANDLE("zzz");
  if (a.getCrc32() < b.getCrc32()) {
    EXPECT_LT(a, b);
    EXPECT_FALSE(a > b);
  } else {
    EXPECT_GT(a, b);
    EXPECT_FALSE(a < b);
  }
}

TEST(NameHandleTest, LessOrEqualOperator) {
  const NameHandle a = IGL_NAMEHANDLE("same");
  const NameHandle b = IGL_NAMEHANDLE("same");
  EXPECT_LE(a, b);
  EXPECT_GE(a, b);
}

TEST(NameHandleTest, ConversionToConstCharPtr) {
  const NameHandle nh = IGL_NAMEHANDLE("convert");
  const char* str = nh;
  EXPECT_STREQ(str, "convert");
}

TEST(NameHandleTest, GenNameHandle) {
  const std::string name = "myBuffer";
  const NameHandle nh = genNameHandle(name);
  EXPECT_STREQ(nh.c_str(), "myBuffer");
  EXPECT_EQ(nh.getCrc32(), iglCrc32(name.c_str(), name.length()));
}

TEST(NameHandleTest, IglNamehandleMacro) {
  const NameHandle nh = IGL_NAMEHANDLE("macroTest");
  EXPECT_STREQ(nh.c_str(), "macroTest");
  EXPECT_EQ(nh.getCrc32(), iglCrc32ConstExpr("macroTest"));
}

TEST(NameHandleTest, Crc32ConstExprCharPtrMatchesRuntime) {
  const char* str = "hello";
  const uint32_t constexprCrc = iglCrc32ConstExpr(str);
  const uint32_t runtimeCrc = iglCrc32(str, 5);
  EXPECT_EQ(constexprCrc, runtimeCrc);
}

TEST(NameHandleTest, Crc32ConstExprStringViewMatchesCharPtr) {
  const char* str = "world";
  const std::string_view sv = str;
  EXPECT_EQ(iglCrc32ConstExpr(str), iglCrc32ConstExpr(sv));
}

TEST(NameHandleTest, Crc32EmptyStringConsistent) {
  const uint32_t crc = iglCrc32ConstExpr("");
  const uint32_t crcSv = iglCrc32ConstExpr(std::string_view(""));
  EXPECT_EQ(crc, crcSv);
}

TEST(NameHandleTest, HashConsistency) {
  const NameHandle nh = IGL_NAMEHANDLE("hashMe");
  const std::hash<NameHandle> hasher;
  EXPECT_EQ(hasher(nh), hasher(nh));
}

TEST(NameHandleTest, HashEqualObjectsSameHash) {
  const NameHandle a = IGL_NAMEHANDLE("equal");
  const NameHandle b = IGL_NAMEHANDLE("equal");
  const std::hash<NameHandle> hasher;
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(NameHandleTest, HashDifferentObjectsDifferentHash) {
  const NameHandle a = IGL_NAMEHANDLE("one");
  const NameHandle b = IGL_NAMEHANDLE("two");
  const std::hash<NameHandle> hasher;
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(NameHandleTest, HashPairConsistency) {
  const auto a = IGL_NAMEHANDLE("first");
  const auto b = IGL_NAMEHANDLE("second");
  const std::hash<std::pair<NameHandle, NameHandle>> hasher;
  const auto pair = std::make_pair(a, b);
  EXPECT_EQ(hasher(pair), hasher(pair));
}

TEST(NameHandleTest, UsableAsUnorderedMapKey) {
  std::unordered_map<NameHandle, int> map;
  const NameHandle key = IGL_NAMEHANDLE("mapKey");
  map[key] = 42;
  EXPECT_EQ(map[key], 42);
  EXPECT_EQ(map.count(IGL_NAMEHANDLE("mapKey")), 1u);
  EXPECT_EQ(map.count(IGL_NAMEHANDLE("other")), 0u);
}

} // namespace igl::tests
