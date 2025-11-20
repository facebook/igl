/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if !defined(IGL_SHELL_SESSION)
#error "IGL_SHELL_SESSION must be defined";
#endif

#define IGL_SHELL_PATH <shell/renderSessions/IGL_SHELL_SESSION.h>

#include IGL_SHELL_PATH
#include <shell/shared/renderSession/DefaultRenderSessionFactory.h>

namespace igl::shell {

class RenderSessionFactory final : public IRenderSessionFactory {
 public:
  std::unique_ptr<RenderSession> createRenderSession(
      std::shared_ptr<Platform> platform) noexcept final;
};
std::unique_ptr<IRenderSessionFactory> createDefaultRenderSessionFactory() {
  return std::make_unique<RenderSessionFactory>();
}
std::unique_ptr<RenderSession> RenderSessionFactory::createRenderSession(
    std::shared_ptr<Platform> platform) noexcept {
  return std::make_unique<IGL_SHELL_SESSION>(std::move(platform));
}

} // namespace igl::shell
