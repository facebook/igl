/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/WebGPUContext.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <igl/webgpu/DepthUploader.h>
#include <igl/webgpu/MipmapGenerator.h>
#include <igl/webgpu/ResourcesBinder.h>
#include <igl/webgpu/UniformArena.h>
#include <igl/webgpu/WebGPUCompat.h>

// @fb-only
// @fb-only
// @fb-only
// @fb-only

namespace igl::webgpu {

namespace {

void installProcs() {
// @fb-only
  // @fb-only
  // @fb-only
// @fb-only
}

WGPUBackendType getPlatformDefaultBackendType() {
#if IGL_PLATFORM_APPLE
  return WGPUBackendType_Metal;
#elif IGL_PLATFORM_WINDOWS
  return WGPUBackendType_D3D12;
#elif IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
  return WGPUBackendType_Vulkan;
#else
  return WGPUBackendType_Undefined;
#endif
}

// A WaitAnyOnly callback can still run after its wait timed out (at the latest when the instance
// is released), so it owns a reference to its state instead of pointing into the waiting frame.
template<typename State>
void* IGL_NONNULL retainForCallback(const std::shared_ptr<State>& state) {
  return new std::shared_ptr<State>(state);
}

template<typename State>
std::unique_ptr<std::shared_ptr<State>> adoptFromCallback(void* IGL_NULLABLE userdata) {
  return std::unique_ptr<std::shared_ptr<State>>(static_cast<std::shared_ptr<State>*>(userdata));
}

constexpr WGPUFeatureName kOptionalFeatures[] = {
    WGPUFeatureName_Float32Filterable,
    WGPUFeatureName_Float32Blendable,
    WGPUFeatureName_TimestampQuery,
    WGPUFeatureName_Depth32FloatStencil8,
    WGPUFeatureName_IndirectFirstInstance,
    WGPUFeatureName_BGRA8UnormStorage,
    WGPUFeatureName_RG11B10UfloatRenderable,
    WGPUFeatureName_TextureCompressionBC,
    WGPUFeatureName_TextureCompressionETC2,
    WGPUFeatureName_TextureCompressionASTC,
    compat::kUnorm16TextureFormatsFeature,
};

// Packs rows `paddedBytesPerRow` apart into tight rows. Bytes of the last word past `totalBytes`
// keep their value.
constexpr const char* kRowPackShader = R"(
struct Params {
  tightBytesPerRow : u32,
  paddedBytesPerRow : u32,
  tightBytesPerImage : u32,
  paddedBytesPerImage : u32,
  totalBytes : u32,
}
@group(0) @binding(0) var<uniform> params : Params;
@group(0) @binding(1) var<storage, read> src : array<u32>;
@group(0) @binding(2) var<storage, read_write> dst : array<u32>;

fn srcByte(i : u32) -> u32 {
  let image = i / params.tightBytesPerImage;
  let inImage = i - image * params.tightBytesPerImage;
  let row = inImage / params.tightBytesPerRow;
  let s = image * params.paddedBytesPerImage + row * params.paddedBytesPerRow + inImage -
          row * params.tightBytesPerRow;
  return (src[s / 4u] >> ((s % 4u) * 8u)) & 0xffu;
}

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id : vec3u, @builtin(num_workgroups) groups : vec3u) {
  let word = id.y * groups.x * 64u + id.x;
  let first = word * 4u;
  if (first >= params.totalBytes) {
    return;
  }
  var value = dst[word];
  for (var k = 0u; k < 4u; k++) {
    if (first + k < params.totalBytes) {
      let shift = k * 8u;
      value = (value & ~(0xffu << shift)) | (srcByte(first + k) << shift);
    }
  }
  dst[word] = value;
}
)";

void onQueueWorkDone(WGPUQueueWorkDoneStatus status,
                     void* IGL_NULLABLE userdata1,
                     void* IGL_NULLABLE /*userdata2*/) {
  if (const auto s = adoptFromCallback<WGPUQueueWorkDoneStatus>(userdata1)) {
    **s = status;
  }
}

