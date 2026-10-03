/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <tuple>
#include <vector>
#include <igl/Common.h>
#include <igl/webgpu/BindLayouts.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class Buffer;
class DeviceFeatureSet;
class SamplerState;
class Texture;
class UniformArena;
class WebGPUContext;

/// @brief LRU cache of bind groups keyed by layout and the ids, offsets and sizes of their
/// resources. Resources evict their bind groups when they are destroyed.
class BindGroupCache final {
 public:
  static constexpr size_t kDefaultCapacity = 1024;

  struct Entry {
    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    /// Identifies the texture, sampler or buffer of the entry (see allocateResourceId()).
    uint64_t resourceId = 0;
    /// Distinguishes different WebGPU objects created for one resource (a sampler's nearest twin).
    uint64_t variant = 0;
  };

  explicit BindGroupCache(const WebGPUContext& ctx, size_t capacity = kDefaultCapacity) :
    ctx_(ctx), capacity_(capacity) {}

  /// The bind group of `layout` with `entries` (sorted by binding), created on a miss.
  [[nodiscard]] WGPUBindGroup IGL_NULLABLE get(WGPUBindGroupLayout IGL_NONNULL layout,
                                               const std::vector<Entry>& entries,
                                               Result* IGL_NULLABLE outResult);
  /// Drops every bind group that references `resourceId`.
  void evict(uint64_t resourceId);
  void clear();

  [[nodiscard]] size_t size() const noexcept {
    return groups_.size();
  }
  [[nodiscard]] size_t getCreationCount() const noexcept {
    return creationCount_;
  }
  [[nodiscard]] size_t getHitCount() const noexcept {
    return hitCount_;
  }

 private:
  struct Key {
    WGPUBindGroupLayout layout = nullptr;
    std::vector<uint64_t> words;
    bool operator<(const Key& other) const;
  };
  struct Value {
    Handle<WGPUBindGroup> group;
    std::list<Key>::iterator lru;
    std::vector<uint64_t> resourceIds;
  };

  const WebGPUContext& ctx_;
  const size_t capacity_;
  std::map<Key, Value> groups_;
  // Most recently used first.
  std::list<Key> lru_;
  size_t creationCount_ = 0;
  size_t hitCount_ = 0;
};

/// @brief Placeholder resources for declared bindings that nothing is bound to, so every
/// pipeline layout can be satisfied.
class DummyResources final {
 public:
  explicit DummyResources(const WebGPUContext& ctx) : ctx_(ctx) {}

  [[nodiscard]] WGPUTextureView IGL_NULLABLE getTextureView(WGPUTextureViewDimension dimension,
                                                            SampleClass sampleClass,
                                                            bool multisampled);
  /// One placeholder per `binding`: Dawn rejects writable storage bindings that alias.
  [[nodiscard]] WGPUTextureView IGL_NULLABLE
  getStorageTextureView(uint32_t binding,
                        WGPUTextureViewDimension dimension,
                        WGPUTextureFormat format);
  [[nodiscard]] WGPUSampler IGL_NULLABLE getSampler(WGPUSamplerBindingType type);
  /// A buffer with Uniform and Storage usage of at least `size` bytes.
  [[nodiscard]] WGPUBuffer IGL_NULLABLE getBuffer(uint64_t size);
  /// One placeholder per `binding` for writable storage buffers: Dawn rejects writable bindings
  /// that alias each other or a read-only binding of the same buffer.
  [[nodiscard]] WGPUBuffer IGL_NULLABLE getWritableStorageBuffer(uint32_t binding, uint64_t size);
  /// Resource id of every dummy resource.
  [[nodiscard]] uint64_t getResourceId() const noexcept {
    return resourceId_;
  }

 private:
  const WebGPUContext& ctx_;
  const uint64_t resourceId_ = allocateResourceId();
  std::map<std::tuple<WGPUTextureViewDimension, SampleClass, bool>, Handle<WGPUTexture>> textures_;
  std::map<std::tuple<WGPUTextureViewDimension, SampleClass, bool>, Handle<WGPUTextureView>> views_;
  std::map<std::tuple<uint32_t, WGPUTextureViewDimension, WGPUTextureFormat>, Handle<WGPUTexture>>
      storageTextures_;
  std::map<std::tuple<uint32_t, WGPUTextureViewDimension, WGPUTextureFormat>,
           Handle<WGPUTextureView>>
      storageViews_;
  std::map<WGPUSamplerBindingType, Handle<WGPUSampler>> samplers_;
  Handle<WGPUBuffer> buffer_;
  uint64_t bufferSize_ = 0;
  std::map<uint32_t, std::pair<Handle<WGPUBuffer>, uint64_t>> writableStorageBuffers_;
};

