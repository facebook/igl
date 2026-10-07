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

// Two generations of webgpu.h are supported, told apart by a struct only one of them declares:
//   V1: Dawn d57bbdb7 (native builds).
//   V2: emdawnwebgpu v20260219.200501, i.e. Dawn 536c572a (Emscripten builds).
// Raw WGPU enum values differ between them, so never persist them.
#if defined(WGPU_INSTANCE_LIMITS_INIT)
#define IGL_WEBGPU_HEADER_V2 1
#elif defined(WGPU_INSTANCE_CAPABILITIES_INIT)
#define IGL_WEBGPU_HEADER_V2 0
#else
#error "igl/webgpu: unrecognized webgpu.h"
#endif

#if IGL_WEBGPU_HEADER_V2 != IGL_PLATFORM_EMSCRIPTEN
#error "igl/webgpu: the V2 webgpu.h is only supported with Emscripten and V1 only natively"
#endif

namespace igl::webgpu::compat {

#if IGL_WEBGPU_HEADER_V2
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
#if IGL_WEBGPU_HEADER_V2
  static constexpr WGPUInstanceFeatureName kTimedWaitAny = WGPUInstanceFeatureName_TimedWaitAny;
  if (timedWaitAny) {
    desc.requiredFeatureCount = 1;
    desc.requiredFeatures = &kTimedWaitAny;
  }
#else
  desc.capabilities.timedWaitAnyEnable = timedWaitAny ? 1u : 0u;
#endif
  const WGPUInstance instance = wgpuCreateInstance(&desc);
  if (outHasTimedWaitAny != nullptr) {
    *outHasTimedWaitAny = instance != nullptr && timedWaitAny;
  }
  return instance;
}

using QueueWorkDoneFn = void (*)(WGPUQueueWorkDoneStatus status,
                                 void* IGL_NULLABLE userdata1,
                                 void* IGL_NULLABLE userdata2);

/// V2 added a message parameter to the queue work-done callback; `Fn` never sees it.
template<QueueWorkDoneFn Fn>
[[nodiscard]] WGPUQueueWorkDoneCallbackInfo queueWorkDoneCallbackInfo(
    WGPUCallbackMode mode,
    void* IGL_NULLABLE userdata1,
    void* IGL_NULLABLE userdata2 = nullptr) {
  return {
      .nextInChain = nullptr,
      .mode = mode,
#if IGL_WEBGPU_HEADER_V2
      .callback = [](WGPUQueueWorkDoneStatus status,
                     WGPUStringView /*message*/,
                     void* IGL_NULLABLE data1,
                     void* IGL_NULLABLE data2) { Fn(status, data1, data2); },
#else
      .callback = [](WGPUQueueWorkDoneStatus status,
                     void* IGL_NULLABLE data1,
                     void* IGL_NULLABLE data2) { Fn(status, data1, data2); },
#endif
      .userdata1 = userdata1,
      .userdata2 = userdata2,
  };
}

} // namespace igl::webgpu::compat