struct RequestAdapterState {
  WGPURequestAdapterStatus status = WGPURequestAdapterStatus_Error;
  Handle<WGPUAdapter> adapter;
  std::string message;
};

struct RequestDeviceState {
  WGPURequestDeviceStatus status = WGPURequestDeviceStatus_Error;
  Handle<WGPUDevice> device;
  std::string message;
};

struct PopErrorScopeState {
  bool completed = false;
  WGPUErrorType type = WGPUErrorType_NoError;
  std::string message;
};

constexpr size_t kMaxLatchedErrors = 64;

} // namespace

#if IGL_PLATFORM_EMSCRIPTEN
// Called by emscripten/library_iglwebgpu.js for imported devices; `state` is the
// std::shared_ptr<CallbackState>* passed to igl_webgpu_import_js_device(), freed on loss unless
// ~WebGPUContext() released the import first.
void onJsDeviceLost(void* IGL_NONNULL state, int destroyed, const char* IGL_NULLABLE message) {
  const std::unique_ptr<std::shared_ptr<WebGPUContext::CallbackState>> s(
      static_cast<std::shared_ptr<WebGPUContext::CallbackState>*>(state));
  (*s)->deviceLost = true;
  if (destroyed == 0) {
    IGL_LOG_ERROR("WebGPU device lost: %s\n", message != nullptr ? message : "");
  }
}

// emscripten/library_iglwebgpu.js
extern "C" int igl_webgpu_release_js_device(void* state);

void onJsUncapturedError(void* IGL_NONNULL state, const char* IGL_NULLABLE message) {
  auto& s = *static_cast<std::shared_ptr<WebGPUContext::CallbackState>*>(state);
  s->uncapturedErrorCount++;
  IGL_LOG_ERROR("Uncaptured WebGPU error: %s\n", message != nullptr ? message : "");
  IGL_SOFT_ERROR("Uncaptured WebGPU error: %s", message != nullptr ? message : "");
}
#endif

void WebGPUContext::CallbackState::latch(Result error) {
  const std::lock_guard<std::mutex> lock(latchedErrorsMutex);
  if (latchedErrors.size() < kMaxLatchedErrors) {
    latchedErrors.push_back(std::move(error));
  } else {
    IGL_LOG_ERROR_ONCE("WebGPU: %zu latched errors are pending; dropping further errors\n",
                       kMaxLatchedErrors);
  }
}

WebGPUContext::WebGPUContext(WebGPUContextDesc desc) : desc_(std::move(desc)) {}

WebGPUContext::~WebGPUContext() {
#if IGL_PLATFORM_EMSCRIPTEN
  // The page keeps an imported device (emdawnwebgpu does not destroy it), so its listener must
  // not keep reporting into this context.
  // After a loss, onJsDeviceLost() has freed the state, and its address may already belong to
  // another import, so only an import that is still live is released.
  if (jsImportState_ != nullptr && !callbackState_->deviceLost &&
      igl_webgpu_release_js_device(jsImportState_) != 0) {
    delete static_cast<std::shared_ptr<CallbackState>*>(jsImportState_);
  }
#endif
  rowPackPipeline_ = nullptr;
  mipmapGenerator_.reset();
  depthUploader_.reset();
  uniformArenaPool_.reset();
  bindGroupCache_.reset();
  dummyResources_.reset();
  queue_ = nullptr;
  device_ = nullptr;
  adapter_ = nullptr;
  instance_ = nullptr;
}

std::span<const WGPUFeatureName> WebGPUContext::getOptionalFeatures() noexcept {
  return kOptionalFeatures;
}

