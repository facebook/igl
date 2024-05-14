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

using RenderSessionLoader = std::function<std::unique_ptr<igl::shell::RenderSession>(
    std::shared_ptr<igl::shell::Platform>)>;

} // namespace igl::shell
