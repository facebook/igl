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
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

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

} // namespace

WebGPUContext::WebGPUContext(WebGPUContextDesc desc) : desc_(std::move(desc)) {}

WebGPUContext::~WebGPUContext() {
  rowPackPipeline_ = nullptr;
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
  Result::setOk(outResult);
  return ctx;
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
    adapterBackendType_ = info.backendType;
    adapterType_ = info.adapterType;
    vendorId_ = info.vendorID;
    adapterName_ = toStdString(info.device);
    adapterVendor_ = toStdString(info.vendor);
    wgpuAdapterInfoFreeMembers(info);
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

bool WebGPUContext::waitFuture(WGPUFuture future, uint64_t timeoutNs) const {
  WGPUFutureWaitInfo waitInfo = {.future = future, .completed = 0};
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

Result WebGPUContext::popErrorScope() const {
  const auto state = std::make_shared<PopErrorScopeState>();
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
  if (!waitFuture(wgpuDevicePopErrorScope(device_.get(), callbackInfo)) || !state->completed) {
    return Result(Result::Code::RuntimeError, "wgpuDevicePopErrorScope() failed");
  }
  return getResultFromWGPUError(state->type, toWGPUStringView(state->message));
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

bool WebGPUContext::isDeviceLost() const noexcept {
  return callbackState_->deviceLost;
}

uint32_t WebGPUContext::getUncapturedErrorCount() const noexcept {
  return callbackState_->uncapturedErrorCount;
}

} // namespace igl::webgpu
