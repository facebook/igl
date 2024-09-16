/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/DefaultRenderSessionFactory.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/IRenderSessionFactory.h>

namespace igl::shell {
namespace {
class TransitionRenderSessionFactory final : public IRenderSessionFactory {
 public:
  std::unique_ptr<RenderSession> createRenderSession(
      std::shared_ptr<Platform> platform) noexcept final {
    return createDefaultRenderSession(std::move(platform));
  }
};
} // namespace

std::unique_ptr<IRenderSessionFactory> createDefaultRenderSessionFactory() {
  return std::make_unique<TransitionRenderSessionFactory>();
}

} // namespace igl::shell
