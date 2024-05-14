/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// NOTE: Only include this in the Obj-C++ implementation (.mm) file
// Use the public header in pure Obj-C files

#include "IglSurfaceTexturesAdapter.h"

#include <igl/Texture.h>

struct IglSurfaceTexturesAdapter {
  igl::SurfaceTextures surfaceTextures;
};
