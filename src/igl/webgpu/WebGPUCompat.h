/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <webgpu/webgpu.h>
#include <igl/Config.h>
#include <igl/IGLFolly.h>

#if IGL_PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

// Supported webgpu.h versions:
//   Dawn 42d0e90f (native builds).
//   emdawnwebgpu v20260219.200501, i.e. Dawn 536c572a (Emscripten builds).
// Raw WGPU enum values differ between them, so never persist them.
#if !defined(WGPU_INSTANCE_LIMITS_INIT)
#error "igl/webgpu: unrecognized webgpu.h"
#endif

namespace igl::webgpu::compat {

#if IGL_PLATFORM_EMSCRIPTEN
inline constexpr WGPUFeatureName kUnorm16TextureFormatsFeature =
    WGPUFeatureName_TextureFormatsTier1;
#else
inline constexpr WGPUFeatureName kUnorm16TextureFormatsFeature =
    WGPUFeatureName_Unorm16TextureFormats;
#endif

/// Creates an instance, requesting timed WaitAny() when `requestTimedWaitAny` is true. Emscripten
/// builds get it only with JSPI or Asyncify; requesting it otherwise makes instance creation fail.
[[nodiscard]] inline WGPUInstance IGL_NULLABLE
createInstance(bool requestTimedWaitAny, bool* IGL_NULLABLE outHasTimedWaitAny) {
#if IGL_PLATFORM_EMSCRIPTEN
  const bool timedWaitAny = requestTimedWaitAny && emscripten_has_asyncify() != 0;
#else
  const bool timedWaitAny = requestTimedWaitAny;
#endif
  WGPUInstanceDescriptor desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
  static constexpr WGPUInstanceFeatureName kTimedWaitAny = WGPUInstanceFeatureName_TimedWaitAny;
  if (timedWaitAny) {
    desc.requiredFeatureCount = 1;
    desc.requiredFeatures = &kTimedWaitAny;
  }
  const WGPUInstance instance = wgpuCreateInstance(&desc);
  if (outHasTimedWaitAny != nullptr) {
    *outHasTimedWaitAny = instance != nullptr && timedWaitAny;
  }
  return instance;
}

using QueueWorkDoneFn = void (*)(WGPUQueueWorkDoneStatus status,
                                 void* IGL_NULLABLE userdata1,
                                 void* IGL_NULLABLE userdata2);

/// Adapts `Fn` to the queue work-done callback, whose message parameter `Fn` never sees.
template<QueueWorkDoneFn Fn>
[[nodiscard]] WGPUQueueWorkDoneCallbackInfo queueWorkDoneCallbackInfo(
    WGPUCallbackMode mode,
    void* IGL_NULLABLE userdata1,
    void* IGL_NULLABLE userdata2 = nullptr) {
  return {
      .nextInChain = nullptr,
      .mode = mode,
      .callback = [](WGPUQueueWorkDoneStatus status,
                     WGPUStringView /*message*/,
                     void* IGL_NULLABLE data1,
                     void* IGL_NULLABLE data2) { Fn(status, data1, data2); },
      .userdata1 = userdata1,
      .userdata2 = userdata2,
  };
}

} // namespace igl::webgpu::compat
