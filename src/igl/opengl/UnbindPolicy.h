/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace igl::opengl {

enum class UnbindPolicy : uint8_t {
  Default, // Do nothing
  EndScope, // Unbind at end of scope
  /// Unbinding a device when it's no longer scoped will clear the context from being current.
  ClearContext,
};

} // namespace igl::opengl
