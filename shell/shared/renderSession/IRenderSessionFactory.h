/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

namespace igl::shell {

class Platform;
class RenderSession;

class IRenderSessionFactory {
 public:
  virtual ~IRenderSessionFactory() noexcept = default;

  virtual std::unique_ptr<RenderSession> createRenderSession(
      std::shared_ptr<Platform> platform) noexcept = 0;
};

} // namespace igl::shell
