/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Buffer.h>

#include <algorithm>
#include <cstring>
#include <utility>
#include <igl/Common.h>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/Readback.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

constexpr size_t kCopyAlignment = 4;

size_t alignDown(size_t value) {
  return value / kCopyAlignment * kCopyAlignment;
}

size_t alignUp(size_t value) {
  return (value + kCopyAlignment - 1) / kCopyAlignment * kCopyAlignment;
}

WGPUBufferUsage toWGPUBufferUsage(BufferDesc::BufferType type) {
  WGPUBufferUsage usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
  if ((type & BufferDesc::BufferTypeBits::Index) != 0) {
    usage |= WGPUBufferUsage_Index;
  }
  if ((type & BufferDesc::BufferTypeBits::Vertex) != 0) {
    usage |= WGPUBufferUsage_Vertex;
  }
  if ((type & BufferDesc::BufferTypeBits::Uniform) != 0) {
    usage |= WGPUBufferUsage_Uniform;
  }
  if ((type & BufferDesc::BufferTypeBits::Storage) != 0) {
    usage |= WGPUBufferUsage_Storage;
  }
  if ((type & BufferDesc::BufferTypeBits::Indirect) != 0) {
    usage |= WGPUBufferUsage_Indirect;
  }
  return usage;
}

bool isInBounds(const BufferRange& range, size_t length) {
  return range.offset <= length && range.size <= length - range.offset;
}

} // namespace

std::unique_ptr<Buffer> Buffer::create(WebGPUContext& ctx,
                                       const BufferDesc& desc,
                                       Result* IGL_NULLABLE outResult) {
  // Uniform buffers are padded to 16 bytes: WGSL rounds uniform structs up to 16 bytes, and a
  // binding must cover the whole struct (a C++ struct of a mat4 and a float is 68 bytes, its WGSL
  // counterpart 80).
  const size_t alignment = (desc.type & BufferDesc::BufferTypeBits::Uniform) != 0 ? size_t{16}
                                                                                  : kCopyAlignment;
  const size_t allocatedSize =
      std::max((desc.length + alignment - 1) / alignment * alignment, kCopyAlignment);
  WGPULimits limits = WGPU_LIMITS_INIT;
  if (wgpuDeviceGetLimits(ctx.getDevice(), &limits) == WGPUStatus_Success &&
      allocatedSize > limits.maxBufferSize) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Buffer exceeds maxBufferSize");
    return nullptr;
  }

  const WGPUBufferDescriptor bufferDesc = {
      .nextInChain = nullptr,
      .label = toWGPUStringView(desc.debugName),
      .usage = toWGPUBufferUsage(desc.type),
      .size = allocatedSize,
      .mappedAtCreation = desc.data != nullptr ? 1u : 0u,
  };
  ctx.pushErrorScope(WGPUErrorFilter_OutOfMemory);
  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPUBuffer> buffer(wgpuDeviceCreateBuffer(ctx.getDevice(), &bufferDesc));
  Result scopes = ctx.popErrorScopes(2);
  if (!scopes.isOk() || !buffer) {
    Result::setResult(outResult, std::move(scopes));
    return nullptr;
  }

  auto result = std::unique_ptr<Buffer>(new Buffer(ctx, desc, std::move(buffer)));
  if (desc.data != nullptr) {
    std::memcpy(result->shadow_.data(), desc.data, desc.length);
    void* mapped = wgpuBufferGetMappedRange(result->buffer_.get(), 0, allocatedSize);
    if (mapped == nullptr) {
      Result::setResult(outResult, Result::Code::RuntimeError, "wgpuBufferGetMappedRange() failed");
      return nullptr;
    }
    std::memcpy(mapped, result->shadow_.data(), allocatedSize);
    wgpuBufferUnmap(result->buffer_.get());
  }
  Result::setOk(outResult);
  return result;
}

