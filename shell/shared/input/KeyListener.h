/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>

namespace igl::shell {

enum {
  KeyEventModifierNone = 0,
  KeyEventModifierShift = 1 << 0,
  KeyEventModifierCapslock = 1 << 1,
  KeyEventModifierControl = 1 << 2,
  KeyEventModifierOption = 1 << 3,
  KeyEventModifierCommand = 1 << 4,
};

struct KeyEvent {
  int key;
  bool isDown;
  uint32_t modifiers;

  KeyEvent() = default;
  KeyEvent(bool isDown, int key, uint32_t modifiers = KeyEventModifierNone) :
    key(key), isDown(isDown), modifiers(modifiers) {}
};

class IKeyListener {
 public:
  virtual bool process(const KeyEvent& event) = 0;

  virtual ~IKeyListener() = default;
};

} // namespace igl::shell
