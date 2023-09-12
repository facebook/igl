/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/platform/Platform.h>

#include <memory>

namespace igl {
class IFramebuffer;
} // namespace igl

namespace igl::shell {
void SaveFrameBufferToPng(const char* absoluteFilename,
                          const std::shared_ptr<IFramebuffer>& framebuffer,
                          Platform& platform);

} // namespace igl::shell