Buffer::Buffer(WebGPUContext& ctx, const BufferDesc& desc, Handle<WGPUBuffer> buffer) :
  ctx_(ctx),
  type_(desc.type),
  hint_(desc.hint),
  storage_(desc.storage),
  length_(desc.length),
  buffer_(std::move(buffer)),
  shadow_(static_cast<size_t>(wgpuBufferGetSize(buffer_.get())), 0) {}

Buffer::~Buffer() {
  ctx_.evictBindGroups(resourceId_);
  ctx_.getResourceTracker().retire(std::move(buffer_), lastUseSerial_);
}

Result Buffer::upload(const void* IGL_NULLABLE data, const BufferRange& range) {
  if (data == nullptr) {
    return Result(Result::Code::ArgumentNull, "Buffer upload data is null");
  }
  if (!isInBounds(range, length_)) {
    return Result(Result::Code::ArgumentOutOfRange, "Buffer upload is out of bounds");
  }
  if (range.size == 0) {
    return Result();
  }
  const size_t begin = alignDown(range.offset);
  const size_t end = alignUp(range.offset + range.size);
  if (isShadowStale() && (begin != range.offset || end != range.offset + range.size)) {
    // The padding bytes written back around an unaligned range must be the GPU's current bytes.
    Result result = refreshShadow();
    if (!result.isOk()) {
      return result;
    }
  }
  std::memcpy(shadow_.data() + range.offset, data, range.size);
  writeShadowRange(begin, end - begin);
  return Result();
}

void* IGL_NULLABLE Buffer::map(const BufferRange& range, Result* IGL_NULLABLE outResult) {
  if (!isInBounds(range, length_)) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Buffer map is out of bounds");
    return nullptr;
  }
  if (isShadowStale()) {
    Result result = refreshShadow();
    if (!result.isOk()) {
      Result::setResult(outResult, std::move(result));
      return nullptr;
    }
  }
  mappedRange_ = range;
  Result::setOk(outResult);
  return shadow_.data() + range.offset;
}

void Buffer::unmap() {
  if (!mappedRange_) {
    return;
  }
  // The mapped pointer is writable, so write the mapped range back.
  if (mappedRange_->size != 0) {
    const size_t begin = alignDown(mappedRange_->offset);
    const size_t end = alignUp(mappedRange_->offset + mappedRange_->size);
    writeShadowRange(begin, end - begin);
  }
  mappedRange_.reset();
}

Result Buffer::refreshShadow() {
  Result result = readBuffer(ctx_, buffer_.get(), 0, shadow_.size(), shadow_.data());
  if (result.isOk() && !ctx_.getResourceTracker().isOpen(gpuWriteSerial_)) {
    gpuWriteSerial_ = 0;
  }
  return result;
}

void Buffer::writeShadowRange(size_t offset, size_t size) {
  wgpuQueueWriteBuffer(ctx_.getQueue(), buffer_.get(), offset, shadow_.data() + offset, size);
}

void Buffer::recordUse(uint64_t serial, bool gpuWrite) noexcept {
  lastUseSerial_ = std::max(lastUseSerial_, serial);
  if (gpuWrite) {
    gpuWriteSerial_ = std::max(gpuWriteSerial_, serial);
  }
}

BufferDesc::BufferAPIHint Buffer::requestedApiHints() const noexcept {
  return hint_;
}

BufferDesc::BufferAPIHint Buffer::acceptedApiHints() const noexcept {
  // Queue-ordered writes already keep frames in flight intact, so Ring is accepted as a no-op.
  return hint_ & BufferDesc::BufferAPIHintBits::Ring;
}

ResourceStorage Buffer::storage() const noexcept {
  return storage_;
}

size_t Buffer::getSizeInBytes() const {
  return length_;
}

uint64_t Buffer::gpuAddress(size_t /*offset*/) const {
  return 0;
}

BufferDesc::BufferType Buffer::getBufferType() const {
  return type_;
}

} // namespace igl::webgpu
