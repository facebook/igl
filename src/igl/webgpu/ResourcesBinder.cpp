/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/ResourcesBinder.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/DeviceFeatureSet.h>
#include <igl/webgpu/SamplerState.h>
#include <igl/webgpu/StateSanitizer.h>
#include <igl/webgpu/Texture.h>
#include <igl/webgpu/UniformArena.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

constexpr uint64_t kNonFilteringTwin = 1;

WGPUTextureViewDimension toViewDimension(TextureType type) {
  switch (type) {
  case TextureType::TwoD:
    return WGPUTextureViewDimension_2D;
  case TextureType::TwoDArray:
    return WGPUTextureViewDimension_2DArray;
  case TextureType::Cube:
    return WGPUTextureViewDimension_Cube;
  case TextureType::ThreeD:
    return WGPUTextureViewDimension_3D;
  case TextureType::Invalid:
  case TextureType::ExternalImage:
    return WGPUTextureViewDimension_Undefined;
  }
  IGL_UNREACHABLE_RETURN(WGPUTextureViewDimension_Undefined)
}

WGPUTextureFormat getDummyFormat(SampleClass sampleClass) {
  switch (sampleClass) {
  case SampleClass::Float:
  case SampleClass::UnfilterableFloat:
    return WGPUTextureFormat_RGBA8Unorm;
  case SampleClass::Depth:
    return WGPUTextureFormat_Depth32Float;
  case SampleClass::Sint:
    return WGPUTextureFormat_R32Sint;
  case SampleClass::Uint:
    return WGPUTextureFormat_R32Uint;
  }
  IGL_UNREACHABLE_RETURN(WGPUTextureFormat_RGBA8Unorm)
}

// The sampler binding type makeBindGroupLayoutEntries() chose for `binding`.
WGPUSamplerBindingType getSamplerType(const PipelineBindings& bindings,
                                      const WgslBinding& binding,
                                      SampleClasses classes) {
  if (binding.kind == WgslBindingKind::ComparisonSampler) {
    return WGPUSamplerBindingType_Comparison;
  }
  const bool filtering = bindings.find(kTextureGroup, binding.binding - 1) == nullptr ||
                         getSampleClass(classes, binding.binding / 2) == SampleClass::Float;
  return filtering ? WGPUSamplerBindingType_Filtering : WGPUSamplerBindingType_NonFiltering;
}

std::string describe(const WgslBinding& binding) {
  return "@group(" + std::to_string(binding.group) + ") @binding(" +
         std::to_string(binding.binding) + ") " + binding.name;
}

} // namespace

bool BindGroupCache::Key::operator<(const Key& other) const {
  return std::tie(layout, words) < std::tie(other.layout, other.words);
}

WGPUBindGroup IGL_NULLABLE BindGroupCache::get(WGPUBindGroupLayout IGL_NONNULL layout,
                                               const std::vector<Entry>& entries,
                                               Result* IGL_NULLABLE outResult) {
  Key key{.layout = layout};
  key.words.reserve(entries.size() * 5);
  for (const Entry& e : entries) {
    key.words.insert(key.words.end(),
                     {e.entry.binding, e.resourceId, e.variant, e.entry.offset, e.entry.size});
  }
  if (const auto it = groups_.find(key); it != groups_.end()) {
    lru_.splice(lru_.begin(), lru_, it->second.lru);
    ++hitCount_;
    Result::setOk(outResult);
    return it->second.group.get();
  }

  std::vector<WGPUBindGroupEntry> wgpuEntries;
  std::vector<uint64_t> resourceIds;
  wgpuEntries.reserve(entries.size());
  for (const Entry& e : entries) {
    wgpuEntries.push_back(e.entry);
    resourceIds.push_back(e.resourceId);
  }
  WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
  desc.layout = layout;
  desc.entryCount = wgpuEntries.size();
  desc.entries = wgpuEntries.data();
  ctx_.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPUBindGroup> group(wgpuDeviceCreateBindGroup(ctx_.getDevice(), &desc));
  Result result = ctx_.popErrorScope();
  if (!result.isOk() || !group) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  ++creationCount_;

  if (groups_.size() >= capacity_ && !lru_.empty()) {
    groups_.erase(lru_.back());
    lru_.pop_back();
  }
  lru_.push_front(key);
  const WGPUBindGroup rawGroup = group.get();
  groups_.emplace(
      std::move(key),
      Value{.group = std::move(group), .lru = lru_.begin(), .resourceIds = std::move(resourceIds)});
  Result::setOk(outResult);
  return rawGroup;
}

