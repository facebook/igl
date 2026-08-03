/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/DestructionGuard.h>

#include <igl/Macros.h>
#include <igl/opengl/IContext.h>

namespace igl::opengl {

DestructionGuard::DestructionGuard(std::shared_ptr<IContext> context) :
  context_(std::move(context)) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (IGL_DEBUG_VERIFY(context_)) {
    ++context_->lockCount_;
  }
}

DestructionGuard::~DestructionGuard() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  if (context_) {
    --context_->lockCount_;
  }
}

} // namespace igl::opengl
