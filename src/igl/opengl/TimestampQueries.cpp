/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/TimestampQueries.h>

#include <igl/opengl/GLFunc.h>

#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#endif

#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif

namespace igl::opengl {

TimestampQueries::TimestampQueries(IContext& context, uint32_t maxSlots) :
  WithContext(context), maxSlots_(maxSlots) {
  queryIds_.resize(maxSlots);
  iglGenQueries(maxSlots, queryIds_.data());
}

TimestampQueries::~TimestampQueries() {
  if (!queryIds_.empty()) {
    iglDeleteQueries(static_cast<GLsizei>(queryIds_.size()), queryIds_.data());
  }
}

uint32_t TimestampQueries::capacity() const {
  return maxSlots_;
}

uint32_t TimestampQueries::count() const {
  return currentIndex_;
}

void TimestampQueries::reset() {
  currentIndex_ = 0;
}

bool TimestampQueries::resultsAvailable() const {
  if (currentIndex_ == 0) {
    return false;
  }
  GLint available = 0;
  iglGetQueryObjectiv(queryIds_[currentIndex_ - 1], GL_QUERY_RESULT_AVAILABLE, &available);
  return available != 0;
}

uint64_t TimestampQueries::getElapsedNanos(uint32_t slotIndex) const {
  if (slotIndex >= currentIndex_) {
    return 0;
  }

  GLint available = 0;
  iglGetQueryObjectiv(queryIds_[slotIndex], GL_QUERY_RESULT_AVAILABLE, &available);
  if (!available) {
    return 0;
  }

  GLuint64 result = 0;
  iglGetQueryObjectui64v(queryIds_[slotIndex], GL_QUERY_RESULT, &result);
  return result;
}

void TimestampQueries::beginElapsedQuery(uint32_t slotIndex) {
  if (slotIndex >= maxSlots_) {
    return;
  }
  iglBeginQuery(GL_TIME_ELAPSED, queryIds_[slotIndex]);
  if (slotIndex >= currentIndex_) {
    currentIndex_ = slotIndex + 1;
  }
}

void TimestampQueries::endElapsedQuery() {
  iglEndQuery(GL_TIME_ELAPSED);
}

} // namespace igl::opengl
