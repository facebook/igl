/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/platform/Platform.h>

namespace igl::shell {

std::shared_ptr<Platform> createPlatform(std::shared_ptr<IDevice> device);

} // namespace igl::shell
