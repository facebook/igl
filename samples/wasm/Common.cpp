/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Common.h"

#include <emscripten/html5.h>
#include <igl/Common.h>

void getRenderingBufferSize(int& width, int& height) {
  double cssWidth = 0.0;
  double cssHeight = 0.0;
  emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);

  double devicePixelRatio = emscripten_get_device_pixel_ratio();

  width = cssWidth * devicePixelRatio;
  height = cssHeight * devicePixelRatio;

  IGL_ASSERT_MSG(width > 0 && height > 0, "zero or negative size");
}