std::optional<WGPUBackendType> WebGPUContext::parseBackendType(std::string_view name) {
  if (name == "metal") {
    return WGPUBackendType_Metal;
  }
  if (name == "vulkan") {
    return WGPUBackendType_Vulkan;
  }
  if (name == "null") {
    return WGPUBackendType_Null;
  }
  if (name == "d3d12") {
    return WGPUBackendType_D3D12;
  }
  if (name == "d3d11") {
    return WGPUBackendType_D3D11;
  }
  if (name == "opengl") {
    return WGPUBackendType_OpenGL;
  }
  if (name == "opengles") {
    return WGPUBackendType_OpenGLES;
  }
  return std::nullopt;
}

WGPUBackendType WebGPUContext::resolveBackendType(WGPUBackendType requested) {
  if (requested != WGPUBackendType_Undefined) {
    return requested;
  }
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  if (const char* env = std::getenv("IGL_WEBGPU_BACKEND"); env != nullptr && *env != '\0') {
    if (const auto type = parseBackendType(env)) {
      return *type;
    }
    IGL_LOG_ERROR("IGL_WEBGPU_BACKEND=%s is not a known backend; using the default\n", env);
  }
#if defined(IGL_WEBGPU_DEFAULT_BACKEND)
  if (const auto type = parseBackendType(IGL_WEBGPU_DEFAULT_BACKEND)) {
    return *type;
  }
#endif
  return getPlatformDefaultBackendType();
}

std::unique_ptr<WebGPUContext> WebGPUContext::create(const WebGPUContextDesc& desc,
                                                     Result* IGL_NULLABLE outResult) {
  auto ctx = std::unique_ptr<WebGPUContext>(new WebGPUContext(desc));
  Result result = ctx->initInstanceAndAdapter();
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  ctx->resolveErrorMode();
  Result::setOk(outResult);
  return ctx;
}

std::unique_ptr<WebGPUContext> WebGPUContext::createWithDevice(const WebGPUContextDesc& desc,
                                                               WGPUInstance IGL_NULLABLE instance,
                                                               WGPUDevice IGL_NULLABLE device,
                                                               Result* IGL_NULLABLE outResult) {
  if (instance == nullptr || device == nullptr) {
    Result::setResult(
        outResult, Result::Code::ArgumentNull, "A WebGPU instance and device are required");
    return nullptr;
  }
  auto ctx = std::unique_ptr<WebGPUContext>(new WebGPUContext(desc));
  ctx->instance_ = Handle<WGPUInstance>::retain(instance);
  ctx->device_ = Handle<WGPUDevice>::retain(device);
  ctx->queue_.reset(wgpuDeviceGetQueue(device));
  WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
  if (wgpuDeviceGetAdapterInfo(device, &info) == WGPUStatus_Success) {
    ctx->setAdapterInfo(info);
  }
  ctx->resolveErrorMode();
  Result::setOk(outResult);
  return ctx;
}

#if IGL_PLATFORM_EMSCRIPTEN
extern "C" {
// emscripten/library_iglwebgpu.js
WGPUDevice igl_webgpu_import_js_device(WGPUInstance instance, void* state);

EMSCRIPTEN_KEEPALIVE void igl_webgpu_on_device_lost(void* state,
                                                    int destroyed,
                                                    const char* message) {
  onJsDeviceLost(state, destroyed, message);
}

EMSCRIPTEN_KEEPALIVE void igl_webgpu_on_uncaptured_error(void* state, const char* message) {
  onJsUncapturedError(state, message);
}
}