void BindGroupCache::evict(uint64_t resourceId) {
  for (auto it = groups_.begin(); it != groups_.end();) {
    const auto& ids = it->second.resourceIds;
    if (std::find(ids.begin(), ids.end(), resourceId) != ids.end()) {
      lru_.erase(it->second.lru);
      it = groups_.erase(it);
    } else {
      ++it;
    }
  }
}

void BindGroupCache::clear() {
  groups_.clear();
  lru_.clear();
}

WGPUTextureView IGL_NULLABLE DummyResources::getTextureView(WGPUTextureViewDimension dimension,
                                                            SampleClass sampleClass,
                                                            bool multisampled) {
  const auto key = std::make_tuple(dimension, sampleClass, multisampled);
  if (const auto it = views_.find(key); it != views_.end()) {
    return it->second.get();
  }
  const bool isCube = dimension == WGPUTextureViewDimension_Cube ||
                      dimension == WGPUTextureViewDimension_CubeArray;
  WGPUTextureDescriptor textureDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
  textureDesc.label = toWGPUStringView("igl.webgpu.dummy");
  textureDesc.usage = WGPUTextureUsage_TextureBinding |
                      (multisampled ? WGPUTextureUsage_RenderAttachment : WGPUTextureUsage_None);
  textureDesc.dimension = dimension == WGPUTextureViewDimension_3D   ? WGPUTextureDimension_3D
                          : dimension == WGPUTextureViewDimension_1D ? WGPUTextureDimension_1D
                                                                     : WGPUTextureDimension_2D;
  textureDesc.size = {.width = 1, .height = 1, .depthOrArrayLayers = isCube ? 6u : 1u};
  textureDesc.format = getDummyFormat(sampleClass);
  textureDesc.sampleCount = multisampled ? 4 : 1;
  Handle<WGPUTexture> texture(wgpuDeviceCreateTexture(ctx_.getDevice(), &textureDesc));

  WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  viewDesc.dimension = dimension;
  Handle<WGPUTextureView> view(wgpuTextureCreateView(texture.get(), &viewDesc));
  const WGPUTextureView result = view.get();
  textures_.emplace(key, std::move(texture));
  views_.emplace(key, std::move(view));
  return result;
}

WGPUTextureView IGL_NULLABLE
DummyResources::getStorageTextureView(uint32_t binding,
                                      WGPUTextureViewDimension dimension,
                                      WGPUTextureFormat format) {
  const auto key = std::make_tuple(binding, dimension, format);
  if (const auto it = storageViews_.find(key); it != storageViews_.end()) {
    return it->second.get();
  }
  WGPUTextureDescriptor textureDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
  textureDesc.label = toWGPUStringView("igl.webgpu.dummy");
  textureDesc.usage = WGPUTextureUsage_StorageBinding;
  textureDesc.dimension = dimension == WGPUTextureViewDimension_3D   ? WGPUTextureDimension_3D
                          : dimension == WGPUTextureViewDimension_1D ? WGPUTextureDimension_1D
                                                                     : WGPUTextureDimension_2D;
  textureDesc.size = {.width = 1, .height = 1, .depthOrArrayLayers = 1};
  textureDesc.format = format;
  Handle<WGPUTexture> texture(wgpuDeviceCreateTexture(ctx_.getDevice(), &textureDesc));
  WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  viewDesc.dimension = dimension;
  Handle<WGPUTextureView> view(wgpuTextureCreateView(texture.get(), &viewDesc));
  const WGPUTextureView result = view.get();
  storageTextures_.emplace(key, std::move(texture));
  storageViews_.emplace(key, std::move(view));
  return result;
}

WGPUSampler IGL_NULLABLE DummyResources::getSampler(WGPUSamplerBindingType type) {
  if (const auto it = samplers_.find(type); it != samplers_.end()) {
    return it->second.get();
  }
  WGPUSamplerDescriptor desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
  if (type == WGPUSamplerBindingType_Comparison) {
    desc.compare = WGPUCompareFunction_Less;
  }
  Handle<WGPUSampler> sampler(wgpuDeviceCreateSampler(ctx_.getDevice(), &desc));
  const WGPUSampler result = sampler.get();
  samplers_.emplace(type, std::move(sampler));
  return result;
}

