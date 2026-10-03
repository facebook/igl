/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <igl/Common.h>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/ResourceTracker.h>

namespace igl::webgpu {

class BindGroupCache;
class DepthUploader;
class DummyResources;
class MipmapGenerator;
class UniformArenaPool;

/// @brief When WebGPU validation errors reach IGL. Waiting for an error scope blocks natively and
/// suspends the caller in the browser (JSPI), which is only possible where suspension is allowed.
enum class ErrorMode : uint8_t {
  /// Sync natively; SyncPipelines in browser builds with JSPI; Latched in browser builds without.
  Default,
  /// Every create call waits for its error scope and fails with the error.
  Sync,
  /// Only shader module and pipeline creation waits; other create calls return Ok provisionally and
  /// latch their errors (see WebGPUContext::takeErrors()).
  SyncPipelines,
  /// No create call waits; errors are latched and logged at the next submit.
  Latched,
};

/// @brief What an error scope guards; decides whether popErrorScope() waits (see ErrorMode).
enum class ErrorScopeKind : uint8_t {
  Resource,
  /// Shader modules and pipelines.
  Pipeline,
};

/// @brief Fixed at device creation.
struct WebGPUContextDesc {
  /// Adapter backend. Undefined resolves to the IGL_WEBGPU_BACKEND environment variable, then to
  /// the compile-time IGL_WEBGPU_DEFAULT_BACKEND, then to the platform default (Metal on Apple,
  /// Vulkan on Linux and Android, D3D12 on Windows). Ignored in the browser.
  WGPUBackendType backendType = WGPUBackendType_Undefined;
  /// Device creation fails when the adapter lacks any of these.
  std::vector<WGPUFeatureName> requiredFeatures;
  /// Also enable each of getOptionalFeatures() that the adapter offers.
  bool requestOptionalFeatures = true;
  /// Request the adapter's limits instead of the spec defaults. Off by default so every lane runs
  /// against portable limits.
  bool requestAdapterLimits = false;
  /// Native builds: full-resolution timestamp queries (Dawn quantizes them otherwise, like
  /// browsers do). Ignored in the browser.
  bool highResolutionTimestamps = true;
  ErrorMode errorMode = ErrorMode::Default;
  std::string debugName = "igl.webgpu";
};

/// @brief Owns the WebGPU instance, adapter, device and queue, and reports device errors.
class WebGPUContext final {
 public:
  static constexpr uint64_t kDefaultTimeoutNs = 10'000'000'000ull;

  /// Creates the instance and requests an adapter. Call initDevice() before any other use.
  [[nodiscard]] static std::unique_ptr<WebGPUContext> create(const WebGPUContextDesc& desc,
                                                             Result* IGL_NULLABLE outResult);
  /// Wraps a device created outside IGL; the context adds its own references to `instance` and
  /// `device`. The instance must be the one that tracks the device's futures. Its uncaptured errors
  /// and device loss are reported to whoever created it, not to the context.
  [[nodiscard]] static std::unique_ptr<WebGPUContext> createWithDevice(
      const WebGPUContextDesc& desc,
      WGPUInstance IGL_NULLABLE instance,
      WGPUDevice IGL_NULLABLE device,
      Result* IGL_NULLABLE outResult);
#if IGL_PLATFORM_EMSCRIPTEN
  /// Imports the GPUDevice in `Module.iglWebGPUDevice` (else `Module.preinitializedWebGPUDevice`)
  /// through emscripten/library_iglwebgpu.js, which reports its uncaptured errors and loss to the
  /// context.
  [[nodiscard]] static std::unique_ptr<WebGPUContext> createWithJsDevice(
      const WebGPUContextDesc& desc,
      Result* IGL_NULLABLE outResult);
#endif
  ~WebGPUContext();

  WebGPUContext(const WebGPUContext&) = delete;
  WebGPUContext& operator=(const WebGPUContext&) = delete;
  WebGPUContext(WebGPUContext&&) = delete;
  WebGPUContext& operator=(WebGPUContext&&) = delete;

  /// Requests the device and its queue from the adapter.
  [[nodiscard]] Result initDevice();

