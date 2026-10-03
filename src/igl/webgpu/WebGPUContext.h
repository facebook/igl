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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <igl/Common.h>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/ResourceTracker.h>

namespace igl::webgpu {

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
  std::string debugName = "igl.webgpu";
};

/// @brief Owns the WebGPU instance, adapter, device and queue, and reports device errors.
class WebGPUContext final {
 public:
  static constexpr uint64_t kDefaultTimeoutNs = 10'000'000'000ull;

  /// Creates the instance and requests an adapter. Call initDevice() before any other use.
  [[nodiscard]] static std::unique_ptr<WebGPUContext> create(const WebGPUContextDesc& desc,
                                                             Result* IGL_NULLABLE outResult);
  ~WebGPUContext();

  WebGPUContext(const WebGPUContext&) = delete;
  WebGPUContext& operator=(const WebGPUContext&) = delete;
  WebGPUContext(WebGPUContext&&) = delete;
  WebGPUContext& operator=(WebGPUContext&&) = delete;

  /// Requests the device and its queue from the adapter.
  [[nodiscard]] Result initDevice();

  /// Blocks until `future` completes or `timeoutNs` elapses; returns whether it completed.
  /// Callbacks of futures waited on here must use WGPUCallbackMode_WaitAnyOnly.
  [[nodiscard]] bool waitFuture(WGPUFuture future, uint64_t timeoutNs = kDefaultTimeoutNs) const;
  void processEvents() const;

  /// Blocks until all work submitted to the queue so far has completed.
  [[nodiscard]] Result waitForSubmittedWork(uint64_t timeoutNs = kDefaultTimeoutNs) const;

  void pushErrorScope(WGPUErrorFilter filter) const;
  /// Pops the innermost error scope and returns the error it captured, if any.
  [[nodiscard]] Result popErrorScope() const;

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
  };

  explicit WebGPUContext(WebGPUContextDesc desc);
  [[nodiscard]] Result initInstanceAndAdapter();

  WebGPUContextDesc desc_;
  ResourceTracker resourceTracker_;
  std::shared_ptr<CallbackState> callbackState_ = std::make_shared<CallbackState>();

  Handle<WGPUInstance> instance_;
  Handle<WGPUAdapter> adapter_;
  Handle<WGPUDevice> device_;
  Handle<WGPUQueue> queue_;
  mutable Handle<WGPUComputePipeline> rowPackPipeline_;

  bool hasTimedWaitAny_ = false;
  WGPUBackendType adapterBackendType_ = WGPUBackendType_Undefined;
  WGPUAdapterType adapterType_ = WGPUAdapterType_Unknown;
  uint32_t vendorId_ = 0;
  std::string adapterName_;
  std::string adapterVendor_;
};

} // namespace igl::webgpu