WGPUBuffer IGL_NULLABLE DummyResources::getBuffer(uint64_t size) {
  if (!buffer_ || bufferSize_ < size) {
    bufferSize_ = std::max<uint64_t>((size + 255) / 256 * 256, 256);
    const WGPUBufferDescriptor desc = {
        .nextInChain = nullptr,
        .label = toWGPUStringView("igl.webgpu.dummy"),
        .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_Storage,
        .size = bufferSize_,
        .mappedAtCreation = 0,
    };
    buffer_.reset(wgpuDeviceCreateBuffer(ctx_.getDevice(), &desc));
  }
  return buffer_.get();
}

WGPUBuffer IGL_NULLABLE DummyResources::getWritableStorageBuffer(uint32_t binding, uint64_t size) {
  auto& [buffer, bufferSize] = writableStorageBuffers_[binding];
  if (!buffer || bufferSize < size) {
    bufferSize = std::max<uint64_t>((size + 255) / 256 * 256, 256);
    const WGPUBufferDescriptor desc = {
        .nextInChain = nullptr,
        .label = toWGPUStringView("igl.webgpu.dummy.storage"),
        .usage = WGPUBufferUsage_Storage,
        .size = bufferSize,
        .mappedAtCreation = 0,
    };
    buffer.reset(wgpuDeviceCreateBuffer(ctx_.getDevice(), &desc));
  }
  return buffer.get();
}

ResourcesBinder::ResourcesBinder(WebGPUContext& ctx, const DeviceFeatureSet& features) :
  ctx_(ctx), features_(features) {}

void ResourcesBinder::bindTexture(uint32_t unit, Texture* IGL_NULLABLE texture) {
  if (unit < kMaxTextureUnits) {
    textures_[unit] = texture;
  }
}

void ResourcesBinder::bindSampler(uint32_t unit, SamplerState* IGL_NULLABLE sampler) {
  if (unit < kMaxTextureUnits) {
    samplers_[unit] = sampler;
  }
}

void ResourcesBinder::bindBuffer(uint32_t index,
                                 Buffer* IGL_NULLABLE buffer,
                                 size_t offset,
                                 size_t size) {
  if (index < buffers_.size()) {
    buffers_[index] = {.buffer = buffer, .offset = offset, .size = size};
  }
}

void ResourcesBinder::bindStorageTexture(uint32_t index, Texture* IGL_NULLABLE texture) {
  if (index < kMaxStorageTextures) {
    storageTextures_[index] = texture;
  }
}

Result ResourcesBinder::getSampleClasses(const PipelineLayoutSource& pipeline,
                                         SampleClasses& outClasses) const {
  const bool float32Filterable = features_.hasWGPUFeature(WGPUFeatureName_Float32Filterable);
  outClasses = 0;
  for (const PipelineBinding& binding : pipeline.getBindings().groups[kTextureGroup]) {
    const WgslBinding& declaration = binding.declaration;
    if (declaration.binding % 2 != 0) {
      continue;
    }
    const uint32_t unit = declaration.binding / 2;
    const Texture* texture = textures_[unit];
    if (texture == nullptr) {
      outClasses = setSampleClass(outClasses, unit, getDefaultSampleClass(declaration));
      continue;
    }
    const std::optional<SampleClass> sampleClass =
        getSampleClass(declaration, texture->getFormat(), float32Filterable);
    if (!sampleClass) {
      return Result(Result::Code::ArgumentInvalid,
                    describe(declaration) + " cannot sample a texture of format " +
                        texture->getProperties().name);
    }
    outClasses = setSampleClass(outClasses, unit, *sampleClass);
  }
  return Result();
}

