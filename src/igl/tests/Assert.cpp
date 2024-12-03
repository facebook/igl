/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

// Only include Assert.h and ensure it is configured to enable both soft errors and debug aborts
#define IGL_DEBUG 1
#define IGL_SOFT_ERROR_ENABLED 1
#define IGL_COMMON_SKIP_CHECK 1
#include <igl/Assert.h>

namespace igl::tests {
namespace {
bool sAbort = false;
bool sSoftError = false;
} // namespace

class AssertTest : public ::testing::Test {
 public:
  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    IGLSetDebugAbortListener([](const char* /*category*/,
                                const char* /*reason*/,
                                const char* /*file*/,
                                const char* /*func*/,
                                int /*line*/,
                                const char* /*format*/,
                                va_list /*ap*/) { sAbort = true; });
    IGLSetSoftErrorHandler([](const char* /*category*/,
                              const char* /*reason*/,
                              const char* /*file*/,
                              const char* /*func*/,
                              int /*line*/,
                              const char* /*format*/,
                              va_list /*ap*/) { sSoftError = true; });
  }

  void TearDown() override {
    IGLSetDebugAbortListener(nullptr);
    IGLSetSoftErrorHandler(nullptr);
  }
};

TEST_F(AssertTest, DebugAbort) {
  sAbort = false;
  EXPECT_FALSE(sAbort);
  IGL_DEBUG_ABORT("Aborting");
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  IGL_DEBUG_ABORT("Aborting with arg %d", 1);
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  IGL_DEBUG_ASSERT(false);
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  IGL_DEBUG_ASSERT(false, "Aborting");
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  IGL_DEBUG_ASSERT(false, "Aborting with arg %d", 1);
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  std::ignore = IGL_DEBUG_VERIFY(false);
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  std::ignore = IGL_DEBUG_VERIFY(false, "Aborting");
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  std::ignore = IGL_DEBUG_VERIFY(false, "Aborting with arg %d", 1);
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  std::ignore = IGL_DEBUG_VERIFY_NOT(true);
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  std::ignore = IGL_DEBUG_VERIFY_NOT(true, "Aborting");
  EXPECT_TRUE(sAbort);

  sAbort = false;
  EXPECT_FALSE(sAbort);
  std::ignore = IGL_DEBUG_VERIFY_NOT(true, "Aborting with arg %d", 1);
  EXPECT_TRUE(sAbort);
}

TEST_F(AssertTest, SoftError) {
  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  IGL_SOFT_ERROR("Aborting");
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  IGL_SOFT_ERROR("Aborting with arg %d", 1);
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  IGL_SOFT_ASSERT(false);
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  IGL_SOFT_ASSERT(false, "Aborting");
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  IGL_SOFT_ASSERT(false, "Aborting with arg %d", 1);
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  std::ignore = IGL_SOFT_VERIFY(false);
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  std::ignore = IGL_SOFT_VERIFY(false, "Aborting");
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  std::ignore = IGL_SOFT_VERIFY(false, "Aborting with arg %d", 1);
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  std::ignore = IGL_SOFT_VERIFY_NOT(true);
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  std::ignore = IGL_SOFT_VERIFY_NOT(true, "Aborting");
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);

  sAbort = false;
  sSoftError = false;
  EXPECT_FALSE(sAbort);
  EXPECT_FALSE(sSoftError);
  std::ignore = IGL_SOFT_VERIFY_NOT(true, "Aborting with arg %d", 1);
  EXPECT_TRUE(sAbort);
  EXPECT_TRUE(sSoftError);
}

} // namespace igl::tests
