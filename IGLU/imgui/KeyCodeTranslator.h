/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "imgui.h"
#include <shell/shared/input/KeyListener.h>

namespace iglu::imgui {
ImGuiKey keyFromShellKeyEvent(igl::shell::KeyEvent event);
} // namespace iglu::imgui
