/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifdef IGL_WITH_PERFETTO

#include <perfetto.h>
#include <string>

// Category definitions live here so every TU that includes this header can
// call TRACE_EVENT / TRACE_EVENT_INSTANT etc. directly.
// PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE lives in IglPerfetto.cpp (one TU only).
PERFETTO_DEFINE_CATEGORIES_IN_NAMESPACE(
    igl::shell::profiling,
    perfetto::Category("igl").SetDescription("IGL shell CPU events"),
    perfetto::Category("igl.benchmark").SetDescription("IGL benchmark frame / scene events"));

PERFETTO_USE_CATEGORIES_FROM_NAMESPACE(igl::shell::profiling);

namespace igl::shell::profiling {

/// Initialize Perfetto tracing.
/// Backend is chosen at compile time:
///   - Android: kSystemBackend (capture via on-device traced daemon).
///   - Other platforms: kInProcessBackend (self-contained capture, written to
///     a file by stopTrace() — no traced daemon required).
/// Idempotent; only the first call configures the SDK.
/// Safe to call regardless of whether a trace session will be started.
void initPerfetto() noexcept;

/// Bump the IGL::FrameIndex counter track and emit a frame-boundary instant.
/// Called automatically from RenderSession::runUpdate() when perfetto is enabled.
void markFrame(const char* name) noexcept;

/// Start an in-process Perfetto trace session that captures every registered
/// TrackEvent category. The buffer is held in memory and flushed to
/// `outputPath` on stopTrace(). At most one session may be active at a time;
/// a second start while one is running is rejected.
///
/// The session runs until stopTrace() is called. Callers are responsible for
/// their own timing (e.g. a background thread that sleeps for a duration,
/// or a shutdown notification).
///
/// Returns `true` iff a new session was started and is now active. Returns
/// `false` on every failure path (already-active session, internal SDK
/// rejection, etc.); diagnostics are also logged to stderr. Always returns
/// `false` under the Android system backend, where there is no in-process
/// session to start.
///
/// Available on platforms using kInProcessBackend (i.e. non-Android).
bool startTraceToFile(std::string outputPath) noexcept;

/// Stop the active in-process session, flush the buffer, and write it to the
/// path passed to startTraceToFile(). Idempotent — calling without a matching
/// start, or calling twice, is a no-op. Safe to call from a shutdown
/// notification (e.g. `std::atexit`). Not async-signal-safe — do not call
/// from a POSIX signal handler.
void stopTrace() noexcept;

/// True iff startTraceToFile() has been called and stopTrace() hasn't yet
/// completed. Always false under the Android system backend (no in-process
/// session to query). Thread-safe.
bool isTracing() noexcept;

} // namespace igl::shell::profiling

#endif // IGL_WITH_PERFETTO
