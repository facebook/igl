/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Readback.h>

#include <cstring>
#include <functional>
#include <string>
#include <igl/webgpu/Common.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

constexpr uint64_t kBytesPerRowAlignment = 256;

uint64_t alignUp(uint64_t value, uint64_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

struct MapState {
  bool completed = false;
  WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
  std::string message;
};

// Encodes the copy into `staging` with `encode`, submits it, maps `staging` and hands the mapped
// bytes to `consume`.
Result copyAndMap(const WebGPUContext& ctx,
                  uint64_t stagingSize,
                  const std::function<void(WGPUCommandEncoder, WGPUBuffer)>& encode,
                  const std::function<void(const uint8_t*)>& consume) {
  ctx.pushErrorScope(WGPUErrorFilter_Validation);

  const WGPUBufferDescriptor stagingDesc = {
      .nextInChain = nullptr,
      .label = toWGPUStringView("igl.webgpu.readback"),
      .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
      .size = stagingSize,
      .mappedAtCreation = 0,
  };
  const Handle<WGPUBuffer> staging(wgpuDeviceCreateBuffer(ctx.getDevice(), &stagingDesc));

  const Handle<WGPUCommandEncoder> encoder(
      wgpuDeviceCreateCommandEncoder(ctx.getDevice(), nullptr));
  encode(encoder.get(), staging.get());
  const Handle<WGPUCommandBuffer> commands(wgpuCommandEncoderFinish(encoder.get(), nullptr));
  const WGPUCommandBuffer rawCommands = commands.get();
  wgpuQueueSubmit(ctx.getQueue(), 1, &rawCommands);

  Result result = ctx.popErrorScope();
  if (!result.isOk()) {
    return result;
  }

  MapState state;
  const WGPUBufferMapCallbackInfo callbackInfo = {
      .nextInChain = nullptr,
      .mode = WGPUCallbackMode_WaitAnyOnly,
      .callback =
          [](WGPUMapAsyncStatus status,
             WGPUStringView message,
             void* IGL_NULLABLE userdata1,
             void* IGL_NULLABLE /*userdata2*/) {
            auto* s = static_cast<MapState*>(userdata1);
            if (s == nullptr) {
              return;
            }
            s->completed = true;
            s->status = status;
            s->message = toStdString(message);
          },
      .userdata1 = &state,
      .userdata2 = nullptr,
  };
  const WGPUFuture future =
      wgpuBufferMapAsync(staging.get(), WGPUMapMode_Read, 0, stagingSize, callbackInfo);
  if (!ctx.waitFuture(future) || !state.completed) {
    return Result(Result::Code::RuntimeError, "Timed out waiting for a WebGPU readback");
  }
  if (state.status != WGPUMapAsyncStatus_Success) {
    return Result(Result::Code::RuntimeError, "wgpuBufferMapAsync() failed: " + state.message);
  }

  const auto* mapped =
      static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(staging.get(), 0, stagingSize));
  if (mapped == nullptr) {
    wgpuBufferUnmap(staging.get());
    return Result(Result::Code::RuntimeError, "wgpuBufferGetConstMappedRange() failed");
  }
  consume(mapped);
  wgpuBufferUnmap(staging.get());
  wgpuBufferDestroy(staging.get());
  return Result();
}

} // namespace

Result readTexture(const WebGPUContext& ctx,
                   const TextureReadbackDesc& desc,
                   void* IGL_NONNULL dst) {
  if (desc.texture == nullptr || desc.bytesPerTexel == 0) {
    return Result(Result::Code::ArgumentInvalid, "Invalid texture readback");
  }
  if (desc.width == 0 || desc.height == 0) {
    return Result();
  }
  const uint64_t tightBytesPerRow = static_cast<uint64_t>(desc.width) * desc.bytesPerTexel;
  const uint64_t paddedBytesPerRow = alignUp(tightBytesPerRow, kBytesPerRowAlignment);
  const size_t dstBytesPerRow = desc.dstBytesPerRow != 0 ? desc.dstBytesPerRow
                                                         : static_cast<size_t>(tightBytesPerRow);
  if (dstBytesPerRow < tightBytesPerRow) {
    return Result(Result::Code::ArgumentOutOfRange, "Destination rows are too short");
  }

  const auto encode = [&desc, paddedBytesPerRow](WGPUCommandEncoder encoder, WGPUBuffer staging) {
    const WGPUTexelCopyTextureInfo source = {
        .texture = desc.texture,
        .mipLevel = desc.mipLevel,
        .origin = {.x = desc.x, .y = desc.y, .z = desc.layer},
        .aspect = desc.aspect,
    };
    const WGPUTexelCopyBufferInfo destination = {
        .layout = {.offset = 0,
                   .bytesPerRow = static_cast<uint32_t>(paddedBytesPerRow),
                   .rowsPerImage = desc.height},
        .buffer = staging,
    };
    const WGPUExtent3D extent = {
        .width = desc.width, .height = desc.height, .depthOrArrayLayers = 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &source, &destination, &extent);
  };
  const auto consume =
      [&desc, dst, paddedBytesPerRow, tightBytesPerRow, dstBytesPerRow](const uint8_t* mapped) {
        auto* out = static_cast<uint8_t*>(dst);
        for (uint32_t row = 0; row < desc.height; ++row) {
          const uint32_t srcRow = desc.flipVertically ? desc.height - 1 - row : row;
          std::memcpy(out + row * dstBytesPerRow,
                      mapped + srcRow * paddedBytesPerRow,
                      static_cast<size_t>(tightBytesPerRow));
        }
      };
  return copyAndMap(ctx, paddedBytesPerRow * desc.height, encode, consume);
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
    std::memcpy(dst, mapped, static_cast<size_t>(size));
  };
  return copyAndMap(ctx, size, encode, consume);
}

} // namespace igl::webgpu
