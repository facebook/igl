/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "InputDispatcher.h"
#include "MouseListener.h"
#include "TouchListener.h"

namespace igl {
namespace shell {

void InputDispatcher::processEvents() {
  std::lock_guard<std::mutex> guard(_mutex);

  while (!_events.empty()) {
    auto& event = _events.front();

    if (event.type == EventType::MouseButton || event.type == EventType::MouseMotion ||
        event.type == EventType::MouseWheel) {
      for (auto& listener : _mouseListeners) {
        if (event.type == EventType::MouseButton && listener->process(event.mouseButtonEvent)) {
          break;
        }
        if (event.type == EventType::MouseMotion && listener->process(event.mouseMotionEvent)) {
          break;
        }
        if (event.type == EventType::MouseWheel && listener->process(event.mouseWheelEvent)) {
          break;
        }
      }
    } else if (event.type == EventType::Touch) {
      for (auto& listener : _touchListeners) {
        if (event.type == EventType::Touch && listener->process(event.touchEvent)) {
          break;
        }
      }
    }

    _events.pop();
  }
}

void InputDispatcher::addMouseListener(const std::shared_ptr<IMouseListener>& listener) {
  std::lock_guard<std::mutex> guard(_mutex);
  _mouseListeners.push_back(listener);
}

void InputDispatcher::removeMouseListener(const std::shared_ptr<IMouseListener>& listener) {
  std::lock_guard<std::mutex> guard(_mutex);
  for (int i = _mouseListeners.size() - 1; i >= 0; --i) {
    if (listener.get() == _mouseListeners[i].get()) {
      _mouseListeners.erase(_mouseListeners.begin() + i);
    }
  }
}

void InputDispatcher::addTouchListener(const std::shared_ptr<ITouchListener>& listener) {
  std::lock_guard<std::mutex> guard(_mutex);
  _touchListeners.push_back(listener);
}

void InputDispatcher::removeTouchListener(const std::shared_ptr<ITouchListener>& listener) {
  std::lock_guard<std::mutex> guard(_mutex);
  for (int i = _touchListeners.size() - 1; i >= 0; --i) {
    if (listener.get() == _touchListeners[i].get()) {
      _touchListeners.erase(_touchListeners.begin() + i);
    }
  }
}

void InputDispatcher::queueEvent(const MouseButtonEvent& event) {
  std::lock_guard<std::mutex> guard(_mutex);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseButton;
  evt.mouseButtonEvent = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const MouseMotionEvent& event) {
  std::lock_guard<std::mutex> guard(_mutex);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseMotion;
  evt.mouseMotionEvent = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const MouseWheelEvent& event) {
  std::lock_guard<std::mutex> guard(_mutex);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseWheel;
  evt.mouseWheelEvent = event;
  _events.push(evt);
}

void InputDispatcher::queueEvent(const TouchEvent& event) {
  std::lock_guard<std::mutex> guard(_mutex);

  InputDispatcher::Event evt;
  evt.type = EventType::Touch;
  evt.touchEvent = event;
  _events.push(evt);
}

} // namespace shell
} // namespace igl
