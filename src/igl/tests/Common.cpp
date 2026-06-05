/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Common.h>

#include <igl/CommandEncoder.h>

namespace igl::tests {

TEST(CommonTest, BackendTypeToStringTest) {
  ASSERT_EQ(BackendTypeToString(BackendType::Invalid), "Invalid");
  ASSERT_EQ(BackendTypeToString(BackendType::OpenGL), "OpenGL");
  ASSERT_EQ(BackendTypeToString(BackendType::Metal), "Metal");
  ASSERT_EQ(BackendTypeToString(BackendType::Vulkan), "Vulkan");
  // @fb-only
}

TEST(CommonTest, ResultTest) {
  Result testResult;
  Result testResult2(Result::Code::Ok, "test message2");
  Result testResult3(Result::Code::Ok, std::string("test message3"));
  ASSERT_STREQ(testResult2.message.c_str(), "test message2");
  ASSERT_TRUE(testResult2.isOk());
  ASSERT_STREQ(testResult3.message.c_str(), "test message3");
  ASSERT_TRUE(testResult3.isOk());

  Result::setResult(&testResult, Result::Code::ArgumentInvalid, std::string("new test message"));
  ASSERT_STREQ(testResult.message.c_str(), "new test message");
  ASSERT_FALSE(testResult.isOk());

  Result::setResult(&testResult3, testResult);
  ASSERT_FALSE(testResult3.isOk());

  Result::setResult(&testResult2, std::move(testResult));
  ASSERT_FALSE(testResult2.isOk());
}

TEST(CommonTest, RectTest) {
  const ScissorRect testRect;
  ASSERT_TRUE(testRect.isNull());
  const ScissorRect testRect2{.x = 0, .y = 0, .width = 1, .height = 1};
  ASSERT_FALSE(testRect2.isNull());
}

TEST(CommonTest, SizeTest) {
  Size size;
  ASSERT_EQ(size.height, 0.0f);
  ASSERT_EQ(size.width, 0.0f);
  Size size2(2, 2);
  ASSERT_EQ(size2.height, 2.0f);
  ASSERT_EQ(size2.width, 2.0f);

  ASSERT_TRUE(size != size2);
  ASSERT_TRUE(size2 == size2);
  ASSERT_FALSE(size == size2);
  ASSERT_FALSE(size2 != size2);
}

TEST(CommonTest, DimensionTest) {
  Dimensions dimension;
  ASSERT_EQ(dimension.height, 0);
  ASSERT_EQ(dimension.width, 0);
  ASSERT_EQ(dimension.depth, 0);
  Dimensions dimension2(2, 2, 2);
  ASSERT_EQ(dimension2.height, 2);
  ASSERT_EQ(dimension2.width, 2);
  ASSERT_EQ(dimension2.depth, 2);

  ASSERT_TRUE(dimension != dimension2);
  ASSERT_TRUE(dimension2 == dimension2);
  ASSERT_FALSE(dimension == dimension2);
  ASSERT_FALSE(dimension2 != dimension2);
}

TEST(CommonTest, ViewportTest) {
  Viewport viewport;
  ASSERT_TRUE(viewport != kInvalidViewport);
  Viewport viewport2;
  ASSERT_TRUE(viewport == viewport2);
}

TEST(CommonTest, ViewportDefaultValues) {
  // The default viewport covers a unit region: width/height default to 1.0f (not
  // 0.0f) so a freshly constructed Viewport is immediately usable.
  const Viewport viewport;
  EXPECT_FLOAT_EQ(viewport.x, 0.0f);
  EXPECT_FLOAT_EQ(viewport.y, 0.0f);
  EXPECT_FLOAT_EQ(viewport.width, 1.0f);
  EXPECT_FLOAT_EQ(viewport.height, 1.0f);
  EXPECT_FLOAT_EQ(viewport.minDepth, 0.0f);
  EXPECT_FLOAT_EQ(viewport.maxDepth, 1.0f);
}

TEST(CommonTest, InvalidViewportValues) {
  // kInvalidViewport is the sentinel used by `!= kInvalidViewport` checks; every
  // field must be -1.0f so it can never be confused with a valid viewport.
  EXPECT_FLOAT_EQ(kInvalidViewport.x, -1.0f);
  EXPECT_FLOAT_EQ(kInvalidViewport.y, -1.0f);
  EXPECT_FLOAT_EQ(kInvalidViewport.width, -1.0f);
  EXPECT_FLOAT_EQ(kInvalidViewport.height, -1.0f);
  EXPECT_FLOAT_EQ(kInvalidViewport.minDepth, -1.0f);
  EXPECT_FLOAT_EQ(kInvalidViewport.maxDepth, -1.0f);
}

TEST(CommonTest, ViewportInequalityPerField) {
  // operator== must compare every field; mutating each one in isolation should
  // make the viewports compare non-equal.
  const Viewport base;
  {
    Viewport other = base;
    other.x = 5.0f;
    EXPECT_TRUE(base != other);
    EXPECT_FALSE(base == other);
  }
  {
    Viewport other = base;
    other.y = 5.0f;
    EXPECT_TRUE(base != other);
    EXPECT_FALSE(base == other);
  }
  {
    Viewport other = base;
    other.width = 5.0f;
    EXPECT_TRUE(base != other);
    EXPECT_FALSE(base == other);
  }
  {
    Viewport other = base;
    other.height = 5.0f;
    EXPECT_TRUE(base != other);
    EXPECT_FALSE(base == other);
  }
  {
    Viewport other = base;
    other.minDepth = 0.5f;
    EXPECT_TRUE(base != other);
    EXPECT_FALSE(base == other);
  }
  {
    Viewport other = base;
    other.maxDepth = 0.5f;
    EXPECT_TRUE(base != other);
    EXPECT_FALSE(base == other);
  }
}

TEST(CommonTest, EnumToValueTest) {
  auto val = EnumToValue(BackendType::Vulkan);
  ASSERT_EQ(val, 3);
}

TEST(CommonTest, ScopeGuardTest) {
  int testValue = 0;
  {
    auto scopeGuard = ScopeGuardOnExit() + [&]() { ++testValue; };
  }
  ASSERT_EQ(testValue, 1);
}

TEST(CommonTest, OptimizedMemCopyTest) {
  uint8_t buffer1[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                       17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
  uint8_t buffer2[32] = {0};

  optimizedMemcpy(buffer1, buffer2, 4);
  ASSERT_EQ(*reinterpret_cast<uint32_t*>(buffer1), *reinterpret_cast<uint32_t*>(buffer2));
  optimizedMemcpy(buffer1, buffer2, 8);
  ASSERT_EQ(*reinterpret_cast<uint64_t*>(buffer1), *reinterpret_cast<uint64_t*>(buffer2));
  optimizedMemcpy(buffer1, buffer2, 16);
  ASSERT_EQ(*reinterpret_cast<uint64_t*>(buffer1), *reinterpret_cast<uint64_t*>(buffer2));
  ASSERT_EQ(*(reinterpret_cast<uint64_t*>(buffer1) + 1),
            *(reinterpret_cast<uint64_t*>(buffer2) + 1));
  optimizedMemcpy(buffer1, buffer2, 32);
  ASSERT_EQ(*reinterpret_cast<uint64_t*>(buffer1), *reinterpret_cast<uint64_t*>(buffer2));
  ASSERT_EQ(*(reinterpret_cast<uint64_t*>(buffer1) + 1),
            *(reinterpret_cast<uint64_t*>(buffer2) + 1));
  ASSERT_EQ(*(reinterpret_cast<uint64_t*>(buffer1) + 2),
            *(reinterpret_cast<uint64_t*>(buffer2) + 2));
  ASSERT_EQ(*(reinterpret_cast<uint64_t*>(buffer1) + 3),
            *(reinterpret_cast<uint64_t*>(buffer2) + 3));
}

TEST(CommonTest, ResultSetOk) {
  Result result(Result::Code::ArgumentInvalid, "some error");
  ASSERT_FALSE(result.isOk());
  ASSERT_FALSE(result.message.empty());

  Result::setOk(&result);
  ASSERT_TRUE(result.isOk());
  ASSERT_TRUE(result.message.empty());
}

TEST(CommonTest, ResultSetOkNullSafe) {
  Result::setOk(nullptr);
}

TEST(CommonTest, ResultSetResultNullSafe) {
  Result::setResult(nullptr, Result::Code::RuntimeError, "should not crash");
  const Result src(Result::Code::Unsupported, "src");
  Result::setResult(nullptr, src);
  Result::setResult(nullptr, Result(Result::Code::Unimplemented, "tmp"));
}

TEST(CommonTest, ResultErrorCodes) {
  const Result r1(Result::Code::ArgumentNull, "null arg");
  ASSERT_FALSE(r1.isOk());
  ASSERT_EQ(r1.code, Result::Code::ArgumentNull);

  const Result r2(Result::Code::ArgumentOutOfRange);
  ASSERT_FALSE(r2.isOk());
  ASSERT_EQ(r2.code, Result::Code::ArgumentOutOfRange);

  const Result r3(Result::Code::Unsupported, "not supported");
  ASSERT_EQ(r3.code, Result::Code::Unsupported);

  const Result r4(Result::Code::DeviceLost, "gpu gone");
  ASSERT_EQ(r4.code, Result::Code::DeviceLost);
}

TEST(CommonTest, HandleTest) {
  const Holder<BindGroupTextureHandle> bindGroupHandle;
  const Holder<BindGroupBufferHandle> bindGroupBufferHandle;
  const Holder<TextureHandle> textureHandle;
  const Holder<SamplerHandle> samplerHandle;
}

TEST(CommonTest, PoolTest) {
  Pool<BindGroupBufferTag, BindGroupBufferDesc> bindGroupBuffersPool;
}

TEST(HolderTest, DefaultConstructedIsEmptyAndInvalid) {
  const Holder<BindGroupTextureHandle> holder;
  EXPECT_TRUE(holder.empty());
  EXPECT_FALSE(holder.valid());
}

TEST(HolderTest, DefaultConstructedIndexAndGenAreZero) {
  const Holder<TextureHandle> holder;
  EXPECT_EQ(holder.index(), 0u);
  EXPECT_EQ(holder.gen(), 0u);
}

TEST(HolderTest, MoveConstructionTransfersState) {
  Pool<BindGroupBufferTag, BindGroupBufferDesc> pool;
  const BindGroupBufferHandle handle = pool.create(BindGroupBufferDesc{});

  Holder<BindGroupBufferHandle> original(nullptr, handle);
  EXPECT_FALSE(original.empty());
  EXPECT_TRUE(original.valid());

  Holder<BindGroupBufferHandle> moved(std::move(original));
  EXPECT_FALSE(moved.empty());
  EXPECT_TRUE(moved.valid());
  EXPECT_TRUE(original.empty()); // NOLINT(bugprone-use-after-move)
}

TEST(HolderTest, MoveAssignmentTransfersState) {
  Pool<BindGroupBufferTag, BindGroupBufferDesc> pool;
  const BindGroupBufferHandle handle = pool.create(BindGroupBufferDesc{});

  Holder<BindGroupBufferHandle> a(nullptr, handle);
  Holder<BindGroupBufferHandle> b;
  EXPECT_FALSE(a.empty());
  EXPECT_TRUE(b.empty());

  b = std::move(a);
  EXPECT_FALSE(b.empty());
  EXPECT_TRUE(a.empty()); // NOLINT(bugprone-use-after-move)
}

TEST(HolderTest, AssignNullptrResetsHolder) {
  Pool<BindGroupBufferTag, BindGroupBufferDesc> pool;
  const BindGroupBufferHandle handle = pool.create(BindGroupBufferDesc{});

  Holder<BindGroupBufferHandle> holder(nullptr, handle);
  EXPECT_FALSE(holder.empty());

  holder = nullptr;
  EXPECT_TRUE(holder.empty());
  EXPECT_FALSE(holder.valid());
}

TEST(HolderTest, ReleaseReturnsHandleAndEmptiesHolder) {
  Pool<BindGroupBufferTag, BindGroupBufferDesc> pool;
  const BindGroupBufferHandle handle = pool.create(BindGroupBufferDesc{});

  Holder<BindGroupBufferHandle> holder(nullptr, handle);
  EXPECT_FALSE(holder.empty());

  const BindGroupBufferHandle released = holder.release();
  EXPECT_FALSE(released.empty());
  EXPECT_TRUE(holder.empty());
}

TEST(HolderTest, ResetEmptiesHolder) {
  Pool<BindGroupBufferTag, BindGroupBufferDesc> pool;
  const BindGroupBufferHandle handle = pool.create(BindGroupBufferDesc{});

  Holder<BindGroupBufferHandle> holder(nullptr, handle);
  EXPECT_FALSE(holder.empty());

  holder.reset();
  EXPECT_TRUE(holder.empty());
  EXPECT_FALSE(holder.valid());
}

} // namespace igl::tests
