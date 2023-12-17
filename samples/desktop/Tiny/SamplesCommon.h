/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>

namespace igl {

class IDevice;

namespace sample {

enum class Keys {
  Key_Unknown = -1,
  Key_SPACE,
  Key_APOSTROPHE,
  Key_COMMA,
  Key_MINUS,
  Key_PERIOD,
  Key_SLASH,
  Key_0,
  Key_1,
  Key_2,
  Key_3,
  Key_4,
  Key_5,
  Key_6,
  Key_7,
  Key_8,
  Key_9,
  Key_SEMICOLON,
  Key_EQUAL,
  Key_A,
  Key_B,
  Key_C,
  Key_D,
  Key_E,
  Key_F,
  Key_G,
  Key_H,
  Key_I,
  Key_J,
  Key_K,
  Key_L,
  Key_M,
  Key_N,
  Key_O,
  Key_P,
  Key_Q,
  Key_R,
  Key_S,
  Key_T,
  Key_U,
  Key_V,
  Key_W,
  Key_X,
  Key_Y,
  Key_Z,
  Key_LEFT_BRACKET,
  Key_BACKSLASH,
  Key_RIGHT_BRACKET,
  Key_GRAVE_ACCENT,
  Key_WORLD_1,
  Key_WORLD_2,

  Key_ESCAPE,
  Key_ENTER,
  Key_TAB,
  Key_BACKSPACE,
  Key_INSERT,
  Key_DELETE,
  Key_RIGHT,
  Key_LEFT,
  Key_DOWN,
  Key_UP,
  Key_PAGE_UP,
  Key_PAGE_DOWN,
  Key_HOME,
  Key_END,
  Key_CAPS_LOCK,
  Key_SCROLL_LOCK,
  Key_NUM_LOCK,
  Key_PRINT_SCREEN,
  Key_PAUSE,
  Key_F1,
  Key_F2,
  Key_F3,
  Key_F4,
  Key_F5,
  Key_F6,
  Key_F7,
  Key_F8,
  Key_F9,
  Key_F10,
  Key_F11,
  Key_F12,
  Key_F13,
  Key_F14,
  Key_F15,
  Key_F16,
  Key_F17,
  Key_F18,
  Key_F19,
  Key_F20,
  Key_F21,
  Key_F22,
  Key_F23,
  Key_F24,
  Key_F25,
  Key_KP_0,
  Key_KP_1,
  Key_KP_2,
  Key_KP_3,
  Key_KP_4,
  Key_KP_5,
  Key_KP_6,
  Key_KP_7,
  Key_KP_8,
  Key_KP_9,
  Key_KP_DECIMAL,
  Key_KP_DIVIDE,
  Key_KP_MULTIPLY,
  Key_KP_SUBTRACT,
  Key_KP_ADD,
  Key_KP_ENTER,
  Key_KP_EQUAL,
  Key_LEFT_SHIFT,
  Key_LEFT_CONTROL,
  Key_LEFT_ALT,
  Key_LEFT_SUPER,
  Key_RIGHT_SHIFT,
  Key_RIGHT_CONTROL,
  Key_RIGHT_ALT,
  Key_RIGHT_SUPER,
  Key_MENU,
};

enum class KeyMods {
  None = 0x00,
  Shift = 1 << 0,
  Alt = 1 << 1,
  Control = 1 << 2,
  Meta = 1 << 3,
};

enum class MouseButton { Unknown, Left, Right, Middle };

bool initWindow(const char* windowTitle,
                bool fullscreen,
                int* widthPtr,
                int* heightPtr,
                int* fbWidthPtr,
                int* fbHeightPtr);
void setDevice(IDevice* device);

void* getWindow();
void* getDisplay();
void* getContext();

bool isDone();
void update();
void shutdown();

double getTimeInSecs();

// Callbacks
using CallbackKeyboard = std::function<void(Keys key, bool pressed, KeyMods keyMods)>;
using CallbackMouseButton =
    std::function<void(MouseButton button, bool pressed, double xpos, double ypos)>;
using CallbackMousePos = std::function<void(double xpos, double ypos)>;

void setCallbackKeyboard(CallbackKeyboard callback);
void setCallbackMouseButton(CallbackMouseButton callback);
void setCallbackMousePos(CallbackMousePos callback);

} // namespace sample
} // namespace igl
