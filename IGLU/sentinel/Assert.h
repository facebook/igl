/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

#define IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert) \
  IGL_DEBUG_ASSERT(!(shouldAssert), "Sentinel implementation should NOT be reached")
