/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "KeyCodeTranslator.h"

#include <igl/Config.h>

#if IGL_PLATFORM_APPLE
namespace {
enum KeyModifiers {
  kVkReturn = 0x24,
  kVkTab = 0x30,
  kVkDelete = 0x33,
  kVkEscape = 0x35,
  kVkShift = 0x38,
  kVkOption = 0x3A,
  kVkControl = 0x3B,
  kVkRightArrow = 0x7C,
  kVkLeftArrow = 0x7B,
  kVkDownArrow = 0x7D,
  kVkUpArrow = 0x7E,
  kVkForwardDelete = 0x75,
  kVkHome = 0x73,
  kVkEnd = 0x77,
  kVkPageUp = 0x74,
  kVkPageDown = 0x79,
};

[[maybe_unused]] ImGuiKey keyFromShellKeyEventApple(igl::shell::KeyEvent event) {
  int keyCode = event.key;

  switch (keyCode) {
  case kVkReturn:
    return ImGuiKey_Enter;
  case kVkTab:
    return ImGuiKey_Tab;
  case kVkDelete:
    return ImGuiKey_Backspace;
  case kVkForwardDelete:
    return ImGuiKey_Delete;
  case kVkEscape:
    return ImGuiKey_Escape;
  case kVkShift:
    return ImGuiKey_LeftShift;
  case kVkOption:
    return ImGuiKey_LeftAlt;
  case kVkControl:
    return ImGuiKey_LeftCtrl;
  case kVkLeftArrow:
    return ImGuiKey_LeftArrow;
  case kVkRightArrow:
    return ImGuiKey_RightArrow;
  case kVkUpArrow:
    return ImGuiKey_UpArrow;
  case kVkDownArrow:
    return ImGuiKey_DownArrow;
  case kVkHome:
    return ImGuiKey_Home;
  case kVkEnd:
    return ImGuiKey_End;
  case kVkPageUp:
    return ImGuiKey_PageUp;
  case kVkPageDown:
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
