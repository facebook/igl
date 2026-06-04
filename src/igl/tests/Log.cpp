/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <random>
#include <string>
#include <thread>
#include <igl/Core.h>

namespace igl::tests {

namespace {
int testHandlerCallCount = 0;
IGLLogLevel testHandlerLastLevel = IGLLogInfo;

int testLogHandler(IGLLogLevel logLevel, const char* IGL_RESTRICT /*format*/, va_list /*ap*/) {
  ++testHandlerCallCount;
  testHandlerLastLevel = logLevel;
  return 0;
}

int sentinelReturnValue = 0;

int returningLogHandler(IGLLogLevel /*logLevel*/,
                        const char* IGL_RESTRICT /*format*/,
                        va_list /*ap*/) {
  return sentinelReturnValue;
}

// Trampoline that packages variadic arguments into a va_list so IGLLogV() can be
// exercised directly (it is otherwise only reachable through IGLLog()).
int callLogV(IGLLogLevel logLevel, const char* IGL_RESTRICT format, ...) {
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  va_list ap;
  va_start(ap, format);
  const int result = IGLLogV(logLevel, format, ap);
  va_end(ap);
  return result;
}
} // namespace

TEST(LogTest, GetDefaultHandlerReturnsNonNull) {
  EXPECT_NE(IGLLogGetHandler(), nullptr);
}

TEST(LogTest, SetAndGetCustomHandler) {
  const auto originalHandler = IGLLogGetHandler();
  IGLLogSetHandler(testLogHandler);
  EXPECT_EQ(IGLLogGetHandler(), testLogHandler);
  IGLLogSetHandler(originalHandler);
  EXPECT_EQ(IGLLogGetHandler(), originalHandler);
}

TEST(LogTest, CustomHandlerReceivesMessages) {
  const auto originalHandler = IGLLogGetHandler();
  testHandlerCallCount = 0;
  IGLLogSetHandler(testLogHandler);

  IGLLog(IGLLogWarning, "test message %d", 42);

  EXPECT_EQ(testHandlerCallCount, 1);
  EXPECT_EQ(testHandlerLastLevel, IGLLogWarning);

  IGLLogSetHandler(originalHandler);
}

TEST(LogTest, LogReturnsHandlerReturnValue) {
  const auto originalHandler = IGLLogGetHandler();
  sentinelReturnValue = 123;
  IGLLogSetHandler(returningLogHandler);

  // IGLLog() forwards to IGLLogV(), which returns whatever the handler returns.
  EXPECT_EQ(IGLLog(IGLLogInfo, "ignored"), 123);

  IGLLogSetHandler(originalHandler);
}

TEST(LogTest, LogVForwardsToHandler) {
  const auto originalHandler = IGLLogGetHandler();
  testHandlerCallCount = 0;
  IGLLogSetHandler(testLogHandler);

  callLogV(IGLLogError, "value %d", 7);

  EXPECT_EQ(testHandlerCallCount, 1);
  EXPECT_EQ(testHandlerLastLevel, IGLLogError);

  IGLLogSetHandler(originalHandler);
}

TEST(LogTest, LogOnceSuppressesDuplicateMessages) {
  const auto originalHandler = IGLLogGetHandler();
  testHandlerCallCount = 0;
  IGLLogSetHandler(testLogHandler);

  // The first occurrence of a unique message reaches the handler; an identical
  // message logged again is suppressed and must not invoke the handler.
  IGLLogOnce(IGLLogInfo, "igl-log-once-unique-marker-A");
  IGLLogOnce(IGLLogInfo, "igl-log-once-unique-marker-A");
  EXPECT_EQ(testHandlerCallCount, 1);

  // A distinct message is logged independently.
  IGLLogOnce(IGLLogInfo, "igl-log-once-unique-marker-B");
  EXPECT_EQ(testHandlerCallCount, 2);

  IGLLogSetHandler(originalHandler);
}

TEST(LogTest, LogOnceRaceCondition) {
  auto logSomethingUniqueManyTimes = []() {
    std::default_random_engine generator;
    std::uniform_int_distribution<short> distribution('a', 'z');
    size_t repetitions = 1000;
    while (repetitions-- > 0) {
      const size_t len = 16;
      std::string msg(16, 'a');
      for (int i = 0; i < len; ++i) {
        msg[i] = static_cast<char>(distribution(generator));
      }
      IGLLogOnce(IGLLogInfo, "%s", msg.c_str());
    }
  };

  std::thread t1(logSomethingUniqueManyTimes);
  std::thread t2(logSomethingUniqueManyTimes);
  std::thread t3(logSomethingUniqueManyTimes);
  std::thread t4(logSomethingUniqueManyTimes);
  t1.join();
  t2.join();
  t3.join();
  t4.join();
}

} // namespace igl::tests
