/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace igl::shell {
enum class RenderMode {
  Mono,
  DualPassStereo, // Render each eye separately
  SinglePassStereo, // Render both eyes at the same time
};
}
