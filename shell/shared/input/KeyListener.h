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
  KeyEventModifierNone = 0,
  KeyEventModifierShift = 1 << 0,
  KeyEventModifierControl = 1 << 1,
  KeyEventModifierOption = 1 << 2,
  KeyEventModifierCapsLock = 1 << 3,
  KeyEventModifierNumLock = 1 << 4,
  KeyEventModifierCommand = 1 << 5,
};

struct KeyEvent {
  int key;
  bool isDown;
  uint32_t modifiers;

  KeyEvent() = default;
  KeyEvent(bool isDown, int key, uint32_t modifiers = KeyEventModifierNone) :
    key(key), isDown(isDown), modifiers(modifiers) {}
};

struct CharEvent {
  int character;
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
