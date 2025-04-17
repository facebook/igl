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
#include <igl/Common.h>

namespace igl::tests {

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
