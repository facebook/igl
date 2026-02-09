/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Timer.h>

#include <igl/opengl/DeviceFeatureSet.h>

namespace igl::opengl {

#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif

#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#endif

#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT 0x8FBB
#endif

Timer::Timer(IContext& context) : WithContext(context) {
  iglGenQueries(1, &id_);
  iglBeginQuery(GL_TIME_ELAPSED, id_);
}

Timer::~Timer() {
  iglDeleteQueries(1, &id_);
}

void Timer::end() {
  iglEndQuery(GL_TIME_ELAPSED);
}

uint64_t Timer::getElapsedTimeNanos() const {
  // Check for GPU disjoint event (power management, context switch, etc.)
  // If a disjoint occurred, the timing results are invalid
  if (DeviceFeatureSet::usesOpenGLES()) {
    GLint disjoint = 0;
    getContext().getIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    if (disjoint) {
      return 0;
    }
  }

  GLuint64 result = 0;
  iglGetQueryObjectui64v(id_, GL_QUERY_RESULT, &result);
  return result;
}

bool Timer::resultsAvailable() const {
  GLint available = 0;
  iglGetQueryObjectiv(id_, GL_QUERY_RESULT_AVAILABLE, &available);
  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  return available;
}

} // namespace igl::opengl
