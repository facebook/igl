/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/WithContext.h>

#include <igl/Core.h>
#include <igl/opengl/IContext.h>

namespace igl::opengl {

WithContext::WithContext(IContext& context) : context_(&context) {
  if (!context_->addRef()) {
    IGL_DEBUG_ABORT("Object created with an invalid IContext reference.");
  }
}

WithContext::~WithContext() {
  if (!context_->releaseRef()) {
    IGL_DEBUG_ABORT(
        "Object destroyed after the IContext."
        // @fb-only
    );
  }
}

IContext& WithContext::getContext() const {
  IGL_DEBUG_ASSERT(context_->isLikelyValidObject(),
                   "Accessing invalid IContext reference."
                   // @fb-only
  );
  return *context_;
}

} // namespace igl::opengl
