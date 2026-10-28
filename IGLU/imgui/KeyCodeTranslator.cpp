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
#else
namespace {
// GLFW's key codes, which the shells that send key events off Apple use.
enum GlfwKeys {
  kGlfwKeySpace = 32,
  kGlfwKey0 = 48,
  kGlfwKey9 = 57,
  kGlfwKeyA = 65,
  kGlfwKeyZ = 90,
  kGlfwKeyEscape = 256,
  kGlfwKeyEnter = 257,
  kGlfwKeyTab = 258,
  kGlfwKeyBackspace = 259,
  kGlfwKeyInsert = 260,
  kGlfwKeyDelete = 261,
  kGlfwKeyRight = 262,
  kGlfwKeyLeft = 263,
  kGlfwKeyDown = 264,
  kGlfwKeyUp = 265,
  kGlfwKeyPageUp = 266,
  kGlfwKeyPageDown = 267,
  kGlfwKeyHome = 268,
  kGlfwKeyEnd = 269,
  kGlfwKeyF1 = 290,
  kGlfwKeyF12 = 301,
  kGlfwKeyKpEnter = 335,
  kGlfwKeyLeftShift = 340,
  kGlfwKeyLeftControl = 341,
  kGlfwKeyLeftAlt = 342,
  kGlfwKeyLeftSuper = 343,
  kGlfwKeyRightShift = 344,
  kGlfwKeyRightControl = 345,
  kGlfwKeyRightAlt = 346,
  kGlfwKeyRightSuper = 347,
};

ImGuiKey keyFromShellKeyEventGlfw(igl::shell::KeyEvent event) {
  const int keyCode = event.key;
  if (keyCode >= kGlfwKey0 && keyCode <= kGlfwKey9) {
    return static_cast<ImGuiKey>(ImGuiKey_0 + (keyCode - kGlfwKey0));
  }
  if (keyCode >= kGlfwKeyA && keyCode <= kGlfwKeyZ) {
    return static_cast<ImGuiKey>(ImGuiKey_A + (keyCode - kGlfwKeyA));
  }
  if (keyCode >= kGlfwKeyF1 && keyCode <= kGlfwKeyF12) {
    return static_cast<ImGuiKey>(ImGuiKey_F1 + (keyCode - kGlfwKeyF1));
  }

  switch (keyCode) {
  case kGlfwKeySpace:
    return ImGuiKey_Space;
  case kGlfwKeyEscape:
    return ImGuiKey_Escape;
  case kGlfwKeyEnter:
    return ImGuiKey_Enter;
  case kGlfwKeyKpEnter:
    return ImGuiKey_KeypadEnter;
  case kGlfwKeyTab:
    return ImGuiKey_Tab;
  case kGlfwKeyBackspace:
    return ImGuiKey_Backspace;
  case kGlfwKeyInsert:
    return ImGuiKey_Insert;
  case kGlfwKeyDelete:
    return ImGuiKey_Delete;
  case kGlfwKeyRight:
    return ImGuiKey_RightArrow;
  case kGlfwKeyLeft:
    return ImGuiKey_LeftArrow;
  case kGlfwKeyDown:
    return ImGuiKey_DownArrow;
  case kGlfwKeyUp:
    return ImGuiKey_UpArrow;
  case kGlfwKeyPageUp:
    return ImGuiKey_PageUp;
  case kGlfwKeyPageDown:
    return ImGuiKey_PageDown;
  case kGlfwKeyHome:
    return ImGuiKey_Home;
  case kGlfwKeyEnd:
    return ImGuiKey_End;
  case kGlfwKeyLeftShift:
    return ImGuiKey_LeftShift;
  case kGlfwKeyLeftControl:
    return ImGuiKey_LeftCtrl;
  case kGlfwKeyLeftAlt:
    return ImGuiKey_LeftAlt;
  case kGlfwKeyLeftSuper:
    return ImGuiKey_LeftSuper;
  case kGlfwKeyRightShift:
    return ImGuiKey_RightShift;
  case kGlfwKeyRightControl:
    return ImGuiKey_RightCtrl;
  case kGlfwKeyRightAlt:
    return ImGuiKey_RightAlt;
  case kGlfwKeyRightSuper:
    return ImGuiKey_RightSuper;
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
  return keyFromShellKeyEventGlfw(event);
#endif
}
} // namespace iglu::imgui
