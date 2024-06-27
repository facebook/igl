/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

namespace igl::opengl {

class IContext;

///
/// Guard class providing RAII-style mechanism for disabling destruction of GL-side objects when
/// IContext is destroyed. Destruction is disabled as long as client holds an instance of this
/// class.
///
class DestructionGuard final {
 public:
  DestructionGuard(std::shared_ptr<IContext> context);
  ~DestructionGuard();
  DestructionGuard(DestructionGuard&& context) = default;
  DestructionGuard& operator=(DestructionGuard&&) = default;
  DestructionGuard(const DestructionGuard&) = delete;
  DestructionGuard& operator=(const DestructionGuard&) = delete;

 private:
  std::shared_ptr<IContext> context_;
};

} // namespace igl::opengl