std::unique_ptr<WebGPUContext> WebGPUContext::createWithJsDevice(const WebGPUContextDesc& desc,
                                                                 Result* IGL_NULLABLE outResult) {
  auto ctx = std::unique_ptr<WebGPUContext>(new WebGPUContext(desc));
  ctx->instance_.reset(
      compat::createInstance(/*requestTimedWaitAny=*/true, &ctx->hasTimedWaitAny_));
  if (!ctx->instance_) {
    Result::setResult(outResult, Result::Code::RuntimeError, "wgpuCreateInstance() failed");
    return nullptr;
  }
  // The callbacks own a reference to the state until the device is lost or the import is released
  // by ~WebGPUContext().
  auto* state = new std::shared_ptr<CallbackState>(ctx->callbackState_);
  ctx->device_.reset(igl_webgpu_import_js_device(ctx->instance_.get(), state));
  if (!ctx->device_) {
    delete state;
    Result::setResult(outResult,
                      Result::Code::ArgumentNull,
                      "Module.iglWebGPUDevice and Module.preinitializedWebGPUDevice are unset");
    return nullptr;
  }
  ctx->jsImportState_ = state;
  ctx->queue_.reset(wgpuDeviceGetQueue(ctx->device_.get()));
  WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
  if (wgpuDeviceGetAdapterInfo(ctx->device_.get(), &info) == WGPUStatus_Success) {
    ctx->setAdapterInfo(info);
  }
  ctx->resolveErrorMode();
  Result::setOk(outResult);
  return ctx;
}
#endif

void WebGPUContext::setAdapterInfo(WGPUAdapterInfo& info) {
  adapterBackendType_ = info.backendType;
  adapterType_ = info.adapterType;
  vendorId_ = info.vendorID;
  adapterName_ = toStdString(info.device);
  adapterVendor_ = toStdString(info.vendor);
  wgpuAdapterInfoFreeMembers(info);
}

void WebGPUContext::resolveErrorMode() {
  errorMode_ = desc_.errorMode;
  if (errorMode_ == ErrorMode::Default) {
#if IGL_PLATFORM_EMSCRIPTEN
    errorMode_ = hasTimedWaitAny_ ? ErrorMode::SyncPipelines : ErrorMode::Latched;
#else
    errorMode_ = ErrorMode::Sync;
#endif
  }
}

Result WebGPUContext::initInstanceAndAdapter() {
  installProcs();

  instance_.reset(compat::createInstance(/*requestTimedWaitAny=*/true, &hasTimedWaitAny_));
  if (!instance_) {
    return Result(Result::Code::RuntimeError, "wgpuCreateInstance() failed");
  }

  const WGPUBackendType backendType = resolveBackendType(desc_.backendType);
  WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
  options.featureLevel = WGPUFeatureLevel_Core;
  options.powerPreference = WGPUPowerPreference_HighPerformance;
  // forceFallbackAdapter restricts Dawn's Vulkan backend to its bundled SwiftShader.
  options.forceFallbackAdapter = 0;
  options.backendType = backendType;

  const auto state = std::make_shared<RequestAdapterState>();
  const WGPURequestAdapterCallbackInfo callbackInfo = {
      .nextInChain = nullptr,
      .mode = WGPUCallbackMode_WaitAnyOnly,
      .callback =
          [](WGPURequestAdapterStatus status,
             WGPUAdapter adapter,
             WGPUStringView message,
             void* IGL_NULLABLE userdata1,
             void* IGL_NULLABLE /*userdata2*/) {
            const auto s = adoptFromCallback<RequestAdapterState>(userdata1);
            if (!s) {
              return;
            }
            (*s)->status = status;
            (*s)->adapter.reset(adapter);
            (*s)->message = toStdString(message);
          },
      .userdata1 = retainForCallback(state),
      .userdata2 = nullptr,
  };
  if (!waitFuture(wgpuInstanceRequestAdapter(instance_.get(), &options, callbackInfo))) {
    return Result(Result::Code::RuntimeError, "Timed out waiting for a WebGPU adapter");
  }
  if (state->status != WGPURequestAdapterStatus_Success || !state->adapter) {
    return Result(Result::Code::Unsupported, "No WebGPU adapter: " + state->message);
  }
  adapter_ = std::move(state->adapter);

  WGPUAdapterInfo info = WGPU_ADAPTER_INFO_INIT;
  if (wgpuAdapterGetInfo(adapter_.get(), &info) == WGPUStatus_Success) {
    setAdapterInfo(info);
  }
  if (backendType != WGPUBackendType_Undefined && adapterBackendType_ != backendType) {
    return Result(Result::Code::Unsupported, "The WebGPU adapter uses a different backend");
  }
  return Result();
}

