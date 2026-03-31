/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifdef IGL_WITH_PERFETTO

#include <perfetto.h>

// Category definitions live here so every TU that includes this header can
// call TRACE_EVENT / TRACE_EVENT_INSTANT etc. directly.
// PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE lives in IglPerfetto.cpp (one TU only).
PERFETTO_DEFINE_CATEGORIES_IN_NAMESPACE(
    igl::shell::profiling,
    perfetto::Category("igl").SetDescription("IGL shell CPU events"),
    perfetto::Category("igl.benchmark").SetDescription("IGL benchmark frame / scene events"));

PERFETTO_USE_CATEGORIES_FROM_NAMESPACE(igl::shell::profiling);

namespace igl::shell::profiling {

/// Initialize Perfetto tracing with kSystemBackend.
/// Call once at app startup before any IGL_PROFILER_* macros fire.
void initPerfetto() noexcept;

/// Bump the IGL::FrameIndex counter track and emit a frame-boundary instant.
/// Called automatically from RenderSession::runUpdate() when perfetto is enabled.
void markFrame(const char* name) noexcept;

} // namespace igl::shell::profiling

#endif // IGL_WITH_PERFETTO
