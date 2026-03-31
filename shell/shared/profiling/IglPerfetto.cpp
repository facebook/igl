/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifdef IGL_WITH_PERFETTO

#include <shell/shared/profiling/IglPerfetto.h>

#include <atomic>

// One-TU static storage for the category registry defined in IglPerfetto.h
PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE(igl::shell::profiling);

namespace igl::shell::profiling {

void initPerfetto() noexcept {
  perfetto::TracingInitArgs args;
  // kSystemBackend connects to the system traced daemon — compatible with
  // barebone `perfetto` CLI capture without any in-process session setup.
  args.backends = perfetto::kSystemBackend;
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

} // namespace igl::shell::profiling

#endif // IGL_WITH_PERFETTO
