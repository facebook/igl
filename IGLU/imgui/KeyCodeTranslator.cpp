/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "KeyCodeTranslator.h"

#include <igl/Config.h>

#if IGL_PLATFORM_APPLE
namespace {
enum KeyModifiers {
  kVK_Return = 0x24,
  kVK_Tab = 0x30,
  kVK_Delete = 0x33,
  kVK_Escape = 0x35,
  kVK_Shift = 0x38,
  kVK_Option = 0x3A,
  kVK_Control = 0x3B,
  kVK_RightArrow = 0x7C,
  kVK_LeftArrow = 0x7B,
  kVK_DownArrow = 0x7D,
  kVK_UpArrow = 0x7E,
  kVK_ForwardDelete = 0x75,
  kVK_Home = 0x73,
  kVK_End = 0x77,
  kVK_PageUp = 0x74,
  kVK_PageDown = 0x79,
};

[[maybe_unused]] ImGuiKey keyFromShellKeyEventApple(igl::shell::KeyEvent event) {
  int keyCode = event.key;

  switch (keyCode) {
  case kVK_Return:
    return ImGuiKey_Enter;
  case kVK_Tab:
    return ImGuiKey_Tab;
  case kVK_Delete:
    return ImGuiKey_Backspace;
  case kVK_ForwardDelete:
    return ImGuiKey_Delete;
  case kVK_Escape:
    return ImGuiKey_Escape;
  case kVK_Shift:
    return ImGuiKey_LeftShift;
  case kVK_Option:
    return ImGuiKey_LeftAlt;
  case kVK_Control:
    return ImGuiKey_LeftCtrl;
  case kVK_LeftArrow:
    return ImGuiKey_LeftArrow;
  case kVK_RightArrow:
    return ImGuiKey_RightArrow;
  case kVK_UpArrow:
    return ImGuiKey_UpArrow;
  case kVK_DownArrow:
    return ImGuiKey_DownArrow;
  case kVK_Home:
    return ImGuiKey_Home;
  case kVK_End:
    return ImGuiKey_End;
  case kVK_PageUp:
    return ImGuiKey_PageUp;
  case kVK_PageDown:
    return ImGuiKey_PageDown;
  default:
    return ImGuiKey_None;
  }
}
} // namespace
#endif

namespace iglu::imgui {
ImGuiKey keyFromShellKeyEvent(igl::shell::KeyEvent event) {
#if IGL_PLATFORM_APPLE
  return keyFromShellKeyEventApple(event);
#else
  // For non-Apple platforms, return ImGuiKey_None for unmapped keys
  // to avoid passing invalid key codes to ImGui
  return ImGuiKey_None;
#endif
}
} // namespace iglu::imgui
