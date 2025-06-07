/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <shell/shared/renderSession/IRenderSessionFactory.h>
#include <igl/Config.h>

namespace igl::shell {

#if IGL_PLATFORM_LINUX || IGL_PLATFORM_APPLE
__attribute__((weak))
#endif // IGL_PLATFORM_LINUX
std::unique_ptr<IRenderSessionFactory>
createDefaultRenderSessionFactory();

} // namespace igl::shell
