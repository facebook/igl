/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifndef IGL_CORE_H
#define IGL_CORE_H

/**
 * @brief Core.h is a minimal header set for non-graphics utilities (e.g. logging, assert)
 *
 * This is useful for auxiliary modules in IGL that don't involve graphics (e.g. shell)
 */

// Define macros to disable checks
#if !defined(IGL_COMMON_SKIP_CHECK)
#define IGL_COMMON_SKIP_CHECK 1
#define IGL_CORE_H_COMMON_SKIP_CHECK 1 // Marker so we know to undefine at end of header
#endif // !defined(IGL_COMMON_SKIP_CHECK)

#include <igl/Assert.h>
#include <igl/Log.h>
#include <igl/Macros.h>

// Undefine macros that are local to this header
#if defined(IGL_CORE_H_COMMON_SKIP_CHECK)
#undef IGL_COMMON_SKIP_CHECK
#undef IGL_CORE_H_COMMON_SKIP_CHECK
#endif // defined(IGL_CORE_H_COMMON_SKIP_CHECK)

#include <igl/IGLFolly.h>

#endif // IGL_CORE_H
