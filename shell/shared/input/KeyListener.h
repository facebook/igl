/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace igl::shell {

struct KeyEvent {
  int key;
  bool isDown;

  KeyEvent() = default;
  KeyEvent(bool isDown, int key) : key(key), isDown(isDown) {}
};

class IKeyListener {
 public:
  virtual bool process(const KeyEvent& event) = 0;

  virtual ~IKeyListener() = default;
};

} // namespace igl::shell
