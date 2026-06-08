/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifdef IGL_WITH_PERFETTO

#include <shell/shared/profiling/IglPerfetto.h>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <utility>

// One-TU static storage for the category registry defined in IglPerfetto.h
PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE(igl::shell::profiling);

namespace igl::shell::profiling {

namespace {

#if !defined(__ANDROID__)
// State for the in-process tracing session. Only one session may be active.
// Function-local static so initialization is lazy (avoids the
// global-constructor lint and runs only when --perfetto is actually used).
struct SessionState {
  std::mutex mutex;
  std::unique_ptr<perfetto::TracingSession> session;
  std::string outputPath;
};

SessionState& sessionState() {
  // NOLINTNEXTLINE(facebook-static-object-destructor-check)
  static SessionState state;
  return state;
}

// Drain the session buffer to file. Caller owns the session and must have
// already called StopBlocking().
void drainSessionToFile(perfetto::TracingSession& session, const std::string& path) noexcept {
  const std::vector<char> buffer = session.ReadTraceBlocking();

  std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out) {
    fprintf(stderr, "[perfetto] failed to open output file: %s\n", path.c_str());
    return;
  }
  out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  // Explicit close() so a deferred-flush failure (e.g. disk full surfaced
  // during fclose()) is observable here, rather than swallowed by the
  // destructor below the success log.
  out.close();
  if (!out.good()) {
    fprintf(stderr, "[perfetto] failed to write trace to %s\n", path.c_str());
    return;
  }
  fprintf(
      stderr, "[perfetto] tracing stopped — wrote %zu bytes to %s\n", buffer.size(), path.c_str());
}
#endif

} // namespace

void initPerfetto() noexcept {
  perfetto::TracingInitArgs args;
#if defined(__ANDROID__)
  // kSystemBackend connects to the system traced daemon — compatible with
  // barebone `perfetto` CLI capture without any in-process session setup.
  args.backends = perfetto::kSystemBackend;
#else
  // Desktop/host: capture in-process and let startTraceToFile() write the
  // buffer directly to a .perfetto file. No external daemon required.
  args.backends = perfetto::kInProcessBackend;
#endif
  args.shmem_size_hint_kb = 4096;
  perfetto::Tracing::Initialize(args);
  TrackEvent::Register();
}

void markFrame(const char* name) noexcept {
  static std::atomic<int64_t> frameIndex{0};
  static const perfetto::CounterTrack kFrameTrack =
      perfetto::CounterTrack("IGL::FrameIndex").set_unit_name("frame");
  TRACE_COUNTER("igl.benchmark", kFrameTrack, frameIndex.fetch_add(1));
  TRACE_EVENT_INSTANT("igl.benchmark", ::perfetto::DynamicString{name});
}

#if !defined(__ANDROID__)

bool startTraceToFile(std::string outputPath) noexcept {
  auto& state = sessionState();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (state.session) {
    fprintf(stderr,
            "[perfetto] startTraceToFile called while a session is already active — ignoring\n");
    return false;
  }

  // Capture every registered TrackEvent category — no allowlist. Categories
  // registered after the session starts (via late TrackEvent::Register calls
  // from other libraries linked into the binary) are automatically included.
  perfetto::TraceConfig cfg;
  // 64 MB ring buffer.
  cfg.add_buffers()->set_size_kb(64 * 1024);
  auto* dataSource = cfg.add_data_sources();
  auto* dsCfg = dataSource->mutable_config();
  dsCfg->set_name("track_event");

  state.session = perfetto::Tracing::NewTrace();
  if (!state.session) {
    fprintf(stderr,
            "[perfetto] Tracing::NewTrace() returned null — "
            "was initPerfetto() called?\n");
    return false;
  }
  state.session->Setup(cfg);
  state.session->StartBlocking();
  state.outputPath = std::move(outputPath);
  // Intentionally do not log the output path here. The path may not be
  // writable (TCC, disk full, …) and we don't want to mislead the caller —
  // drainSessionToFile() prints the authoritative success/failure line.
  fprintf(stderr, "[perfetto] tracing started\n");
  return true;
}

void stopTrace() noexcept {
  auto& state = sessionState();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!state.session) {
    return;
  }
  state.session->StopBlocking();
  drainSessionToFile(*state.session, state.outputPath);
  state.session.reset();
  state.outputPath.clear();
}

bool isTracing() noexcept {
  auto& state = sessionState();
  std::lock_guard<std::mutex> lock(state.mutex);
  return state.session != nullptr;
}

#else // __ANDROID__

bool startTraceToFile(std::string /*outputPath*/) noexcept {
  // System backend — capture is driven externally via the on-device perfetto
  // CLI / traced daemon; no in-process session to start.
  return false;
}

void stopTrace() noexcept {
  // Same as above — no-op under the system backend.
}

bool isTracing() noexcept {
  // System backend has no in-process session to inspect.
  return false;
}

#endif // __ANDROID__

} // namespace igl::shell::profiling

#endif // IGL_WITH_PERFETTO
