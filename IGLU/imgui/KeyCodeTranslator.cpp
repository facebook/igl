/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "KeyCodeTranslator.h"

#include <igl/Macros.h>

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
};

IGL_MAYBE_UNUSED ImGuiKey keyFromShellKeyEventApple(igl::shell::KeyEvent event) {
  KeyModifiers keyCode = (KeyModifiers)event.key;

  switch (keyCode) {
  case KeyModifiers::kVK_Return:
    return ImGuiKey_Enter;
  case KeyModifiers::kVK_Tab:
    return ImGuiKey_Tab;
  case KeyModifiers::kVK_Delete:
    return ImGuiKey_Backspace;
  case KeyModifiers::kVK_Escape:
    return ImGuiKey_Escape;
  case KeyModifiers::kVK_Shift:
    return ImGuiKey_LeftShift;
  case KeyModifiers::kVK_Option:
    return ImGuiKey_LeftAlt;
  default:
    return (ImGuiKey)keyCode;
  }
}
} // namespace
#endif

namespace iglu::imgui {
ImGuiKey keyFromShellKeyEvent(igl::shell::KeyEvent event) {
#if IGL_PLATFORM_APPLE
  return keyFromShellKeyEventApple(event);
#endif
  return (ImGuiKey)event.key;
}
} // namespace iglu::imgui
