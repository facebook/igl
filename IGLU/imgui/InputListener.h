/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <shell/shared/input/InputDispatcher.h>
#include <shell/shared/input/MouseListener.h>

#include "imgui.h"

namespace iglu::imgui {

class InputListener : public igl::shell::IMouseListener, public igl::shell::ITouchListener {
 public:
  explicit InputListener(ImGuiContext* context);
  ~InputListener() override = default;

 protected:
  bool process(const igl::shell::MouseButtonEvent& event) override;
  bool process(const igl::shell::MouseMotionEvent& event) override;
  bool process(const igl::shell::MouseWheelEvent& event) override;
  bool process(const igl::shell::TouchEvent& event) override;

 private:
  ImGuiContext* _context;

  void makeCurrentContext() const;
};

} // namespace iglu::imgui