Result ResourcesBinder::makeTextureGroup(const PipelineLayoutSource& pipeline,
                                         SampleClasses classes,
                                         std::vector<BindGroupCache::Entry>& outEntries) {
  DummyResources& dummies = ctx_.getDummyResources();
  const PipelineBindings& bindings = pipeline.getBindings();
  for (const PipelineBinding& binding : bindings.groups[kTextureGroup]) {
    const WgslBinding& declaration = binding.declaration;
    const uint32_t unit = declaration.binding / 2;
    BindGroupCache::Entry entry;
    entry.entry.binding = declaration.binding;
    if (declaration.binding % 2 == 0) {
      const Texture* texture = textures_[unit];
      if (texture == nullptr) {
        entry.entry.textureView =
            dummies.getTextureView(declaration.viewDimension,
                                   getSampleClass(classes, unit),
                                   declaration.kind == WgslBindingKind::MultisampledTexture);
        entry.resourceId = dummies.getResourceId();
      } else {
        if (toViewDimension(texture->getType()) != declaration.viewDimension) {
          return Result(Result::Code::ArgumentInvalid,
                        describe(declaration) + " is bound to a texture of another type");
        }
        entry.entry.textureView = texture->getSampledView();
        entry.resourceId = texture->getTextureId();
      }
    } else {
      const WGPUSamplerBindingType type = getSamplerType(bindings, declaration, classes);
      const SamplerState* sampler = samplers_[unit];
      if (sampler == nullptr) {
        entry.entry.sampler = dummies.getSampler(type);
        entry.resourceId = dummies.getResourceId();
      } else if ((type == WGPUSamplerBindingType_Comparison) != sampler->isComparison()) {
        return Result(Result::Code::ArgumentInvalid,
                      describe(declaration) + (sampler->isComparison()
                                                   ? " is not a comparison sampler"
                                                   : " needs a comparison sampler"));
      } else if (type == WGPUSamplerBindingType_NonFiltering && sampler->isFiltering()) {
        IGL_LOG_INFO_ONCE(
            "WebGPU cannot filter the texture of a unit; sampling it with nearest filtering\n");
        entry.entry.sampler = sampler->getNonFilteringSampler();
        entry.resourceId = sampler->getSamplerId();
        entry.variant = kNonFilteringTwin;
      } else {
        entry.entry.sampler = sampler->getWGPUSampler();
        entry.resourceId = sampler->getSamplerId();
      }
    }
    outEntries.push_back(entry);
  }
  return Result();
}

Result ResourcesBinder::makeBufferGroup(const PipelineLayoutSource& pipeline,
                                        std::vector<BindGroupCache::Entry>& outEntries,
                                        std::vector<uint32_t>& outDynamicOffsets) {
  const WGPULimits& limits = features_.getLimits();
  for (const PipelineBinding& binding : pipeline.getBindings().groups[kBufferGroup]) {
    const WgslBinding& declaration = binding.declaration;
    const bool isUniform = declaration.kind == WgslBindingKind::UniformBuffer;
    const uint64_t maxBindingSize = isUniform ? limits.maxUniformBufferBindingSize
                                              : limits.maxStorageBufferBindingSize;
    BindGroupCache::Entry entry;
    entry.entry.binding = declaration.binding;
    const BufferSlot& slot = declaration.binding < buffers_.size() ? buffers_[declaration.binding]
                                                                   : BufferSlot{};
    if (slot.buffer == nullptr) {
      const uint64_t size = std::max<uint64_t>((declaration.bufferSize + 15) / 16 * 16, 16);
      entry.entry.buffer =
          declaration.kind == WgslBindingKind::StorageBuffer
              ? ctx_.getDummyResources().getWritableStorageBuffer(declaration.binding, size)
              : ctx_.getDummyResources().getBuffer(size);
      entry.entry.size = size;
      entry.resourceId = ctx_.getDummyResources().getResourceId();
      if (binding.hasDynamicOffset) {
        outDynamicOffsets.push_back(0);
      }
      outEntries.push_back(entry);
      continue;
    }
    const Buffer& buffer = *slot.buffer;
    const auto requiredType = isUniform ? BufferDesc::BufferTypeBits::Uniform
                                        : BufferDesc::BufferTypeBits::Storage;
    if ((buffer.getBufferType() & requiredType) == 0) {
      return Result(Result::Code::ArgumentInvalid,
                    describe(declaration) +
                        (isUniform ? " needs a Uniform buffer" : " needs a Storage buffer"));
    }
    if (!isDynamicOffsetAligned(slot.offset)) {
      return Result(Result::Code::ArgumentInvalid,
                    describe(declaration) + ": buffer offsets must be multiples of 256 bytes");
    }
    if (slot.offset >= buffer.getAllocatedSize()) {
      return Result(Result::Code::ArgumentOutOfRange,
                    describe(declaration) + ": the buffer offset is out of range");
    }
    const uint64_t remaining = buffer.getAllocatedSize() - slot.offset;
    // A binding smaller than the declared struct is widened when the buffer has room (bindBytes()
    // slices are 16-byte multiples of the payload); WebGPU rejects bindings below the struct size.
    const uint64_t requested =
        slot.size != 0 ? std::max<uint64_t>(slot.size, declaration.bufferSize) : remaining;
    const uint64_t size = std::min<uint64_t>(requested, std::min(remaining, maxBindingSize));
    if (size < declaration.bufferSize) {
      return Result(Result::Code::ArgumentOutOfRange,
                    describe(declaration) + ": the buffer is smaller than the declared struct");
    }
    entry.entry.buffer = buffer.getWGPUBuffer();
    entry.entry.size = size;
    entry.resourceId = buffer.getResourceId();
    if (binding.hasDynamicOffset) {
      outDynamicOffsets.push_back(static_cast<uint32_t>(slot.offset));
    } else {
      entry.entry.offset = slot.offset;
    }
    outEntries.push_back(entry);
  }
  return Result();
}

