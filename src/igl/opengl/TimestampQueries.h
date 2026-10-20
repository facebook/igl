/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>
#include <igl/Framebuffer.h>
#include <igl/TimestampQueries.h>
#include <igl/opengl/GLFunc.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/WithContext.h>

namespace igl::tests {
class TimerOGLTest;
}

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
  [[nodiscard]] bool wasBegun(uint32_t slotIndex) const override;
  void reset() override;
  [[nodiscard]] bool resultsAvailable() const override;
  [[nodiscard]] uint64_t getElapsedNanos(uint32_t slotIndex) const override;
  [[nodiscard]] TimestampQueryResult getElapsedNanosResult(uint32_t slotIndex) const override;
  [[nodiscard]] bool readAndClearDisjoint() override;

  [[nodiscard]] bool isValid() const override;

  /// True: the Mali OpenGL driver internally references FBOs during a GL_TIME_ELAPSED query.
  /// Direct callers of slot-only beginElapsedQuery() must retain framebuffers and attachments
  /// until results are consumed; encoder-issued queries retain them internally until reset().
  [[nodiscard]] bool requiresFramebufferRetention() const override;

  /// Start a GL_TIME_ELAPSED query for the given timing slot.
  /// Must be paired with endElapsedQuery(). Cannot be nested.
  /// Virtual so a subclass can piggy-back additional GL_TIMESTAMP queries at
  /// the same boundary the IGL OpenGL RenderCommandEncoder calls through
  /// TimestampQueries& on.
  virtual void beginElapsedQuery(uint32_t slotIndex);

  /// End the active GL_TIME_ELAPSED query.
  /// Virtual for the same reason as beginElapsedQuery() above.
  virtual void endElapsedQuery();

 private:
  friend class igl::tests::TimerOGLTest;
  friend class RenderCommandEncoder;
  void beginElapsedQuery(uint32_t slotIndex, const std::shared_ptr<IFramebuffer>& framebuffer);

  struct RetainedFramebuffer {
    std::shared_ptr<IFramebuffer> framebuffer;
    FramebufferDesc attachments;
  };

  PFNIGLBEGINQUERYPROC beginQuery_ = iglBeginQuery;
  PFNIGLGETQUERYOBJECTIVPROC getQueryObjectiv_ = iglGetQueryObjectiv;
  PFNIGLGETQUERYOBJECTUI64VPROC getQueryObjectui64v_ = iglGetQueryObjectui64v;
  std::vector<GLuint> queryIds_;
  std::vector<bool> begun_;
  std::vector<RetainedFramebuffer> retainedFramebuffers_;
  uint32_t maxSlots_ = 0;
  uint32_t currentIndex_ = 0;
  bool valid_ = true; // set once in constructor
};

} // namespace igl::opengl
