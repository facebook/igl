/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

// based on
// https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook/blob/master/shared/UtilsFPS.h
class FPSCounter {
 public:
  explicit FPSCounter(bool printToConsole = true, float avgIntervalInSeconds = 1.0f) :
    printToConsole_(printToConsole), avgIntervalInSeconds_(avgIntervalInSeconds) {
    IGL_ASSERT(avgIntervalInSeconds > 0);
  }

  float getAverageFPS() const noexcept {
    return avgFPS_;
  }

  void updateFPS(double seconds) {
    frames_++;
    time_ += seconds;

    if (time_ >= avgIntervalInSeconds_) {
      avgFPS_ = float(frames_ / time_);

      if (printToConsole_) {
        printf("FPS: %.1f\n", avgFPS_);
      }

      frames_ = 0;
      time_ = 0;
    }
  }

 private:
  bool printToConsole_ = true;

  uint32_t frames_ = 0;
  double time_ = 0;

  const float avgIntervalInSeconds_ = 1.0f;
  float avgFPS_ = 72.0f; // randomly picked high-ish initial value
};

} // namespace igl
