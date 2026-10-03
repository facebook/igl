/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Readback.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

struct ReadbackMapState {
  bool completed = false;
  WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
  std::string message;
};

namespace {

constexpr uint64_t kBytesPerRowAlignment = 256;

uint64_t alignUp(uint64_t value, uint64_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

// Creates `outStaging`, encodes the copy into it with `encode` and submits it.
Result submitCopy(const WebGPUContext& ctx,
                  uint64_t stagingSize,
                  const std::function<void(WGPUCommandEncoder, WGPUBuffer)>& encode,
                  Handle<WGPUBuffer>& outStaging) {
  ctx.pushErrorScope(WGPUErrorFilter_Validation);

  const WGPUBufferDescriptor stagingDesc = {
      .nextInChain = nullptr,
      .label = toWGPUStringView("igl.webgpu.readback"),
      .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
      .size = stagingSize,
      .mappedAtCreation = 0,
  };
  outStaging.reset(wgpuDeviceCreateBuffer(ctx.getDevice(), &stagingDesc));

  const Handle<WGPUCommandEncoder> encoder(
      wgpuDeviceCreateCommandEncoder(ctx.getDevice(), nullptr));
  encode(encoder.get(), outStaging.get());
  const Handle<WGPUCommandBuffer> commands(wgpuCommandEncoderFinish(encoder.get(), nullptr));
  const WGPUCommandBuffer rawCommands = commands.get();
  wgpuQueueSubmit(ctx.getQueue(), 1, &rawCommands);

  return ctx.popErrorScope();
}

// The callback owns a reference to `state`, so it may run after the requester is gone.
WGPUFuture requestMap(WGPUBuffer staging,
                      uint64_t size,
                      WGPUCallbackMode mode,
                      const std::shared_ptr<ReadbackMapState>& state) {
  const WGPUBufferMapCallbackInfo callbackInfo = {
      .nextInChain = nullptr,
      .mode = mode,
      .callback =
          [](WGPUMapAsyncStatus status,
             WGPUStringView message,
             void* IGL_NULLABLE userdata1,
             void* IGL_NULLABLE /*userdata2*/) {
            const std::unique_ptr<std::shared_ptr<ReadbackMapState>> s(
                static_cast<std::shared_ptr<ReadbackMapState>*>(userdata1));
            if (s == nullptr) {
              return;
            }
            (*s)->completed = true;
            (*s)->status = status;
            (*s)->message = toStdString(message);
          },
      .userdata1 = new std::shared_ptr<ReadbackMapState>(state),
      .userdata2 = nullptr,
  };
  return wgpuBufferMapAsync(staging, WGPUMapMode_Read, 0, size, callbackInfo);
}

Result getMapResult(const ReadbackMapState& state) {
  if (!state.completed) {
    return Result(Result::Code::RuntimeError, "Timed out waiting for a WebGPU readback");
  }
  if (state.status != WGPUMapAsyncStatus_Success) {
    return Result(Result::Code::RuntimeError, "wgpuBufferMapAsync() failed: " + state.message);
  }
  return Result();
}

// Hands the mapped bytes of `staging` to `consume` and releases its memory.
Result consumeMapped(WGPUBuffer staging,
                     uint64_t size,
                     const std::function<void(const uint8_t*)>& consume) {
  const auto* mapped = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(staging, 0, size));
  if (mapped == nullptr) {
    wgpuBufferUnmap(staging);
    return Result(Result::Code::RuntimeError, "wgpuBufferGetConstMappedRange() failed");
  }
  consume(mapped);
  wgpuBufferUnmap(staging);
  wgpuBufferDestroy(staging);
  return Result();
}

// Encodes the copy into a staging buffer with `encode`, submits it, maps the staging buffer and
// hands the mapped bytes to `consume`.
Result copyAndMap(const WebGPUContext& ctx,
                  uint64_t stagingSize,
                  const std::function<void(WGPUCommandEncoder, WGPUBuffer)>& encode,
                  const std::function<void(const uint8_t*)>& consume) {
  Result result = ctx.checkCanWait();
  if (!result.isOk()) {
    return result;
  }
  Handle<WGPUBuffer> staging;
  result = submitCopy(ctx, stagingSize, encode, staging);
  if (!result.isOk()) {
    return result;
  }
  const auto state = std::make_shared<ReadbackMapState>();
  const WGPUFuture future =
      requestMap(staging.get(), stagingSize, WGPUCallbackMode_WaitAnyOnly, state);
  if (!ctx.waitFuture(future)) {
    return Result(Result::Code::RuntimeError, "Timed out waiting for a WebGPU readback");
  }
  result = getMapResult(*state);
  if (!result.isOk()) {
    return result;
  }
  return consumeMapped(staging.get(), stagingSize, consume);
}

Result validate(const TextureReadbackDesc& desc) {
  if (desc.texture == nullptr || desc.bytesPerTexel == 0) {
    return Result(Result::Code::ArgumentInvalid, "Invalid texture readback");
  }
  const size_t tightBytesPerRow = size_t{desc.width} * desc.bytesPerTexel;
  if (desc.dstBytesPerRow != 0 && desc.dstBytesPerRow < tightBytesPerRow) {
    return Result(Result::Code::ArgumentOutOfRange, "Destination rows are too short");
  }
  return Result();
}

uint64_t getPaddedBytesPerRow(const TextureReadbackDesc& desc) {
  return alignUp(uint64_t{desc.width} * desc.bytesPerTexel, kBytesPerRowAlignment);
}

void encodeTextureCopy(const TextureReadbackDesc& desc,
                       WGPUCommandEncoder encoder,
                       WGPUBuffer staging) {
  const WGPUTexelCopyTextureInfo source = {
      .texture = desc.texture,
      .mipLevel = desc.mipLevel,
      .origin = {.x = desc.x, .y = desc.y, .z = desc.layer},
      .aspect = desc.aspect,
  };
  const WGPUTexelCopyBufferInfo destination = {
      .layout = {.offset = 0,
                 .bytesPerRow = static_cast<uint32_t>(getPaddedBytesPerRow(desc)),
                 .rowsPerImage = desc.height},
      .buffer = staging,
  };
  const WGPUExtent3D extent = {.width = desc.width, .height = desc.height, .depthOrArrayLayers = 1};
  wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &extent);
}

