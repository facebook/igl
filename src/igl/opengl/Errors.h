/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdio>
#include <igl/Common.h>

#if IGL_DEBUG
#ifndef IGL_GLCHECK
#define IGL_GLCHECK(f) \
  do {                 \
    f;                 \
  } while (false)
#endif
#else
#ifndef IGL_GLCHECK
#define IGL_GLCHECK(f) \
  do {                 \
    f;                 \
  } while (false)
#endif
#endif
