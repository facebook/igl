/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>
#include <igl/TimestampQueries.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/WithContext.h>

namespace igl::opengl {

class TimestampQueries : public ITimestampQueries, public WithContext {
 public:
  TimestampQueries(IContext& context, uint32_t maxSlots);
  ~TimestampQueries() override;
  TimestampQueries(TimestampQueries&&) = delete;
  TimestampQueries& operator=(TimestampQueries&&) = delete;
  TimestampQueries(const TimestampQueries&) = delete;
  TimestampQueries& operator=(const TimestampQueries&) = delete;

  [[nodiscard]] uint32_t capacity() const override;
  [[nodiscard]] uint32_t count() const override;
  void reset() override;
  [[nodiscard]] bool resultsAvailable() const override;
  [[nodiscard]] uint64_t getElapsedNanos(uint32_t slotIndex) const override;

  /// Begin a GL_TIME_ELAPSED query for the given timing slot
  void beginElapsedQuery(uint32_t slotIndex);

  /// End the active GL_TIME_ELAPSED query
  void endElapsedQuery();

 private:
  std::vector<GLuint> queryIds_;
  uint32_t maxSlots_ = 0;
  uint32_t currentIndex_ = 0; // not atomic — GL is single-threaded
};

} // namespace igl::opengl