Result WebGPUContext::initDevice() {
  if (!adapter_) {
    return Result(Result::Code::InvalidOperation, "No WebGPU adapter");
  }
  if (device_) {
    return Result(Result::Code::InvalidOperation, "The WebGPU device already exists");
  }

  WGPULimits limits = WGPU_LIMITS_INIT;
  if (desc_.requestAdapterLimits &&
      wgpuAdapterGetLimits(adapter_.get(), &limits) != WGPUStatus_Success) {
    return Result(Result::Code::RuntimeError, "wgpuAdapterGetLimits() failed");
  }

  std::vector<WGPUFeatureName> features = desc_.requiredFeatures;
  if (desc_.requestOptionalFeatures) {
    for (const WGPUFeatureName feature : kOptionalFeatures) {
      if (wgpuAdapterHasFeature(adapter_.get(), feature) != 0 &&
          std::find(features.begin(), features.end(), feature) == features.end()) {
        features.push_back(feature);
      }
    }
  }

  WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
  deviceDesc.label = toWGPUStringView(desc_.debugName);
  deviceDesc.requiredFeatureCount = features.size();
  deviceDesc.requiredFeatures = features.data();
  deviceDesc.requiredLimits = desc_.requestAdapterLimits ? &limits : nullptr;
#if !IGL_WEBGPU_HEADER_V2
  // Dawn quantizes timestamps like browsers do unless this toggle is disabled.
  static constexpr const char* kDisabledToggles[] = {"timestamp_quantization"};
  WGPUDawnTogglesDescriptor toggles = WGPU_DAWN_TOGGLES_DESCRIPTOR_INIT;
  toggles.disabledToggleCount = std::size(kDisabledToggles);
  toggles.disabledToggles = kDisabledToggles;
  if (desc_.highResolutionTimestamps) {
    deviceDesc.nextInChain = &toggles.chain;
  }
#endif
  // The device can outlive the context (resources hold device references), so the callbacks share a
  // reference to the state. The lost callback, which Dawn invokes exactly once, frees it; Dawn
  // clears the uncaptured error callback before invoking the lost callback.
  auto* callbackState = new std::shared_ptr<CallbackState>(callbackState_);
  deviceDesc.deviceLostCallbackInfo = {
      .nextInChain = nullptr,
      .mode = WGPUCallbackMode_AllowSpontaneous,
      .callback =
          [](const WGPUDevice* /*device*/,
             WGPUDeviceLostReason reason,
             WGPUStringView message,
             void* IGL_NULLABLE userdata1,
             void* IGL_NULLABLE /*userdata2*/) {
            const std::unique_ptr<std::shared_ptr<CallbackState>> state(
                static_cast<std::shared_ptr<CallbackState>*>(userdata1));
            (*state)->deviceLost = true;
            if (reason != WGPUDeviceLostReason_Destroyed &&
                reason != WGPUDeviceLostReason_CallbackCancelled) {
              IGL_LOG_ERROR("WebGPU device lost: %s\n", toStdString(message).c_str());
            }
          },
      .userdata1 = callbackState,
      .userdata2 = nullptr,
  };
  deviceDesc.uncapturedErrorCallbackInfo = {
      .nextInChain = nullptr,
      .callback =
          [](const WGPUDevice* /*device*/,
             WGPUErrorType type,
             WGPUStringView message,
             void* IGL_NULLABLE userdata1,
             void* IGL_NULLABLE /*userdata2*/) {
            if (auto* state = static_cast<std::shared_ptr<CallbackState>*>(userdata1)) {
              (*state)->uncapturedErrorCount++;
            }
            const Result result = getResultFromWGPUError(type, message);
            IGL_LOG_ERROR("Uncaptured WebGPU error: %s\n", result.message.c_str());
            IGL_SOFT_ERROR("Uncaptured WebGPU error: %s", result.message.c_str());
          },
      .userdata1 = callbackState,
      .userdata2 = nullptr,
  };

  const auto state = std::make_shared<RequestDeviceState>();
  const WGPURequestDeviceCallbackInfo callbackInfo = {
      .nextInChain = nullptr,
      .mode = WGPUCallbackMode_WaitAnyOnly,
      .callback =
          [](WGPURequestDeviceStatus status,
             WGPUDevice device,
             WGPUStringView message,
             void* IGL_NULLABLE userdata1,
             void* IGL_NULLABLE /*userdata2*/) {
            const auto s = adoptFromCallback<RequestDeviceState>(userdata1);
            if (!s) {
              return;
            }
            (*s)->status = status;
            (*s)->device.reset(device);
            (*s)->message = toStdString(message);
          },
      .userdata1 = retainForCallback(state),
      .userdata2 = nullptr,
  };
  if (!waitFuture(wgpuAdapterRequestDevice(adapter_.get(), &deviceDesc, callbackInfo))) {
    return Result(Result::Code::RuntimeError, "Timed out waiting for the WebGPU device");
  }
  if (state->status != WGPURequestDeviceStatus_Success || !state->device) {
    return Result(Result::Code::RuntimeError,
                  "wgpuAdapterRequestDevice() failed: " + state->message);
  }
  device_ = std::move(state->device);
  queue_.reset(wgpuDeviceGetQueue(device_.get()));
  return Result();
}

