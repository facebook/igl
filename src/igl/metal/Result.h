/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <Metal/Metal.h>
#include <igl/IGL.h>

namespace igl::metal {

IGL_INLINE void setResultFrom(Result* outResult, const NSError* error) {
  if (outResult != nullptr) {
    if (error != nil) {
      outResult->code = Result::Code::RuntimeError;
      const char* message = [error.localizedDescription UTF8String];
      outResult->message = (message ? message : "");
    } else {
      outResult->code = Result::Code::Ok;
      outResult->message = "";
    }
  }
}

} // namespace igl::metal