  /// Blocks (suspends, in the browser) until `future` completes or `timeoutNs` elapses; returns
  /// whether it completed. Only polls once when waiting is impossible (see canWait()), and fails
  /// with a debug abort while suspension is disallowed. Callbacks of futures waited on here must
  /// use WGPUCallbackMode_WaitAnyOnly or WGPUCallbackMode_AllowProcessEvents.
  [[nodiscard]] bool waitFuture(WGPUFuture future, uint64_t timeoutNs = kDefaultTimeoutNs) const;
  void processEvents() const;

  /// Blocks until all work submitted to the queue so far has completed.
  [[nodiscard]] Result waitForSubmittedWork(uint64_t timeoutNs = kDefaultTimeoutNs) const;

  /// Whether waits can block: always natively, only with JSPI in the browser, and never while
  /// suspension is disallowed.
  [[nodiscard]] bool canWait() const noexcept;
  /// Ok if canWait(); otherwise why not, with a debug abort while suspension is disallowed.
  [[nodiscard]] Result checkCanWait() const;
  /// JSPI rule: nothing may suspend while another export is suspended. Exports that are not JSPI
  /// exports, or that run while one is suspended, disallow suspension so that every wait fails
  /// (with a debug abort) instead of trapping with a SuspendError. Error scopes latch instead.
  void setSuspensionAllowed(bool allowed) noexcept {
    suspensionAllowed_ = allowed;
  }
  [[nodiscard]] bool isSuspensionAllowed() const noexcept {
    return suspensionAllowed_;
  }
  [[nodiscard]] ErrorMode getErrorMode() const noexcept {
    return errorMode_;
  }

  void pushErrorScope(WGPUErrorFilter filter) const;
  /// Pops the innermost error scope. Returns the error it captured when the error mode waits for
  /// `kind` and waiting is possible; otherwise returns Ok and latches the error when it arrives.
  [[nodiscard]] Result popErrorScope(ErrorScopeKind kind = ErrorScopeKind::Resource) const;
  /// Pops the `count` innermost error scopes before waiting for any of them, so that no scope is
  /// pushed during the wait; returns the first error, innermost scope first. Use it for nested
  /// scopes instead of consecutive popErrorScope() calls.
  [[nodiscard]] Result popErrorScopes(uint32_t count,
                                      ErrorScopeKind kind = ErrorScopeKind::Resource) const;
  /// Whether popErrorScope(kind) waits for the error.
  [[nodiscard]] bool waitsForErrors(ErrorScopeKind kind) const noexcept;
  /// Returns and clears the errors latched so far (at most 64 are kept).
  [[nodiscard]] std::vector<Result> takeErrors();
  /// Logs the errors latched since the last call; submits call it.
  void logLatchedErrors();

  [[nodiscard]] WGPUInstance IGL_NULLABLE getInstance() const noexcept {
    return instance_.get();
  }
  [[nodiscard]] WGPUAdapter IGL_NULLABLE getAdapter() const noexcept {
    return adapter_.get();
  }
  [[nodiscard]] WGPUDevice IGL_NULLABLE getDevice() const noexcept {
    return device_.get();
  }
  [[nodiscard]] WGPUQueue IGL_NULLABLE getQueue() const noexcept {
    return queue_.get();
  }
  [[nodiscard]] const WebGPUContextDesc& getDesc() const noexcept {
    return desc_;
  }
  [[nodiscard]] WGPUBackendType getBackendType() const noexcept {
    return adapterBackendType_;
  }
  [[nodiscard]] WGPUAdapterType getAdapterType() const noexcept {
    return adapterType_;
  }
  [[nodiscard]] uint32_t getVendorId() const noexcept {
    return vendorId_;
  }
  [[nodiscard]] const std::string& getAdapterName() const noexcept {
    return adapterName_;
  }
  [[nodiscard]] const std::string& getAdapterVendor() const noexcept {
    return adapterVendor_;
  }
  [[nodiscard]] bool hasTimedWaitAny() const noexcept {
    return hasTimedWaitAny_;
  }
  [[nodiscard]] ResourceTracker& getResourceTracker() noexcept {
    return resourceTracker_;
  }
  /// Compute pipeline that packs texture rows copied with a 256-byte row pitch into tight rows that
  /// need not be multiples of 4 bytes (see CommandBuffer::copyTextureToBuffer()). Created on first
  /// use.
  [[nodiscard]] WGPUComputePipeline IGL_NULLABLE getRowPackPipeline() const;
  /// Bind groups shared by all encoders; resources evict theirs when destroyed.
  [[nodiscard]] BindGroupCache& getBindGroupCache() const;
  /// Drops the cached bind groups that use `resourceId`. Does not create the cache, so resources
  /// destroyed while the context is torn down cannot recreate it.
  void evictBindGroups(uint64_t resourceId) const;
  /// Placeholders for declared bindings nothing is bound to.
  [[nodiscard]] DummyResources& getDummyResources() const;
  /// Renders depth uploads that wgpuQueueWriteTexture() cannot do.
  [[nodiscard]] DepthUploader& getDepthUploader() const;
  /// Renders mip chains for Texture::generateMipmap().
  [[nodiscard]] MipmapGenerator& getMipmapGenerator() const;
  /// Chunks for the bindBytes() arenas of command buffers.
  [[nodiscard]] UniformArenaPool& getUniformArenaPool();

