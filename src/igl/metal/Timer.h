/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Timer.h>

namespace igl::metal {

class Timer : public ITimer {
 public:
  ~Timer() override {}

  uint64_t getElapsedTimeNanos() const override {
    return executionTime_;
  }

  virtual bool resultsAvailable() const override {
    return executionTime_ != 0;
  }

 private:
  uint64_t executionTime_ = 0;

  friend class CommandQueue;
};

} // namespace igl::metal
