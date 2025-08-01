/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Timer.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/WithContext.h>

namespace igl {
namespace opengl {

class Timer : public WithContext, public ITimer {
 public:
  Timer(IContext& context);
  ~Timer() override;

  void end();

  uint64_t getElapsedTimeNanos() const override;

  virtual bool resultsAvailable() const override;

 private:
  GLuint id_ = 0;
};

} // namespace opengl
} // namespace igl
