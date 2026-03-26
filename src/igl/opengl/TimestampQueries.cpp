/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/TimestampQueries.h>

#include <igl/opengl/DeviceFeatureSet.h>
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

#ifndef GL_GPU_DISJOINT_EXT
#define GL_GPU_DISJOINT_EXT 0x8FBB
#endif

namespace igl::opengl {

TimestampQueries::TimestampQueries(IContext& context, uint32_t maxSlots) :
  WithContext(context), maxSlots_(maxSlots) {
  queryIds_.resize(maxSlots);
  iglGenQueries(maxSlots, queryIds_.data());

  // Validate: broken Mali budget GPU drivers silently fail iglGenQueries,
  // leaving query IDs as 0. Any zero ID means the driver is broken - disable all.
  bool anyZero = false;
  for (uint32_t i = 0; i < maxSlots; ++i) {
    if (queryIds_[i] == 0) {
      anyZero = true;
      break;
    }
  }
  if (maxSlots > 0 && anyZero) {
    queryIds_.clear();
    valid_ = false;
  }
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

bool TimestampQueries::isValid() const {
  return valid_;
}

bool TimestampQueries::resultsAvailable() const {
  if (!valid_ || currentIndex_ == 0) {
    return false;
  }
  if (queryIds_[currentIndex_ - 1] == 0) {
    return false;
  }
  GLint available = 0;
  iglGetQueryObjectiv(queryIds_[currentIndex_ - 1], GL_QUERY_RESULT_AVAILABLE, &available);
  return available != 0;
}

uint64_t TimestampQueries::getElapsedNanos(uint32_t slotIndex) const {
  if (!valid_ || slotIndex >= currentIndex_) {
    return 0;
  }
  if (queryIds_[slotIndex] == 0) {
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

bool TimestampQueries::readAndClearDisjoint() {
  if (!getContext().deviceFeatures().hasExtension(Extensions::TimerQuery)) {
    return false;
  }
  GLint disjoint = 0;
  getContext().getIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
  return disjoint != 0;
}

void TimestampQueries::beginElapsedQuery(uint32_t slotIndex) {
  if (!valid_ || slotIndex >= maxSlots_) {
    return;
  }
  if (queryIds_[slotIndex] == 0) {
    return; // guard against zero query ID - invalid per GL spec
  }
  iglBeginQuery(GL_TIME_ELAPSED, queryIds_[slotIndex]);
  if (slotIndex >= currentIndex_) {
    currentIndex_ = slotIndex + 1;
  }
}

void TimestampQueries::endElapsedQuery() {
  if (!valid_) {
    return;
  }
  iglEndQuery(GL_TIME_ELAPSED);
}

} // namespace igl::opengl
