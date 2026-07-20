/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/shared/input/InputDispatcher.h>

#include <type_traits>
#include <shell/shared/input/MouseListener.h>
#include <shell/shared/input/TouchListener.h>

namespace igl::shell {

static_assert(std::is_trivially_copyable_v<MouseButtonEvent>);
static_assert(sizeof(MouseMotionEvent) == 16);
static_assert(std::is_trivially_copyable_v<MouseMotionEvent>);
static_assert(sizeof(MouseWheelEvent) == 8);
static_assert(std::is_trivially_copyable_v<MouseWheelEvent>);
static_assert(sizeof(TouchEvent) == 20);
static_assert(std::is_trivially_copyable_v<TouchEvent>);
static_assert(sizeof(KeyEvent) == 12);
static_assert(std::is_trivially_copyable_v<KeyEvent>);
static_assert(sizeof(CharEvent) == 4);
static_assert(std::is_trivially_copyable_v<CharEvent>);

void InputDispatcher::processEvents() {
  const std::lock_guard guard(mutex_);

  while (!events_.empty()) {
    auto& event = events_.front();

    if (event.type == EventType::MouseButton || event.type == EventType::MouseMotion ||
        event.type == EventType::MouseWheel) {
      for (auto& listener : mouseListeners_) {
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
      for (auto& listener : touchListeners_) {
        if (listener->process(std::get<TouchEvent>(event.data))) {
          break;
        }
      }
    } else if (event.type == EventType::Key) {
      for (auto& listener : keyListeners_) {
        if (listener->process(std::get<KeyEvent>(event.data))) {
          break;
        }
      }
    } else if (event.type == EventType::Char) {
      for (auto& listener : keyListeners_) {
        if (listener->process(std::get<CharEvent>(event.data))) {
          break;
        }
      }
    } else if (event.type == EventType::Ray) {
      for (auto& listener : rayListeners_) {
        if (listener->process(std::get<RayEvent>(event.data))) {
          break;
        }
      }
    } else if (event.type == EventType::Intent) {
      for (auto& listener : intentListeners_) {
        if (listener->process(std::get<IntentEvent>(event.data))) {
          break;
        }
      }
    }

    events_.pop();
  }
}

void InputDispatcher::addMouseListener(const std::shared_ptr<IMouseListener>& listener) {
  const std::lock_guard guard(mutex_);
  mouseListeners_.push_back(listener);
}

void InputDispatcher::removeMouseListener(const std::shared_ptr<IMouseListener>& listener) {
  const std::lock_guard guard(mutex_);
  for (size_t i = mouseListeners_.size(); i-- > 0;) {
    if (listener.get() == mouseListeners_[i].get()) {
      mouseListeners_.erase(mouseListeners_.begin() + i);
    }
  }
}

void InputDispatcher::addTouchListener(const std::shared_ptr<ITouchListener>& listener) {
  const std::lock_guard guard(mutex_);
  touchListeners_.push_back(listener);
}

void InputDispatcher::removeTouchListener(const std::shared_ptr<ITouchListener>& listener) {
  const std::lock_guard guard(mutex_);
  for (size_t i = touchListeners_.size(); i-- > 0;) {
    if (listener.get() == touchListeners_[i].get()) {
      touchListeners_.erase(touchListeners_.begin() + i);
    }
  }
}

void InputDispatcher::addKeyListener(const std::shared_ptr<IKeyListener>& listener) {
  const std::lock_guard guard(mutex_);
  keyListeners_.push_back(listener);
}

void InputDispatcher::removeKeyListener(const std::shared_ptr<IKeyListener>& listener) {
  const std::lock_guard guard(mutex_);
  for (size_t i = keyListeners_.size(); i-- > 0;) {
    if (listener.get() == keyListeners_[i].get()) {
      keyListeners_.erase(keyListeners_.begin() + i);
    }
  }
}

void InputDispatcher::addRayListener(const std::shared_ptr<IRayListener>& listener) {
  const std::lock_guard guard(mutex_);
  rayListeners_.push_back(listener);
}

void InputDispatcher::removeRayListener(const std::shared_ptr<IRayListener>& listener) {
  const std::lock_guard guard(mutex_);
  for (size_t i = rayListeners_.size(); i-- > 0;) {
    if (listener.get() == rayListeners_[i].get()) {
      rayListeners_.erase(rayListeners_.begin() + i);
    }
  }
}

void InputDispatcher::addIntentListener(const std::shared_ptr<IIntentListener>& listener) {
  const std::lock_guard guard(mutex_);
  intentListeners_.push_back(listener);
}

void InputDispatcher::removeIntentListener(const std::shared_ptr<IIntentListener>& listener) {
  const std::lock_guard guard(mutex_);
  for (size_t i = intentListeners_.size(); i-- > 0;) {
    if (listener.get() == intentListeners_[i].get()) {
      intentListeners_.erase(intentListeners_.begin() + i);
    }
  }
}

void InputDispatcher::queueEvent(const MouseButtonEvent& event) {
  const std::lock_guard guard(mutex_);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseButton;
  evt.data = event;
  events_.push(evt);
}

void InputDispatcher::queueEvent(const MouseMotionEvent& event) {
  const std::lock_guard guard(mutex_);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseMotion;
  evt.data = event;
  events_.push(evt);
}

void InputDispatcher::queueEvent(const MouseWheelEvent& event) {
  const std::lock_guard guard(mutex_);
  InputDispatcher::Event evt;
  evt.type = EventType::MouseWheel;
  evt.data = event;
  events_.push(evt);
}

void InputDispatcher::queueEvent(const TouchEvent& event) {
  const std::lock_guard guard(mutex_);

  InputDispatcher::Event evt;
  evt.type = EventType::Touch;
  evt.data = event;
  events_.push(evt);
}

void InputDispatcher::queueEvent(const RayEvent& event) {
  const std::lock_guard guard(mutex_);

  InputDispatcher::Event evt;
  evt.type = EventType::Ray;
  evt.data = event;
  events_.push(evt);
}

void InputDispatcher::queueEvent(const IntentEvent& event) {
  const std::lock_guard guard(mutex_);

  InputDispatcher::Event evt;
  evt.type = EventType::Intent;
  evt.data = event;
  events_.push(evt);
}

void InputDispatcher::queueEvent(const KeyEvent& event) {
  const std::lock_guard guard(mutex_);

  InputDispatcher::Event evt;
  evt.type = EventType::Key;
  evt.data = event;
  events_.push(evt);
}

void InputDispatcher::queueEvent(const CharEvent& event) {
  const std::lock_guard guard(mutex_);

  InputDispatcher::Event evt;
  evt.type = EventType::Char;
  evt.data = event;
  events_.push(evt);
}

} // namespace igl::shell
