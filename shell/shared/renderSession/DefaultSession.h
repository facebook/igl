/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

std::unique_ptr<RenderSession> createDefaultRenderSession(std::shared_ptr<Platform> platform);

} // namespace igl::shell
