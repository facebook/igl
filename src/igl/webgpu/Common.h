/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <webgpu/webgpu.h>
#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/DepthStencilState.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/VertexInputState.h>

namespace igl::webgpu {

// Bind convention v1 (fixed; shaders are written against it):
//   group 0: texture unit i at @binding(2i), its sampler at @binding(2i+1)
//   group 1: the buffer at IGL index i at @binding(i)
//   group 2: storage texture i at @binding(i)
//   group 3: push-constant emulation, @binding(0)
inline constexpr uint32_t kTextureGroup = 0;
inline constexpr uint32_t kBufferGroup = 1;
inline constexpr uint32_t kStorageTextureGroup = 2;
inline constexpr uint32_t kPushConstantGroup = 3;
inline constexpr uint32_t kNumBindGroups = 4;
/// Size of the uniform buffer that emulates push constants
/// (DeviceFeatureLimits::MaxPushConstantBytes).
inline constexpr uint32_t kMaxPushConstantBytes = 128;

/// @brief AddRef/Release entry points for a WebGPU object type, used by Handle<T>
template<typename T>
struct HandleTraits;

#define IGL_WEBGPU_HANDLE_TRAITS(Name)                            \
  template<>                                                      \
  struct HandleTraits<WGPU##Name> {                               \
    static void addRef(WGPU##Name IGL_NONNULL handle) noexcept {  \
      wgpu##Name##AddRef(handle);                                 \
    }                                                             \
    static void release(WGPU##Name IGL_NONNULL handle) noexcept { \
      wgpu##Name##Release(handle);                                \
    }                                                             \
  };

IGL_WEBGPU_HANDLE_TRAITS(Adapter)
IGL_WEBGPU_HANDLE_TRAITS(BindGroup)
IGL_WEBGPU_HANDLE_TRAITS(BindGroupLayout)
IGL_WEBGPU_HANDLE_TRAITS(Buffer)
IGL_WEBGPU_HANDLE_TRAITS(CommandBuffer)
IGL_WEBGPU_HANDLE_TRAITS(CommandEncoder)
IGL_WEBGPU_HANDLE_TRAITS(ComputePassEncoder)
IGL_WEBGPU_HANDLE_TRAITS(ComputePipeline)
IGL_WEBGPU_HANDLE_TRAITS(Device)
IGL_WEBGPU_HANDLE_TRAITS(Instance)
IGL_WEBGPU_HANDLE_TRAITS(PipelineLayout)
IGL_WEBGPU_HANDLE_TRAITS(QuerySet)
IGL_WEBGPU_HANDLE_TRAITS(Queue)
IGL_WEBGPU_HANDLE_TRAITS(RenderPassEncoder)
IGL_WEBGPU_HANDLE_TRAITS(RenderPipeline)
IGL_WEBGPU_HANDLE_TRAITS(Sampler)
IGL_WEBGPU_HANDLE_TRAITS(ShaderModule)
IGL_WEBGPU_HANDLE_TRAITS(Surface)
IGL_WEBGPU_HANDLE_TRAITS(Texture)
IGL_WEBGPU_HANDLE_TRAITS(TextureView)

#undef IGL_WEBGPU_HANDLE_TRAITS

