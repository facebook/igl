/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Common.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// Result default construction
// ---------------------------------------------------------------------------

TEST(ResultMethodsTest, DefaultConstructionIsOk) {
  const Result r;
  EXPECT_TRUE(r.isOk());
  EXPECT_EQ(r.code, Result::Code::Ok);
  EXPECT_TRUE(r.message.empty());
}

// ---------------------------------------------------------------------------
// Result constructor with null const char* message
// ---------------------------------------------------------------------------

TEST(ResultMethodsTest, ConstructorWithNullMessageProducesEmpty) {
  const Result r(Result::Code::RuntimeError, nullptr);
  EXPECT_FALSE(r.isOk());
  EXPECT_EQ(r.code, Result::Code::RuntimeError);
  EXPECT_TRUE(r.message.empty());
}

TEST(ResultMethodsTest, ConstructorWithNonNullMessage) {
  const Result r(Result::Code::ArgumentInvalid, "bad arg");
  EXPECT_FALSE(r.isOk());
  EXPECT_EQ(r.message, "bad arg");
}

TEST(ResultMethodsTest, ConstructorWithStdStringMessage) {
  const std::string msg = "some error";
  const Result r(Result::Code::Unsupported, msg);
  EXPECT_FALSE(r.isOk());
  EXPECT_EQ(r.message, "some error");
}

// ---------------------------------------------------------------------------
// Result::isOk() for each error code
// ---------------------------------------------------------------------------

TEST(ResultIsOkTest, OkReturnsTrue) {
  const Result r(Result::Code::Ok);
  EXPECT_TRUE(r.isOk());
}

TEST(ResultIsOkTest, ArgumentInvalidReturnsFalse) {
  const Result r(Result::Code::ArgumentInvalid);
  EXPECT_FALSE(r.isOk());
}

TEST(ResultIsOkTest, ArgumentNullReturnsFalse) {
  const Result r(Result::Code::ArgumentNull);
  EXPECT_FALSE(r.isOk());
}

TEST(ResultIsOkTest, ArgumentOutOfRangeReturnsFalse) {
  const Result r(Result::Code::ArgumentOutOfRange);
  EXPECT_FALSE(r.isOk());
}

TEST(ResultIsOkTest, InvalidOperationReturnsFalse) {
  const Result r(Result::Code::InvalidOperation);
  EXPECT_FALSE(r.isOk());
}

TEST(ResultIsOkTest, UnsupportedReturnsFalse) {
  const Result r(Result::Code::Unsupported);
  EXPECT_FALSE(r.isOk());
}

TEST(ResultIsOkTest, UnimplementedReturnsFalse) {
  const Result r(Result::Code::Unimplemented);
  EXPECT_FALSE(r.isOk());
}

TEST(ResultIsOkTest, RuntimeErrorReturnsFalse) {
  const Result r(Result::Code::RuntimeError);
  EXPECT_FALSE(r.isOk());
}

TEST(ResultIsOkTest, DeviceLostReturnsFalse) {
  const Result r(Result::Code::DeviceLost);
  EXPECT_FALSE(r.isOk());
}

// ---------------------------------------------------------------------------
// Result::setResult() with null outResult — must not crash
// ---------------------------------------------------------------------------

TEST(ResultSetResultTest, NullOutResultCodeOverloadNoOp) {
  Result::setResult(nullptr, Result::Code::RuntimeError, "error");
}

TEST(ResultSetResultTest, NullOutResultCopyOverloadNoOp) {
  const Result source(Result::Code::ArgumentNull, "null");
  Result::setResult(nullptr, source);
}

TEST(ResultSetResultTest, NullOutResultMoveOverloadNoOp) {
  Result source(Result::Code::DeviceLost, "lost");
  Result::setResult(nullptr, std::move(source));
}

// ---------------------------------------------------------------------------
// Result::setOk()
// ---------------------------------------------------------------------------

TEST(ResultSetOkTest, ClearsErrorCodeAndMessage) {
  Result r(Result::Code::RuntimeError, "something broke");
  EXPECT_FALSE(r.isOk());
  EXPECT_FALSE(r.message.empty());

  Result::setOk(&r);
  EXPECT_TRUE(r.isOk());
  EXPECT_TRUE(r.message.empty());
}

TEST(ResultSetOkTest, NullOutResultNoOp) {
  Result::setOk(nullptr);
}

TEST(ResultSetOkTest, AlreadyOkRemainsOk) {
  Result r;
  EXPECT_TRUE(r.isOk());
  Result::setOk(&r);
  EXPECT_TRUE(r.isOk());
}

// ---------------------------------------------------------------------------
// Result::setResult() overwrites existing state
// ---------------------------------------------------------------------------

TEST(ResultSetResultTest, OverwritesOkWithError) {
  Result r;
  EXPECT_TRUE(r.isOk());

  Result::setResult(&r, Result::Code::InvalidOperation, "invalid");
  EXPECT_FALSE(r.isOk());
  EXPECT_EQ(r.code, Result::Code::InvalidOperation);
  EXPECT_EQ(r.message, "invalid");
}

TEST(ResultSetResultTest, OverwritesErrorWithDifferentError) {
  Result r(Result::Code::ArgumentInvalid, "first");
  Result::setResult(&r, Result::Code::DeviceLost, "second");
  EXPECT_EQ(r.code, Result::Code::DeviceLost);
  EXPECT_EQ(r.message, "second");
}

TEST(ResultSetResultTest, CopyOverloadPreservesSource) {
  const Result source(Result::Code::Unimplemented, "not yet");
  Result target;

  Result::setResult(&target, source);
  EXPECT_EQ(target.code, Result::Code::Unimplemented);
  EXPECT_EQ(target.message, "not yet");
  EXPECT_EQ(source.code, Result::Code::Unimplemented);
  EXPECT_EQ(source.message, "not yet");
}

} // namespace igl::tests