Result ResourcesBinder::updatePushConstants(const void* IGL_NULLABLE data,
                                            size_t length,
                                            size_t offset) {
  if (data == nullptr || length == 0 || offset > pushConstants_.size() ||
      length > pushConstants_.size() - offset) {
    return Result(Result::Code::ArgumentOutOfRange,
                  "Push constants are at most " + std::to_string(kMaxPushConstantBytes) + " bytes");
  }
  std::copy_n(static_cast<const uint8_t*>(data), length, pushConstants_.data() + offset);
  pushConstantsUpdated_ = true;
  return Result();
}

void ResourcesBinder::stagePushConstants(UniformArena& arena) {
  if (!pushConstantsUpdated_) {
    return;
  }
  pushConstantsUpdated_ = false;
  const UniformArena::Slice slice = arena.allocate(pushConstants_.data(), pushConstants_.size());
  pushConstantSlot_ = {.buffer = slice.buffer, .offset = slice.offset, .size = slice.size};
}

void ResourcesBinder::makePushConstantGroup(const PipelineLayoutSource& pipeline,
                                            std::vector<BindGroupCache::Entry>& outEntries,
                                            std::vector<uint32_t>& outDynamicOffsets) {
  for (const PipelineBinding& binding : pipeline.getBindings().groups[kPushConstantGroup]) {
    const uint64_t size = std::max<uint64_t>((binding.declaration.bufferSize + 15) / 16 * 16, 16);
    BindGroupCache::Entry entry;
    entry.entry.binding = binding.declaration.binding;
    entry.entry.size = size;
    // Push constants that were never set read zeros from the dummy buffer.
    if (pushConstantSlot_.buffer != nullptr) {
      entry.entry.buffer = pushConstantSlot_.buffer->getWGPUBuffer();
      entry.resourceId = pushConstantSlot_.buffer->getResourceId();
      outDynamicOffsets.push_back(static_cast<uint32_t>(pushConstantSlot_.offset));
    } else {
      entry.entry.buffer = ctx_.getDummyResources().getBuffer(size);
      entry.resourceId = ctx_.getDummyResources().getResourceId();
      outDynamicOffsets.push_back(0);
    }
    outEntries.push_back(entry);
  }
}

Texture* IGL_NULLABLE ResourcesBinder::getStorageTexture(uint32_t index) const {
  if (index < storageTextures_.size() && storageTextures_[index] != nullptr) {
    return storageTextures_[index];
  }
  // Like Metal, bindTexture() also binds storage textures (unit i serves storage texture i).
  Texture* texture = index < textures_.size() ? textures_[index] : nullptr;
  return texture != nullptr && (texture->getUsage() & TextureDesc::TextureUsageBits::Storage) != 0
             ? texture
             : nullptr;
}

Result ResourcesBinder::makeStorageTextureGroup(const PipelineLayoutSource& pipeline,
                                                std::vector<BindGroupCache::Entry>& outEntries) {
  for (const PipelineBinding& binding : pipeline.getBindings().groups[kStorageTextureGroup]) {
    const WgslBinding& declaration = binding.declaration;
    BindGroupCache::Entry entry;
    entry.entry.binding = declaration.binding;
    Texture* texture = getStorageTexture(declaration.binding);
    if (texture == nullptr) {
      entry.entry.textureView = ctx_.getDummyResources().getStorageTextureView(
          declaration.binding, declaration.viewDimension, declaration.storageFormat);
      entry.resourceId = ctx_.getDummyResources().getResourceId();
    } else {
      if ((texture->getUsage() & TextureDesc::TextureUsageBits::Storage) == 0) {
        return Result(Result::Code::ArgumentInvalid,
                      describe(declaration) + " is bound to a texture without Storage usage");
      }
      if (texture->getWGPUFormat() != declaration.storageFormat) {
        return Result(Result::Code::ArgumentInvalid,
                      describe(declaration) + " is bound to a texture of another format");
      }
      WGPUTextureView view = texture->getStorageView(declaration.viewDimension);
      if (view == nullptr) {
        return Result(Result::Code::ArgumentInvalid,
                      describe(declaration) + " is bound to a texture of another type");
      }
      entry.entry.textureView = view;
      entry.resourceId = texture->getTextureId();
    }
    outEntries.push_back(entry);
  }
  return Result();
}