/// @brief Tracks the textures, samplers, buffers and storage textures bound on a render or compute
/// encoder and turns them into bind groups for the current pipeline at draw or dispatch time.
class ResourcesBinder final {
 public:
  ResourcesBinder(WebGPUContext& ctx, const DeviceFeatureSet& features);

  void bindTexture(uint32_t unit, Texture* IGL_NULLABLE texture);
  void bindSampler(uint32_t unit, SamplerState* IGL_NULLABLE sampler);
  /// `size` 0 binds the rest of the buffer from `offset`.
  void bindBuffer(uint32_t index, Buffer* IGL_NULLABLE buffer, size_t offset, size_t size);
  void bindStorageTexture(uint32_t index, Texture* IGL_NULLABLE texture);
  /// Updates bytes [offset, offset + length) of the push constants; they reach the GPU through
  /// stagePushConstants() and the group 3 uniform buffer.
  [[nodiscard]] Result updatePushConstants(const void* IGL_NULLABLE data,
                                           size_t length,
                                           size_t offset);
  /// Copies updated push constants into `arena` for the following flushes; a no-op otherwise.
  void stagePushConstants(UniformArena& arena);

  /// Sample classes of the bound textures for the texture units `pipeline` declares; unbound
  /// units get the class of their declaration. ArgumentInvalid when a declaration cannot sample
  /// the bound texture's format (see getSampleClass()).
  [[nodiscard]] Result getSampleClasses(const PipelineLayoutSource& pipeline,
                                        SampleClasses& outClasses) const;
  /// Sets the bind groups `pipeline` (created for `classes`) needs on `pass`, and records the use
  /// of the bound resources by the command buffer with `serial`.
  [[nodiscard]] Result flush(WGPURenderPassEncoder IGL_NONNULL pass,
                             PipelineLayoutSource& pipeline,
                             SampleClasses classes,
                             uint64_t serial);
  [[nodiscard]] Result flush(WGPUComputePassEncoder IGL_NONNULL pass,
                             PipelineLayoutSource& pipeline,
                             SampleClasses classes,
                             uint64_t serial);

 private:
  struct BufferSlot {
    Buffer* IGL_NULLABLE buffer = nullptr;
    size_t offset = 0;
    size_t size = 0;
  };
  struct BoundGroup {
    WGPUBindGroup IGL_NULLABLE group = nullptr;
    std::vector<uint32_t> dynamicOffsets;
  };

  using SetBindGroup = void (*)(void* IGL_NONNULL pass,
                                uint32_t group,
                                WGPUBindGroup IGL_NONNULL bindGroup,
                                size_t numDynamicOffsets,
                                const uint32_t* IGL_NULLABLE dynamicOffsets);

  [[nodiscard]] Result flush(void* IGL_NONNULL pass,
                             SetBindGroup setBindGroup,
                             PipelineLayoutSource& pipeline,
                             SampleClasses classes,
                             uint64_t serial);
  [[nodiscard]] Result makeTextureGroup(const PipelineLayoutSource& pipeline,
                                        SampleClasses classes,
                                        std::vector<BindGroupCache::Entry>& outEntries);
  [[nodiscard]] Result makeBufferGroup(const PipelineLayoutSource& pipeline,
                                       std::vector<BindGroupCache::Entry>& outEntries,
                                       std::vector<uint32_t>& outDynamicOffsets);
  [[nodiscard]] Result makeStorageTextureGroup(const PipelineLayoutSource& pipeline,
                                               std::vector<BindGroupCache::Entry>& outEntries);
  [[nodiscard]] Texture* IGL_NULLABLE getStorageTexture(uint32_t index) const;
  void makePushConstantGroup(const PipelineLayoutSource& pipeline,
                             std::vector<BindGroupCache::Entry>& outEntries,
                             std::vector<uint32_t>& outDynamicOffsets);

  WebGPUContext& ctx_;
  const DeviceFeatureSet& features_;
  std::array<Texture*, kMaxTextureUnits> textures_ = {};
  std::array<SamplerState*, kMaxTextureUnits> samplers_ = {};
  std::array<BufferSlot, IGL_BUFFER_BINDINGS_MAX> buffers_ = {};
  std::array<Texture*, kMaxStorageTextures> storageTextures_ = {};
  std::array<uint8_t, kMaxPushConstantBytes> pushConstants_ = {};
  bool pushConstantsUpdated_ = false;
  BufferSlot pushConstantSlot_;
  std::array<BoundGroup, kNumBindGroups> boundGroups_ = {};
};

} // namespace igl::webgpu
