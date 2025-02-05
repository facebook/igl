/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

// NOTE: Only include this in the Obj-C++ implementation (.mm) file
// Use the public header in pure Obj-C files

#include "IglShellPlatformAdapter.h"

#include <shell/shared/platform/Platform.h>

struct IglShellPlatformAdapter {
  igl::shell::Platform* platform;
};
