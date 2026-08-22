/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <string>

namespace igl::shell {

enum {
  kKeyEventModifierNone = 0,
  kKeyEventModifierShift = 1 << 0,
  kKeyEventModifierControl = 1 << 1,
  kKeyEventModifierOption = 1 << 2,
  kKeyEventModifierCapsLock = 1 << 3,
  kKeyEventModifierNumLock = 1 << 4,
  kKeyEventModifierCommand = 1 << 5,
};

struct KeyEvent {
  int key = 0;
  bool isDown = false;
  uint32_t modifiers = 0;

  KeyEvent() = default;
  KeyEvent(bool isDown, int key, uint32_t modifiers = kKeyEventModifierNone) :
    key(key), isDown(isDown), modifiers(modifiers) {}
};

struct CharEvent {
  int character = 0;
};

class IKeyListener {
 public:
  virtual bool process(const KeyEvent& /*event*/) {
    return false;
  }
  virtual bool process(const CharEvent& /*event*/) {
    return false;
  }

  virtual ~IKeyListener() = default;
};

} // namespace igl::shell
