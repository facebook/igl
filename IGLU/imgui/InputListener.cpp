/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "InputListener.h"

// ImGui has a very awkward expectation when it comes to processing inputs and making decisions
// based on them. This is what it expects clients to do, in order, every frame:
// 1. Send ImGui all events via the input parameters in ImGuiIO.
// 2. Call ImGui::NewFrame -- that's when events are processed.
// 3. Read the output parameters of ImGuiIO to know which events it wants to capture.
// 4. Forward uncaptured events to other systems.
//
// This is an awkward expectation and we currently don't follow it. Instead, we process events
// before calling ImGui::NewFrame and immediately check whether ImGui wants to capture events, which
// is one frame old. This can be a source of problems if we have multiple input listeners and
// depending on how they process inputs.

namespace iglu::imgui {

InputListener::InputListener(ImGuiContext* context) {
  _context = context;
}

bool InputListener::process(const igl::shell::MouseButtonEvent& event) {
  makeCurrentContext();

  ImGuiIO& io = ImGui::GetIO();
  io.MousePos = ImVec2(event.x, event.y);
  io.MouseDown[event.button] = event.isDown;
  return io.WantCaptureMouse;
}

bool InputListener::process(const igl::shell::MouseMotionEvent& event) {
  makeCurrentContext();

  ImGuiIO& io = ImGui::GetIO();
  io.MousePos = ImVec2(event.x, event.y);
  return io.WantCaptureMouse;
}

bool InputListener::process(const igl::shell::MouseWheelEvent& event) {
  makeCurrentContext();

  ImGuiIO& io = ImGui::GetIO();
  io.MouseWheelH = event.dx;
  io.MouseWheel = event.dy;
  return io.WantCaptureMouse;
}

bool InputListener::process(const igl::shell::TouchEvent& event) {
  makeCurrentContext();

  ImGuiIO& io = ImGui::GetIO();
  io.MousePos = ImVec2(event.x, event.y);
  io.MouseDown[0] = event.isDown;
  return io.WantCaptureMouse;
}

void InputListener::makeCurrentContext() const {
  ImGui::SetCurrentContext(_context);
}

} // namespace iglu::imgui