void copyRows(const TextureReadbackDesc& desc, const uint8_t* mapped, void* dst) {
  const uint64_t paddedBytesPerRow = getPaddedBytesPerRow(desc);
  const size_t tightBytesPerRow = size_t{desc.width} * desc.bytesPerTexel;
  const size_t dstBytesPerRow = desc.dstBytesPerRow != 0 ? desc.dstBytesPerRow : tightBytesPerRow;
  auto* out = static_cast<uint8_t*>(dst);
  for (uint32_t row = 0; row < desc.height; ++row) {
    const uint32_t srcRow = desc.flipVertically ? desc.height - 1 - row : row;
    std::copy_n(mapped + srcRow * paddedBytesPerRow, tightBytesPerRow, out + row * dstBytesPerRow);
  }
}

} // namespace

Result readTexture(const WebGPUContext& ctx,
                   const TextureReadbackDesc& desc,
                   void* IGL_NONNULL dst) {
  Result result = validate(desc);
  if (!result.isOk() || desc.width == 0 || desc.height == 0) {
    return result;
  }
  const auto encode = [&desc](WGPUCommandEncoder encoder, WGPUBuffer staging) {
    encodeTextureCopy(desc, encoder, staging);
  };
  const auto consume = [&desc, dst](const uint8_t* mapped) { copyRows(desc, mapped, dst); };
  return copyAndMap(ctx, getPaddedBytesPerRow(desc) * desc.height, encode, consume);
}

Result readBuffer(const WebGPUContext& ctx,
                  WGPUBuffer IGL_NULLABLE buffer,
                  uint64_t offset,
                  uint64_t size,
                  void* IGL_NONNULL dst) {
  if (buffer == nullptr) {
    return Result(Result::Code::ArgumentNull, "Buffer readback of a null buffer");
  }
  if (offset % 4 != 0 || size % 4 != 0) {
    return Result(Result::Code::ArgumentInvalid, "Buffer readback must be 4-byte aligned");
  }
  if (size == 0) {
    return Result();
  }
  const auto encode = [buffer, offset, size](WGPUCommandEncoder encoder, WGPUBuffer staging) {
    wgpuCommandEncoderCopyBufferToBuffer(encoder, buffer, offset, staging, 0, size);
  };
  const auto consume = [dst, size](const uint8_t* mapped) {
    std::copy_n(mapped, static_cast<size_t>(size), static_cast<uint8_t*>(dst));
  };
  return copyAndMap(ctx, size, encode, consume);
}

AsyncMap::~AsyncMap() = default;

