/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace igl::shell {
struct DepthParams {
  float minDepth = 0.f;
  float maxDepth = 1.f;
  float nearZ = 0.1f;
  float farZ = 100.f;
};
} // namespace igl::shell