bool WebGPUContext::canWait() const noexcept {
#if IGL_PLATFORM_EMSCRIPTEN
  return suspensionAllowed_ && hasTimedWaitAny_;
#else
  return suspensionAllowed_;
#endif
}

Result WebGPUContext::checkCanWait() const {
  if (!suspensionAllowed_) {
    IGL_DEBUG_ABORT("WebGPU wait while suspension is disallowed\n");
    return Result(Result::Code::InvalidOperation,
                  "WebGPU cannot wait while suspension is disallowed");
  }
  if (!canWait()) {
    return Result(Result::Code::Unsupported, "WebGPU cannot wait in browser builds without JSPI");
  }
  return Result();
}

bool WebGPUContext::waitFuture(WGPUFuture future, uint64_t timeoutNs) const {
  WGPUFutureWaitInfo waitInfo = {.future = future, .completed = 0};
  if (!suspensionAllowed_) {
    IGL_DEBUG_ABORT("WebGPU wait while suspension is disallowed\n");
    return false;
  }
#if IGL_PLATFORM_EMSCRIPTEN
  if (!hasTimedWaitAny_) {
    // Nothing completes while this call spins; futures resolve from the browser's event loop.
    return wgpuInstanceWaitAny(instance_.get(), 1, &waitInfo, 0) == WGPUWaitStatus_Success &&
           waitInfo.completed != 0;
  }
#endif
  if (hasTimedWaitAny_) {
    return wgpuInstanceWaitAny(instance_.get(), 1, &waitInfo, timeoutNs) ==
               WGPUWaitStatus_Success &&
           waitInfo.completed != 0;
  }
  // Without timed waits only zero-timeout polling is available.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::nanoseconds(timeoutNs);
  while (true) {
    if (wgpuInstanceWaitAny(instance_.get(), 1, &waitInfo, 0) == WGPUWaitStatus_Success &&
        waitInfo.completed != 0) {
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::yield();
  }
}

Result WebGPUContext::waitForSubmittedWork(uint64_t timeoutNs) const {
  Result result = checkCanWait();
  if (!result.isOk()) {
    return result;
  }
  const auto status = std::make_shared<WGPUQueueWorkDoneStatus>(WGPUQueueWorkDoneStatus_Error);
  // onQueueWorkDone() frees the reference retained for it.
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
  const WGPUFuture future =
      wgpuQueueOnSubmittedWorkDone(queue_.get(),
                                   compat::queueWorkDoneCallbackInfo<onQueueWorkDone>(
                                       WGPUCallbackMode_WaitAnyOnly, retainForCallback(status)));
  if (!waitFuture(future, timeoutNs)) {
    return Result(Result::Code::RuntimeError, "Timed out waiting for the WebGPU queue");
  }
  if (*status != WGPUQueueWorkDoneStatus_Success) {
    return Result(Result::Code::DeviceLost, "The WebGPU queue did not complete its work");
  }
  return Result();
}

void WebGPUContext::processEvents() const {
  wgpuInstanceProcessEvents(instance_.get());
}

void WebGPUContext::pushErrorScope(WGPUErrorFilter filter) const {
  wgpuDevicePushErrorScope(device_.get(), filter);
}

bool WebGPUContext::waitsForErrors(ErrorScopeKind kind) const noexcept {
  if (!canWait()) {
    return false;
  }
  switch (errorMode_) {
  case ErrorMode::Default:
  case ErrorMode::Sync:
    return true;
  case ErrorMode::SyncPipelines:
    return kind == ErrorScopeKind::Pipeline;
  case ErrorMode::Latched:
    return false;
  }
  IGL_UNREACHABLE_RETURN(true)
}

Result WebGPUContext::popErrorScope(ErrorScopeKind kind) const {
  return popErrorScopes(1, kind);
}

Result WebGPUContext::popErrorScopes(uint32_t count, ErrorScopeKind kind) const {
  if (!waitsForErrors(kind)) {
    for (uint32_t i = 0; i < count; ++i) {
      const WGPUPopErrorScopeCallbackInfo latchInfo = {
          .nextInChain = nullptr,
          .mode = WGPUCallbackMode_AllowSpontaneous,
          .callback =
              [](WGPUPopErrorScopeStatus status,
                 WGPUErrorType type,
                 WGPUStringView message,
                 void* IGL_NULLABLE userdata1,
                 void* IGL_NULLABLE /*userdata2*/) {
                const std::unique_ptr<std::shared_ptr<CallbackState>> s(
                    static_cast<std::shared_ptr<CallbackState>*>(userdata1));
                if (s != nullptr && status == WGPUPopErrorScopeStatus_Success &&
                    type != WGPUErrorType_NoError) {
                  (*s)->latch(getResultFromWGPUError(type, message));
                }
              },
          .userdata1 = new std::shared_ptr<CallbackState>(callbackState_),
          .userdata2 = nullptr,
      };
      wgpuDevicePopErrorScope(device_.get(), latchInfo);
    }
    return Result();
  }
  // Error scopes are one stack per device, and under JSPI a wait suspends to the event loop. Every
  // scope is popped before the first wait, so errors of other work on the device meanwhile cannot
  // land in a scope that is still pushed.
  std::vector<std::pair<WGPUFuture, std::shared_ptr<PopErrorScopeState>>> pops;
  pops.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    auto state = std::make_shared<PopErrorScopeState>();
    const WGPUPopErrorScopeCallbackInfo callbackInfo = {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback =
            [](WGPUPopErrorScopeStatus status,
               WGPUErrorType type,
               WGPUStringView message,
               void* IGL_NULLABLE userdata1,
               void* IGL_NULLABLE /*userdata2*/) {
              const auto s = adoptFromCallback<PopErrorScopeState>(userdata1);
              if (!s) {
                return;
              }
              (*s)->completed = status == WGPUPopErrorScopeStatus_Success;
              (*s)->type = type;
              (*s)->message = toStdString(message);
            },
        .userdata1 = retainForCallback(state),
        .userdata2 = nullptr,
    };
    const WGPUFuture future = wgpuDevicePopErrorScope(device_.get(), callbackInfo);
    pops.emplace_back(future, std::move(state));
  }
  Result result;
  for (const auto& [future, state] : pops) {
    Result popped = waitFuture(future) && state->completed
                        ? getResultFromWGPUError(state->type, toWGPUStringView(state->message))
                        : Result(Result::Code::RuntimeError, "wgpuDevicePopErrorScope() failed");
    if (result.isOk() && !popped.isOk()) {
      result = std::move(popped);
    }
  }
  return result;
}