Result AsyncMap::begin(const WebGPUContext& ctx,
                       uint64_t stagingSize,
                       const std::function<void(WGPUCommandEncoder, WGPUBuffer)>& encode) {
  reset();
  Handle<WGPUBuffer> staging;
  Result result = submitCopy(ctx, stagingSize, encode, staging);
  if (!result.isOk()) {
    return result;
  }
  ctx_ = &ctx;
  stagingSize_ = stagingSize;
  staging_ = std::move(staging);
  state_ = std::make_shared<ReadbackMapState>();
  future_ = requestMap(staging_.get(), stagingSize_, WGPUCallbackMode_AllowProcessEvents, state_);
  return Result();
}

bool AsyncMap::poll() {
  if (!state_) {
    return false;
  }
  if (!state_->completed) {
    ctx_->processEvents();
  }
  return state_->completed;
}

Result AsyncMap::wait() {
  if (!state_) {
    return Result(Result::Code::InvalidOperation, "No readback in progress");
  }
  if (!state_->completed) {
    Result result = ctx_->checkCanWait();
    if (!result.isOk()) {
      return result;
    }
  }
  if (!state_->completed && !ctx_->waitFuture(future_)) {
    return Result(Result::Code::RuntimeError, "Timed out waiting for a WebGPU readback");
  }
  return getMapResult(*state_);
}

Result AsyncMap::consume(const std::function<void(const uint8_t*)>& consume) {
  if (!state_) {
    return Result(Result::Code::InvalidOperation, "No readback in progress");
  }
  if (!state_->completed) {
    return Result(Result::Code::InvalidOperation, "The readback has not completed");
  }
  Result result = getMapResult(*state_);
  if (result.isOk()) {
    result = consumeMapped(staging_.get(), stagingSize_, consume);
  }
  reset();
  return result;
}

void AsyncMap::reset() {
  staging_ = nullptr;
  state_.reset();
  future_ = {};
  stagingSize_ = 0;
  ctx_ = nullptr;
}

Result AsyncTextureReadback::begin(const WebGPUContext& ctx, const TextureReadbackDesc& desc) {
  Result result = validate(desc);
  if (!result.isOk()) {
    return result;
  }
  if (desc.width == 0 || desc.height == 0) {
    return Result(Result::Code::ArgumentInvalid, "Empty texture readback");
  }
  result = map_.begin(ctx,
                      getPaddedBytesPerRow(desc) * desc.height,
                      [&desc](WGPUCommandEncoder encoder, WGPUBuffer staging) {
                        encodeTextureCopy(desc, encoder, staging);
                      });
  desc_ = desc;
  desc_.texture = nullptr;
  return result;
}

Result AsyncTextureReadback::copyTo(void* IGL_NONNULL dst) {
  return map_.consume([this, dst](const uint8_t* mapped) { copyRows(desc_, mapped, dst); });
}

Result AsyncTextureReadback::copyTo(void* IGL_NONNULL dst, size_t dstSize) {
  const size_t tightBytesPerRow = size_t{desc_.width} * desc_.bytesPerTexel;
  const size_t dstBytesPerRow = desc_.dstBytesPerRow != 0 ? desc_.dstBytesPerRow : tightBytesPerRow;
  const size_t required =
      desc_.height == 0 ? 0 : dstBytesPerRow * (desc_.height - 1) + tightBytesPerRow;
  if (dstSize < required) {
    (void)map_.consume([](const uint8_t* /*mapped*/) {});
    return Result(Result::Code::ArgumentOutOfRange, "The readback destination is too small");
  }
  return copyTo(dst);
}

Result AsyncBufferReadback::begin(const WebGPUContext& ctx,
                                  WGPUBuffer IGL_NULLABLE buffer,
                                  uint64_t offset,
                                  uint64_t size) {
  if (buffer == nullptr) {
    return Result(Result::Code::ArgumentNull, "Buffer readback of a null buffer");
  }
  if (offset % 4 != 0 || size % 4 != 0 || size == 0) {
    return Result(Result::Code::ArgumentInvalid,
                  "Buffer readbacks must be non-empty and 4-byte aligned");
  }
  size_ = size;
  return map_.begin(
      ctx, size, [buffer, offset, size](WGPUCommandEncoder encoder, WGPUBuffer staging) {
        wgpuCommandEncoderCopyBufferToBuffer(encoder, buffer, offset, staging, 0, size);
      });
}

Result AsyncBufferReadback::copyTo(void* IGL_NONNULL dst) {
  const auto size = static_cast<size_t>(size_);
  return map_.consume([dst, size](const uint8_t* mapped) {
    std::copy_n(mapped, size, static_cast<uint8_t*>(dst));
  });
}

} // namespace igl::webgpu