/// @brief Owns one reference to a WebGPU object. Constructing from a raw handle adopts the
/// caller's reference (what wgpu*Create*() returns); use retain() to add a reference instead.
template<typename T>
class Handle final {
 public:
  Handle() noexcept = default;
  explicit Handle(T handle) noexcept : handle_(handle) {}
  ~Handle() {
    reset();
  }
  Handle(const Handle& other) noexcept : handle_(other.handle_) {
    if (handle_ != nullptr) {
      HandleTraits<T>::addRef(handle_);
    }
  }
  Handle(Handle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Handle& operator=(const Handle& other) noexcept {
    Handle(other).swap(*this);
    return *this;
  }
  Handle& operator=(Handle&& other) noexcept {
    Handle(std::move(other)).swap(*this);
    return *this;
  }
  Handle& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  [[nodiscard]] static Handle retain(T handle) noexcept {
    if (handle != nullptr) {
      HandleTraits<T>::addRef(handle);
    }
    return Handle(handle);
  }

  /// Releases the owned reference and adopts `handle`.
  void reset(T handle = nullptr) noexcept {
    const T old = std::exchange(handle_, handle);
    if (old != nullptr) {
      HandleTraits<T>::release(old);
    }
  }

  /// Gives up ownership without releasing; the caller now owns the reference.
  [[nodiscard]] T release() noexcept {
    return std::exchange(handle_, nullptr);
  }

  void swap(Handle& other) noexcept {
    std::swap(handle_, other.handle_);
  }

  [[nodiscard]] T get() const noexcept {
    return handle_;
  }

  [[nodiscard]] explicit operator bool() const noexcept {
    return handle_ != nullptr;
  }

 private:
  T handle_ = nullptr;
};

/// Returns a process-wide unique id for a texture, sampler or buffer; caches key on these ids.
[[nodiscard]] uint64_t allocateResourceId();

/// @brief `str` must be null-terminated; nullptr yields the null string view.
inline WGPUStringView toWGPUStringView(const char* IGL_NULLABLE str) noexcept {
  return {.data = str, .length = WGPU_STRLEN};
}

inline WGPUStringView toWGPUStringView(const std::string& str) noexcept {
  return {.data = str.data(), .length = str.size()};
}

/// The view would dangle once the temporary is destroyed.
WGPUStringView toWGPUStringView(const std::string&&) = delete;

std::string toStdString(WGPUStringView view);

// The functions below convert IGL values to WebGPU values. std::nullopt means WebGPU has no
// equivalent and the caller must report Unsupported. Never fall back to *_Undefined: in several
// descriptor fields (address modes, texture dimensions) it passes validation as "use the default".
// Two mappings are intentionally lossy, as on Vulkan:
//   - RGBX_UNorm8 -> RGBA8Unorm: X is stored and sampled as alpha, not forced to 1.
//   - SamplerMipFilter::Disabled -> Nearest: the sampler must also set lodMaxClamp to 0.

std::optional<WGPUTextureFormat> textureFormatToWGPUTextureFormat(TextureFormat format);
/// Returns TextureFormat::Invalid for formats IGL does not map.
TextureFormat wgpuTextureFormatToTextureFormat(WGPUTextureFormat format);
/// Returns the feature a device must enable to create textures of `format`, or std::nullopt when
/// every device supports it (or no device does, see textureFormatToWGPUTextureFormat()).
std::optional<WGPUFeatureName> getRequiredWGPUFeature(TextureFormat format);
std::optional<WGPUVertexFormat> vertexAttributeFormatToWGPUVertexFormat(
    VertexAttributeFormat format);
std::optional<WGPUBlendFactor> blendFactorToWGPUBlendFactor(BlendFactor factor);
WGPUBlendOperation blendOpToWGPUBlendOperation(BlendOp op);
WGPUColorWriteMask colorWriteMaskToWGPUColorWriteMask(ColorWriteMask mask);
WGPUCompareFunction compareFunctionToWGPUCompareFunction(CompareFunction func);
WGPUStencilOperation stencilOperationToWGPUStencilOperation(StencilOperation op);
std::optional<WGPUAddressMode> samplerAddressModeToWGPUAddressMode(SamplerAddressMode mode);
WGPUFilterMode samplerMinMagFilterToWGPUFilterMode(SamplerMinMagFilter filter);
WGPUMipmapFilterMode samplerMipFilterToWGPUMipmapFilterMode(SamplerMipFilter filter);
WGPUPrimitiveTopology primitiveTypeToWGPUPrimitiveTopology(PrimitiveType type);
WGPUCullMode cullModeToWGPUCullMode(CullMode mode);
WGPUFrontFace windingModeToWGPUFrontFace(WindingMode mode);
std::optional<WGPUIndexFormat> indexFormatToWGPUIndexFormat(IndexFormat format);
std::optional<WGPUTextureDimension> textureTypeToWGPUTextureDimension(TextureType type);
std::optional<WGPUTextureViewDimension> textureTypeToWGPUTextureViewDimension(TextureType type);
WGPULoadOp loadActionToWGPULoadOp(LoadAction action);
WGPUStoreOp storeActionToWGPUStoreOp(StoreAction action);

Result::Code wgpuErrorTypeToResultCode(WGPUErrorType type);
/// Falls back to a fixed message per error type when `message` is empty.
Result getResultFromWGPUError(WGPUErrorType type, WGPUStringView message);
void setResultFrom(Result* IGL_NULLABLE outResult, WGPUErrorType type, WGPUStringView message);

} // namespace igl::webgpu