Result ResourcesBinder::flush(WGPURenderPassEncoder IGL_NONNULL pass,
                              PipelineLayoutSource& pipeline,
                              SampleClasses classes,
                              uint64_t serial) {
  return flush(
      pass,
      [](void* IGL_NONNULL p,
         uint32_t group,
         WGPUBindGroup IGL_NONNULL bindGroup,
         size_t numDynamicOffsets,
         const uint32_t* IGL_NULLABLE dynamicOffsets) {
        wgpuRenderPassEncoderSetBindGroup(static_cast<WGPURenderPassEncoder>(p),
                                          group,
                                          bindGroup,
                                          numDynamicOffsets,
                                          dynamicOffsets);
      },
      pipeline,
      classes,
      serial);
}

Result ResourcesBinder::flush(WGPUComputePassEncoder IGL_NONNULL pass,
                              PipelineLayoutSource& pipeline,
                              SampleClasses classes,
                              uint64_t serial) {
  return flush(
      pass,
      [](void* IGL_NONNULL p,
         uint32_t group,
         WGPUBindGroup IGL_NONNULL bindGroup,
         size_t numDynamicOffsets,
         const uint32_t* IGL_NULLABLE dynamicOffsets) {
        wgpuComputePassEncoderSetBindGroup(static_cast<WGPUComputePassEncoder>(p),
                                           group,
                                           bindGroup,
                                           numDynamicOffsets,
                                           dynamicOffsets);
      },
      pipeline,
      classes,
      serial);
}

Result ResourcesBinder::flush(void* IGL_NONNULL pass,
                              SetBindGroup setBindGroup,
                              PipelineLayoutSource& pipeline,
                              SampleClasses classes,
                              uint64_t serial) {
  const PipelineBindings& bindings = pipeline.getBindings();
  for (uint32_t group = 0; group < kNumBindGroups; ++group) {
    if (bindings.groups[group].empty()) {
      continue;
    }
    std::vector<BindGroupCache::Entry> entries;
    std::vector<uint32_t> dynamicOffsets;
    Result result;
    switch (group) {
    case kTextureGroup:
      result = makeTextureGroup(pipeline, classes, entries);
      break;
    case kBufferGroup:
      result = makeBufferGroup(pipeline, entries, dynamicOffsets);
      break;
    case kStorageTextureGroup:
      result = makeStorageTextureGroup(pipeline, entries);
      break;
    default:
      makePushConstantGroup(pipeline, entries, dynamicOffsets);
      break;
    }
    if (!result.isOk()) {
      return result;
    }
    WGPUBindGroupLayout layout = pipeline.getBindGroupLayout(group, classes, &result);
    if (layout == nullptr) {
      return result;
    }
    WGPUBindGroup bindGroup = ctx_.getBindGroupCache().get(layout, entries, &result);
    if (bindGroup == nullptr) {
      return result;
    }
    BoundGroup& bound = boundGroups_[group];
    if (bound.group != bindGroup || bound.dynamicOffsets != dynamicOffsets) {
      setBindGroup(pass, group, bindGroup, dynamicOffsets.size(), dynamicOffsets.data());
      bound = {.group = bindGroup, .dynamicOffsets = std::move(dynamicOffsets)};
    }
  }
  for (uint32_t unit = 0; unit < kMaxTextureUnits; ++unit) {
    if (textures_[unit] != nullptr && (bindings.getTextureUnitMask() & (1u << unit)) != 0) {
      textures_[unit]->recordUse(serial);
    }
  }
  for (const PipelineBinding& binding : bindings.groups[kBufferGroup]) {
    const uint32_t index = binding.declaration.binding;
    if (index < buffers_.size() && buffers_[index].buffer != nullptr) {
      buffers_[index].buffer->recordUse(serial,
                                        binding.declaration.kind == WgslBindingKind::StorageBuffer);
    }
  }
  for (const PipelineBinding& binding : bindings.groups[kStorageTextureGroup]) {
    if (Texture* texture = getStorageTexture(binding.declaration.binding)) {
      texture->recordUse(serial);
    }
  }
  return Result();
}

} // namespace igl::webgpu