WGPUComputePipeline IGL_NULLABLE WebGPUContext::getRowPackPipeline() const {
  if (!rowPackPipeline_) {
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = toWGPUStringView(kRowPackShader);
    WGPUShaderModuleDescriptor moduleDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    moduleDesc.nextInChain = &wgsl.chain;
    moduleDesc.label = toWGPUStringView("igl.webgpu.rowPack");
    const Handle<WGPUShaderModule> module(wgpuDeviceCreateShaderModule(device_.get(), &moduleDesc));
    WGPUComputePipelineDescriptor pipelineDesc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
    pipelineDesc.label = toWGPUStringView("igl.webgpu.rowPack");
    pipelineDesc.compute.module = module.get();
    pipelineDesc.compute.entryPoint = toWGPUStringView("main");
    rowPackPipeline_.reset(wgpuDeviceCreateComputePipeline(device_.get(), &pipelineDesc));
  }
  return rowPackPipeline_.get();
}

BindGroupCache& WebGPUContext::getBindGroupCache() const {
  if (!bindGroupCache_) {
    bindGroupCache_ = std::make_unique<BindGroupCache>(*this);
  }
  return *bindGroupCache_;
}

void WebGPUContext::evictBindGroups(uint64_t resourceId) const {
  if (bindGroupCache_) {
    bindGroupCache_->evict(resourceId);
  }
}

