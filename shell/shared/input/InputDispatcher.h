/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "KeyListener.h"
#include "MouseListener.h"
#include "TouchListener.h"

#include <memory>
#include <mutex>
#include <queue>
#include <vector>

namespace igl {
namespace shell {

class InputDispatcher {
 public:
  // Consumer methods
  void addMouseListener(const std::shared_ptr<IMouseListener>& listener);
  void removeMouseListener(const std::shared_ptr<IMouseListener>& listener);

  void addTouchListener(const std::shared_ptr<ITouchListener>& listener);
  void removeTouchListener(const std::shared_ptr<ITouchListener>& listener);

  void addKeyListener(const std::shared_ptr<IKeyListener>& listener);
  void removeKeyListener(const std::shared_ptr<IKeyListener>& listener);

  // Platform methods
  void queueEvent(const MouseButtonEvent& event);
  void queueEvent(const MouseMotionEvent& event);
  void queueEvent(const MouseWheelEvent& event);
  void queueEvent(const TouchEvent& event);
  void queueEvent(const KeyEvent& event);

  void processEvents();

 private:
  enum class EventType {
    // Mouse
    MouseButton,
    MouseMotion,
    MouseWheel,
    // Touch
    Touch,
    // Key
    Key,
  };

  struct Event {
    EventType type;
    union {
      MouseButtonEvent mouseButtonEvent;
      MouseMotionEvent mouseMotionEvent;
      MouseWheelEvent mouseWheelEvent;
      TouchEvent touchEvent;
      KeyEvent keyEvent;
    };
  };

  std::mutex _mutex;
  std::vector<std::shared_ptr<IMouseListener>> _mouseListeners;
  std::vector<std::shared_ptr<ITouchListener>> _touchListeners;
  std::vector<std::shared_ptr<IKeyListener>> _keyListeners;
  std::queue<Event> _events;
};

} // namespace shell
} // namespace igl
