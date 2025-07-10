/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/input/IntentListener.h>
#include <shell/shared/input/KeyListener.h>
#include <shell/shared/input/MouseListener.h>
#include <shell/shared/input/RayListener.h>
#include <shell/shared/input/TouchListener.h>

#include <memory>
#include <mutex>
#include <queue>
#include <variant>
#include <vector>

namespace igl::shell {

class InputDispatcher {
 public:
  // Consumer methods
  void addMouseListener(const std::shared_ptr<IMouseListener>& listener);
  void removeMouseListener(const std::shared_ptr<IMouseListener>& listener);

  void addTouchListener(const std::shared_ptr<ITouchListener>& listener);
  void removeTouchListener(const std::shared_ptr<ITouchListener>& listener);

  void addKeyListener(const std::shared_ptr<IKeyListener>& listener);
  void removeKeyListener(const std::shared_ptr<IKeyListener>& listener);

  void addRayListener(const std::shared_ptr<IRayListener>& listener);
  void removeRayListener(const std::shared_ptr<IRayListener>& listener);

  void addIntentListener(const std::shared_ptr<IIntentListener>& listener);
  void removeIntentListener(const std::shared_ptr<IIntentListener>& listener);

  // Platform methods
  void queueEvent(const MouseButtonEvent& event);
  void queueEvent(const MouseMotionEvent& event);
  void queueEvent(const MouseWheelEvent& event);
  void queueEvent(const TouchEvent& event);
  void queueEvent(const KeyEvent& event);
  void queueEvent(const CharEvent& event);
  void queueEvent(const RayEvent& event);
  void queueEvent(const IntentEvent& event);

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
    Char,
    // Ray
    Ray,
    // Intent
    Intent,
  };

  struct Event {
    EventType type;
    using Data = std::variant<MouseButtonEvent,
                              MouseMotionEvent,
                              MouseWheelEvent,
                              TouchEvent,
                              KeyEvent,
                              CharEvent,
                              RayEvent,
                              IntentEvent>;
    Data data;
  };

  std::mutex mutex_;
  std::vector<std::shared_ptr<IMouseListener>> mouseListeners_;
  std::vector<std::shared_ptr<ITouchListener>> touchListeners_;
  std::vector<std::shared_ptr<IKeyListener>> keyListeners_;
  std::vector<std::shared_ptr<IRayListener>> rayListeners_;
  std::vector<std::shared_ptr<IIntentListener>> intentListeners_;
  std::queue<Event> events_;
};

} // namespace igl::shell
