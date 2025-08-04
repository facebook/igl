/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Timer.h>

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
  GLuint64 result = 0;
  iglGetQueryObjectui64v(id_, GL_QUERY_RESULT, &result);
  return result;
}

bool Timer::resultsAvailable() const {
  GLint available = 0;
  iglGetQueryObjectiv(id_, GL_QUERY_RESULT_AVAILABLE, &available);
  return available;
}

} // namespace igl::opengl
