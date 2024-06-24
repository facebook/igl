/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

/// @brief Based on
/// https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook/blob/master/shared/UtilsFPS.h.
/// Convenience class to measure and print frames per second (FPS) to the console.
class FPSCounter {
 public:
  /// @brief Constructs an instance of the class
  /// @param printToConsole whether to print the FPS count to the console
  /// @param avgIntervalInSeconds how often to update the FPS counter and print the information to
  /// the console. The default is 1 second
  explicit FPSCounter(bool printToConsole = true, float avgIntervalInSeconds = 1.0f) :
    printToConsole_(printToConsole), avgIntervalInSeconds_(avgIntervalInSeconds) {
    IGL_ASSERT(avgIntervalInSeconds > 0);
  }

  [[nodiscard]] float getAverageFPS() const noexcept {
    return avgFPS_;
  }

  /// @brief Updates the frame count and computes the FPS  if the time passed in as a parameter is
  /// greater than the threshold set at construciton time. This function should be called when a
  /// frame has finished being submited for processing by the GPU along with the delta time between
  /// the current and the previous frame
  /// @param seconds the time, in seconds, to record and submit this frame's commands. The delta
  /// time between this frame and the previous frame.
  void updateFPS(double seconds) {
    frames_++;
    time_ += seconds;

    if (time_ >= avgIntervalInSeconds_) {
      avgFPS_ = float(frames_ / time_);

      if (printToConsole_) {
        IGL_LOG_INFO("FPS: %.1f\n", avgFPS_);
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
