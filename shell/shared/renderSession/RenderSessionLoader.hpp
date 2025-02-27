/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <memory>

namespace igl::shell {
class RenderSession;
class Platform;

using RenderSessionLoader =
    std::function<std::unique_ptr<RenderSession>(std::shared_ptr<Platform>)>;

} // namespace igl::shell