DummyResources& WebGPUContext::getDummyResources() const {
  if (!dummyResources_) {
    dummyResources_ = std::make_unique<DummyResources>(*this);
  }
  return *dummyResources_;
}

DepthUploader& WebGPUContext::getDepthUploader() const {
  if (!depthUploader_) {
    depthUploader_ = std::make_unique<DepthUploader>(*this);
  }
  return *depthUploader_;
}

MipmapGenerator& WebGPUContext::getMipmapGenerator() const {
  if (!mipmapGenerator_) {
    mipmapGenerator_ = std::make_unique<MipmapGenerator>(*this);
  }
  return *mipmapGenerator_;
}

UniformArenaPool& WebGPUContext::getUniformArenaPool() {
  if (!uniformArenaPool_) {
    uniformArenaPool_ = std::make_unique<UniformArenaPool>(*this);
  }
  return *uniformArenaPool_;
}

std::vector<Result> WebGPUContext::takeErrors() {
  const std::lock_guard<std::mutex> lock(callbackState_->latchedErrorsMutex);
  callbackState_->loggedLatchedErrorCount = 0;
  return std::exchange(callbackState_->latchedErrors, {});
}

void WebGPUContext::logLatchedErrors() {
  CallbackState& state = *callbackState_;
  const std::lock_guard<std::mutex> lock(state.latchedErrorsMutex);
  for (; state.loggedLatchedErrorCount < state.latchedErrors.size();
       ++state.loggedLatchedErrorCount) {
    IGL_LOG_ERROR("WebGPU error: %s\n",
                  state.latchedErrors[state.loggedLatchedErrorCount].message.c_str());
  }
}

bool WebGPUContext::isDeviceLost() const noexcept {
  return callbackState_->deviceLost;
}

uint32_t WebGPUContext::getUncapturedErrorCount() const noexcept {
  return callbackState_->uncapturedErrorCount;
}

} // namespace igl::webgpu
