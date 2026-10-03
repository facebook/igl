/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/TimestampQuerySet.h>

#include <cstring>
#include <utility>
#include <vector>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

struct TimestampQuerySet::Readback {
  Handle<WGPUBuffer> buffer;
  uint32_t count = 0;
  bool completed = false;
  std::vector<uint64_t> timestamps;
};

std::unique_ptr<TimestampQuerySet> TimestampQuerySet::create(WebGPUContext& ctx,
                                                             uint32_t count,
                                                             Result* IGL_NULLABLE outResult) {
  if (count == 0) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "A query set needs queries");
    return nullptr;
  }
  if (wgpuDeviceHasFeature(ctx.getDevice(), WGPUFeatureName_TimestampQuery) == 0) {
    Result::setResult(outResult, Result::Code::Unsupported, "The device has no timestamp queries");
    return nullptr;
  }
  WGPUQuerySetDescriptor desc = WGPU_QUERY_SET_DESCRIPTOR_INIT;
  desc.label = toWGPUStringView("igl.webgpu.timestamps");
  desc.type = WGPUQueryType_Timestamp;
  desc.count = count;
  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPUQuerySet> querySet(wgpuDeviceCreateQuerySet(ctx.getDevice(), &desc));
  Result result = ctx.popErrorScope();
  if (!result.isOk() || !querySet) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setOk(outResult);
  return std::unique_ptr<TimestampQuerySet>(new TimestampQuerySet(ctx, count, std::move(querySet)));
}

TimestampQuerySet::TimestampQuerySet(WebGPUContext& ctx,
                                     uint32_t count,
                                     Handle<WGPUQuerySet> querySet) :
  ctx_(ctx), count_(count), querySet_(std::move(querySet)) {
  const WGPUBufferDescriptor desc = {
      .nextInChain = nullptr,
      .label = toWGPUStringView("igl.webgpu.timestamps.resolve"),
      .usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc,
      .size = uint64_t{count} * sizeof(uint64_t),
      .mappedAtCreation = 0,
  };
  resolveBuffer_.reset(wgpuDeviceCreateBuffer(ctx_.getDevice(), &desc));
}

TimestampQuerySet::~TimestampQuerySet() = default;

void TimestampQuerySet::encodeResolve(WGPUCommandEncoder IGL_NONNULL encoder, uint32_t count) {
  if (count == 0 || count > count_) {
    return;
  }
  auto readback = std::make_shared<Readback>();
  readback->count = count;
  const uint64_t size = uint64_t{count} * sizeof(uint64_t);
  const WGPUBufferDescriptor desc = {
      .nextInChain = nullptr,
      .label = toWGPUStringView("igl.webgpu.timestamps.readback"),
      .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
      .size = size,
      .mappedAtCreation = 0,
  };
  readback->buffer.reset(wgpuDeviceCreateBuffer(ctx_.getDevice(), &desc));
  wgpuCommandEncoderResolveQuerySet(encoder, querySet_.get(), 0, count, resolveBuffer_.get(), 0);
  wgpuCommandEncoderCopyBufferToBuffer(
      encoder, resolveBuffer_.get(), 0, readback->buffer.get(), 0, size);
  encoded_ = std::move(readback);
}

void TimestampQuerySet::mapAfterSubmit() {
  if (!encoded_) {
    return;
  }
  mapped_ = std::move(encoded_);
  // The callback owns a reference, so it can complete after this object is gone.
  auto* owner = new std::shared_ptr<Readback>(mapped_);
  const WGPUBufferMapCallbackInfo callbackInfo = {
      .nextInChain = nullptr,
      .mode = WGPUCallbackMode_AllowProcessEvents,
      .callback =
          [](WGPUMapAsyncStatus status,
             WGPUStringView /*message*/,
             void* IGL_NULLABLE userdata1,
             void* IGL_NULLABLE /*userdata2*/) {
            const std::unique_ptr<std::shared_ptr<Readback>> owner(
                static_cast<std::shared_ptr<Readback>*>(userdata1));
            if (!owner) {
              return;
            }
            Readback& readback = **owner;
            if (status == WGPUMapAsyncStatus_Success) {
              const size_t size = size_t{readback.count} * sizeof(uint64_t);
              const void* data = wgpuBufferGetConstMappedRange(readback.buffer.get(), 0, size);
              if (data != nullptr) {
                readback.timestamps.resize(readback.count);
                std::memcpy(readback.timestamps.data(), data, size);
              } else {
                IGL_LOG_ERROR_ONCE("Timestamp readback: the mapped range is null\n");
              }
              wgpuBufferUnmap(readback.buffer.get());
            } else if (status == WGPUMapAsyncStatus_Error) {
              // Aborted and CallbackCancelled come from teardown and are not reported.
              IGL_LOG_ERROR_ONCE("Timestamp readback: mapping the buffer failed\n");
            }
            readback.buffer = nullptr;
            readback.completed = true;
          },
      .userdata1 = owner,
      .userdata2 = nullptr,
  };
  const size_t size = size_t{mapped_->count} * sizeof(uint64_t);
  (void)wgpuBufferMapAsync(mapped_->buffer.get(), WGPUMapMode_Read, 0, size, callbackInfo);
}

void TimestampQuerySet::reset() {
  encoded_ = nullptr;
  mapped_ = nullptr;
}

bool TimestampQuerySet::poll() const {
  if (!mapped_) {
    return false;
  }
  if (!mapped_->completed) {
    ctx_.processEvents();
  }
  return mapped_->completed;
}

std::span<const uint64_t> TimestampQuerySet::getTimestamps() const {
  if (!mapped_ || !mapped_->completed) {
    return {};
  }
  return mapped_->timestamps;
}

} // namespace igl::webgpu
