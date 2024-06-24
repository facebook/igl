/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory.h>

namespace igl::opengl {
class IContext;

class WithContext {
 public:
  explicit WithContext(IContext& context);
  virtual ~WithContext();

  // This type is not copyable.
  WithContext(const WithContext&) = delete;
  WithContext& operator=(const WithContext&) = delete;

  [[nodiscard]] IContext& getContext() const;

 private:
  IContext* context_;
};

} // namespace igl::opengl
