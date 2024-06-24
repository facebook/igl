/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "InputDispatcher.h"
#include "MouseListener.h"
#include "TouchListener.h"

namespace igl::shell {

void InputDispatcher::processEvents() {
  const std::lock_guard guard(_mutex);

  while (!_events.empty()) {
    auto& event = _events.front();

    if (event.type == EventType::MouseButton || event.type == EventType::MouseMotion ||
        event.type == EventType::MouseWheel) {
      for (auto& listener : _mouseListeners) {
        if (event.type == EventType::MouseButton &&
            listener->process(std::get<MouseButtonEvent>(event.data))) {
          break;
        }
        if (event.type == EventType::MouseMotion &&
            listener->process(std::get<MouseMotionEvent>(event.data))) {
          break;
        }
        if (event.type == EventType::MouseWheel &&
            listener->process(std::get<MouseWheelEvent>(event.data))) {
          break;
        }
      }
    } else if (event.type == EventType::Touch) {
      for (auto& listener : _touchListeners) {
        if (listener->process(std::get<TouchEvent>(event.data))) {
          break;
        }
      }
    } else if (event.type == EventType::Key) {
      for (auto& listener : _keyListeners) {
        if (listener->process(std::get<KeyEvent>(event.data))) {
          break;
        }
      }
    } else if (event.type == EventType::Ray) {
      for (auto& listener : _rayListeners) {
        if (listener->process(std::get<RayEvent>(event.data))) {
          break;
        }
      }
    } else if (event.type == EventType::Intent) {
      for (auto& listener : _intentListeners) {
        if (listener->process(std::get<IntentEvent>(event.data))) {
          break;
        }
      }
    }

    _events.pop();
  }
}

void InputDispatcher::addMouseListener(const std::shared_ptr<IMouseListener>& listener) {
  const std::lock_guard guard(_mutex);
  _mouseListeners.push_back(listener);
}

void InputDispatcher::removeMouseListener(const std::shared_ptr<IMouseListener>& listener) {
  const std::lock_guard guard(_mutex);
  for (int i = _mouseListeners.size() - 1; i >= 0; --i) {
    if (listener.get() == _mouseListeners[i].get()) {
      _mouseListeners.erase(_mouseListeners.begin() + i);
    }
  }
}

void InputDispatcher::addTouchListener(const std::shared_ptr<ITouchListener>& listener) {
  const std::lock_guard guard(_mutex);
  _touchListeners.push_back(listener);
}

void InputDispatcher::removeTouchListener(const std::shared_ptr<ITouchListener>& listener) {
  const std::lock_guard guard(_mutex);
  for (int i = _touchListeners.size() - 1; i >= 0; --i) {
    if (listener.get() == _touchListeners[i].get()) {
      _touchListeners.erase(_touchListeners.begin() + i);
    }
  }
}

void InputDispatcher::addKeyListener(const std::shared_ptr<IKeyListener>& listener) {
  const std::lock_guard guard(_mutex);
  _keyListeners.push_back(listener);
}

void InputDispatcher::removeKeyListener(const std::shared_ptr<IKeyListener>& listener) {
  const std::lock_guard guard(_mutex);
  for (int i = _keyListeners.size() - 1; i >= 0; --i) {
    if (listener.get() == _keyListeners[i].get()) {
      _keyListeners.erase(_keyListeners.begin() + i);
    }
  }
}

void InputDispatcher::addRayListener(const std::shared_ptr<IRayListener>& listener) {
  const std::lock_guard guard(_mutex);
  _rayListeners.push_back(listener);
}

void InputDispatcher::removeRayListener(const std::shared_ptr<IRayListener>& listener) {
  const std::lock_guard guard(_mutex);
  for (int i = _rayListeners.size() - 1; i >= 0; --i) {
    if (listener.get() == _rayListeners[i].get()) {
      _rayListeners.erase(_rayListeners.begin() + i);
    }
  }
}

void InputDispatcher::addIntentListener(const std::shared_ptr<IIntentListener>& listener) {
  const std::lock_guard guard(_mutex);
  _intentListeners.push_back(listener);
}

void InputDispatcher::removeIntentListener(const std::shared_ptr<IIntentListener>& listener) {
  const std::lock_guard guard(_mutex);
  for (int i = _rayListeners.size() - 1; i >= 0; --i) {
    if (listener.get() == _intentListeners[i].get()) {
      _intentListeners.erase(_intentListeners.begin() + i);
    }
  }
}

void InputDispatcher::queueEvent(const MouseButtonEvent& event) {
  const std::lock_guard guard(_mutex);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseButton;
  evt.data = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const MouseMotionEvent& event) {
  const std::lock_guard guard(_mutex);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseMotion;
  evt.data = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const MouseWheelEvent& event) {
  const std::lock_guard guard(_mutex);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseWheel;
  evt.data = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const TouchEvent& event) {
  const std::lock_guard guard(_mutex);

  InputDispatcher::Event evt;
  evt.type = EventType::Touch;
  evt.data = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const RayEvent& event) {
  const std::lock_guard guard(_mutex);

  InputDispatcher::Event evt;
  evt.type = EventType::Ray;
  evt.data = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const IntentEvent& event) {
  const std::lock_guard guard(_mutex);

  InputDispatcher::Event evt;
  evt.type = EventType::Intent;
  evt.data = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const KeyEvent& event) {
  const std::lock_guard guard(_mutex);

  InputDispatcher::Event evt;
  evt.type = EventType::Key;
  evt.data = event;
  _events.push(evt);
}

} // namespace igl::shell
