/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/base/Handle.h>
#include <igl/base/Pool.h>

namespace igl::tests {

struct Foo {};

struct FooImpl {
  int value = 0;
  bool operator==(const FooImpl& other) const {
    return value == other.value;
  }
};

// --- HandleTest ---

TEST(HandleTest, DefaultConstructedIsEmptyAndInvalid) {
  const Handle<Foo> h;
  EXPECT_TRUE(h.empty());
  EXPECT_FALSE(h.valid());
  EXPECT_FALSE(static_cast<bool>(h));
}

TEST(HandleTest, DefaultConstructedIndexAndGenAreZero) {
  const Handle<Foo> h;
  EXPECT_EQ(h.index(), 0u);
  EXPECT_EQ(h.gen(), 0u);
}

TEST(HandleTest, EqualityOperator) {
  const Handle<Foo> a;
  const Handle<Foo> b;
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(HandleTest, InequalityOperator) {
  Pool<Foo, FooImpl> pool;
  const Handle<Foo> a = pool.create(FooImpl{.value = 1});
  const Handle<Foo> b = pool.create(FooImpl{.value = 2});
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

// --- PoolTest ---

TEST(PoolTest, CreateReturnsValidHandle) {
  Pool<Foo, FooImpl> pool;
  const Handle<Foo> h = pool.create(FooImpl{.value = 42});
  EXPECT_TRUE(h.valid());
  EXPECT_FALSE(h.empty());
}

TEST(PoolTest, GetReturnsCreatedObject) {
  Pool<Foo, FooImpl> pool;
  const Handle<Foo> h = pool.create(FooImpl{.value = 7});
  const FooImpl* obj = pool.get(h);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->value, 7);
}

TEST(PoolTest, NumObjectsTrackCount) {
  Pool<Foo, FooImpl> pool;
  EXPECT_EQ(pool.numObjects(), 0u);
  const Handle<Foo> h1 = pool.create(FooImpl{.value = 1});
  EXPECT_EQ(pool.numObjects(), 1u);
  const Handle<Foo> h2 = pool.create(FooImpl{.value = 2});
  EXPECT_EQ(pool.numObjects(), 2u);
  pool.destroy(h1);
  EXPECT_EQ(pool.numObjects(), 1u);
  pool.destroy(h2);
  EXPECT_EQ(pool.numObjects(), 0u);
}

TEST(PoolTest, GetEmptyHandleReturnsNull) {
  Pool<Foo, FooImpl> pool;
  (void)pool.create(FooImpl{.value = 99});
  const Handle<Foo> empty;
  const FooImpl* obj = pool.get(empty);
  EXPECT_EQ(obj, nullptr);
}

TEST(PoolTest, DestroyResetsObjectToDefault) {
  Pool<Foo, FooImpl> pool;
  const Handle<Foo> h = pool.create(FooImpl{.value = 99});
  pool.destroy(h);
  EXPECT_EQ(pool.objects_[h.index()].obj_, FooImpl{});
}

TEST(PoolTest, DestroySlotRecycledNewHandleDiffersInGen) {
  Pool<Foo, FooImpl> pool;
  const Handle<Foo> h1 = pool.create(FooImpl{.value = 1});
  const uint32_t oldGen = h1.gen();
  const uint32_t oldIndex = h1.index();
  pool.destroy(h1);
  const Handle<Foo> h2 = pool.create(FooImpl{.value = 2});
  EXPECT_EQ(h2.index(), oldIndex);
  EXPECT_GT(h2.gen(), oldGen);
}

TEST(PoolTest, DestroyEmptyHandleIsNoOp) {
  Pool<Foo, FooImpl> pool;
  (void)pool.create(FooImpl{.value = 1});
  const Handle<Foo> empty;
  pool.destroy(empty);
  EXPECT_EQ(pool.numObjects(), 1u);
}

TEST(PoolTest, ClearResetsPool) {
  Pool<Foo, FooImpl> pool;
  (void)pool.create(FooImpl{.value = 1});
  (void)pool.create(FooImpl{.value = 2});
  pool.clear();
  EXPECT_EQ(pool.numObjects(), 0u);
  EXPECT_TRUE(pool.objects_.empty());
}

TEST(PoolTest, FindObjectReturnsCorrectHandle) {
  Pool<Foo, FooImpl> pool;
  (void)pool.create(FooImpl{.value = 10});
  const Handle<Foo> h2 = pool.create(FooImpl{.value = 20});
  (void)pool.create(FooImpl{.value = 30});
  const FooImpl needle{.value = 20};
  const Handle<Foo> found = pool.findObject(&needle);
  EXPECT_TRUE(found.valid());
  EXPECT_EQ(found.index(), h2.index());
  EXPECT_EQ(found.gen(), h2.gen());
}

TEST(PoolTest, FindObjectNullptrReturnsEmpty) {
  Pool<Foo, FooImpl> pool;
  (void)pool.create(FooImpl{.value = 1});
  const Handle<Foo> found = pool.findObject(nullptr);
  EXPECT_TRUE(found.empty());
}

TEST(PoolTest, FindObjectNotInPoolReturnsEmpty) {
  Pool<Foo, FooImpl> pool;
  (void)pool.create(FooImpl{.value = 1});
  const FooImpl needle{.value = 999};
  const Handle<Foo> found = pool.findObject(&needle);
  EXPECT_TRUE(found.empty());
}

TEST(PoolTest, MultipleCreateDestroy) {
  Pool<Foo, FooImpl> pool;
  const Handle<Foo> h1 = pool.create(FooImpl{.value = 1});
  const Handle<Foo> h2 = pool.create(FooImpl{.value = 2});
  const Handle<Foo> h3 = pool.create(FooImpl{.value = 3});
  EXPECT_EQ(pool.numObjects(), 3u);

  const uint32_t midIndex = h2.index();
  pool.destroy(h2);
  EXPECT_EQ(pool.numObjects(), 2u);

  const Handle<Foo> h4 = pool.create(FooImpl{.value = 4});
  EXPECT_EQ(h4.index(), midIndex);
  EXPECT_GT(h4.gen(), h2.gen());
  EXPECT_EQ(pool.numObjects(), 3u);

  const FooImpl* obj1 = pool.get(h1);
  ASSERT_NE(obj1, nullptr);
  EXPECT_EQ(obj1->value, 1);

  const FooImpl* obj3 = pool.get(h3);
  ASSERT_NE(obj3, nullptr);
  EXPECT_EQ(obj3->value, 3);

  const FooImpl* obj4 = pool.get(h4);
  ASSERT_NE(obj4, nullptr);
  EXPECT_EQ(obj4->value, 4);
}

TEST(PoolTest, IndexAsVoidRoundTrips) {
  Pool<Foo, FooImpl> pool;
  const Handle<Foo> h = pool.create(FooImpl{.value = 5});
  void* ptr = h.indexAsVoid();
  const auto roundTripped = reinterpret_cast<ptrdiff_t>(ptr);
  EXPECT_EQ(static_cast<uint32_t>(roundTripped), h.index());
}

} // namespace igl::tests
