/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/HWDevice.h>

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
  const ScissorRect testRect2{0, 0, 1, 1};
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

TEST(CommonTest, HandleTest) {
  const Holder<BindGroupTextureHandle> bindGroupHandle;
  const Holder<BindGroupBufferHandle> bindGroupBufferHandle;
  const Holder<TextureHandle> textureHandle;
  const Holder<SamplerHandle> samplerHandle;
}

TEST(CommonTest, PoolTest) {
  Pool<BindGroupBufferTag, BindGroupBufferDesc> bindGroupBuffersPool;
}
} // namespace igl::tests
