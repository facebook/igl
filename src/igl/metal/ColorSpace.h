/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#import <CoreGraphics/CGColorSpace.h>
#include <igl/ColorSpace.h>

namespace igl::metal {
CGColorSpaceRef colorSpaceToCGColorSpace(ColorSpace colorSpace);
} // namespace igl::metal