  [[nodiscard]] bool isDeviceLost() const noexcept;
  /// Number of errors raised outside any error scope since device creation.
  [[nodiscard]] uint32_t getUncapturedErrorCount() const noexcept;

  /// Features enabled when the adapter offers them and WebGPUContextDesc::requestOptionalFeatures
  /// is set.
  [[nodiscard]] static std::span<const WGPUFeatureName> getOptionalFeatures() noexcept;

  /// Parses a case-sensitive backend name ("metal", "vulkan", "null", "d3d12", "d3d11", "opengl",
  /// "opengles").
  [[nodiscard]] static std::optional<WGPUBackendType> parseBackendType(std::string_view name);
  /// Resolves WGPUBackendType_Undefined as described in WebGPUContextDesc::backendType.
  [[nodiscard]] static WGPUBackendType resolveBackendType(WGPUBackendType requested);

 private:
  // Outlives the context if the device does: Dawn may report loss when the last device reference
  // held by a leaked resource is released.
  struct CallbackState {
    std::atomic<bool> deviceLost = false;
    std::atomic<uint32_t> uncapturedErrorCount = 0;
    std::mutex latchedErrorsMutex;
    std::vector<Result> latchedErrors;
    size_t loggedLatchedErrorCount = 0;

    void latch(Result error);
  };

  explicit WebGPUContext(WebGPUContextDesc desc);
  [[nodiscard]] Result initInstanceAndAdapter();
  void resolveErrorMode();
  /// Copies `info` and frees its members.
  void setAdapterInfo(WGPUAdapterInfo& info);

#if IGL_PLATFORM_EMSCRIPTEN
  friend void onJsDeviceLost(void* IGL_NONNULL state,
                             int destroyed,
                             const char* IGL_NULLABLE message);
  friend void onJsUncapturedError(void* IGL_NONNULL state, const char* IGL_NULLABLE message);
#endif

  WebGPUContextDesc desc_;
  ResourceTracker resourceTracker_;
  std::shared_ptr<CallbackState> callbackState_ = std::make_shared<CallbackState>();
#if IGL_PLATFORM_EMSCRIPTEN
  // The std::shared_ptr<CallbackState>* registered with an imported JS device; see
  // igl_webgpu_release_js_device().
  void* IGL_NULLABLE jsImportState_ = nullptr;
#endif

  Handle<WGPUInstance> instance_;
  Handle<WGPUAdapter> adapter_;
  Handle<WGPUDevice> device_;
  Handle<WGPUQueue> queue_;
  mutable Handle<WGPUComputePipeline> rowPackPipeline_;
  mutable std::unique_ptr<BindGroupCache> bindGroupCache_;
  mutable std::unique_ptr<DummyResources> dummyResources_;
  std::unique_ptr<UniformArenaPool> uniformArenaPool_;
  mutable std::unique_ptr<DepthUploader> depthUploader_;
  mutable std::unique_ptr<MipmapGenerator> mipmapGenerator_;

  bool hasTimedWaitAny_ = false;
  bool suspensionAllowed_ = true;
  ErrorMode errorMode_ = ErrorMode::Sync;
  WGPUBackendType adapterBackendType_ = WGPUBackendType_Undefined;
  WGPUAdapterType adapterType_ = WGPUAdapterType_Unknown;
  uint32_t vendorId_ = 0;
  std::string adapterName_;
  std::string adapterVendor_;
};

} // namespace igl::webgpu
