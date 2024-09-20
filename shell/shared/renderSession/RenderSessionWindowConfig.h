/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>

namespace igl::shell {

enum class WindowMode {
  Window,
  MaximizedWindow,
  Fullscreen,
};

struct RenderSessionWindowConfig {
  uint32_t width = 1024;
  uint32_t height = 768;
  WindowMode windowMode = WindowMode::Window;
};

} // namespace igl::shell
