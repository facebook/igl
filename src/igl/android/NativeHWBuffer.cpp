/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only
// @fb-only

#include "NativeHWBuffer.h"

#include <igl/IGLSafeC.h>

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

// AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420 (0x23) was introduced in NDK r21 (API 30). Some build
// configs still compile against older NDK sysroots that do not declare it, so define a
// fallback when the header does not. It is a real NDK enumerator on newer sysroots, so this
// must come after <android/hardware_buffer.h> (included via NativeHWBuffer.h above) —
// defining it earlier would rewrite the enumerator token inside that header.
#if !defined(AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420)
// NOLINTNEXTLINE(readability-identifier-naming) - macro name must match the NDK enumerator
#define AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420 0x23
#endif

namespace igl::android {

uint32_t getNativeHWFormat(TextureFormat iglFormat) {
  // note that Native HW buffer has compute specific format but is not added here.
  switch (iglFormat) {
  case TextureFormat::RGBX_UNorm8:
    return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;

  case TextureFormat::RGBA_UNorm8:
    return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;

  case TextureFormat::R5G6B5_UNorm:
    return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;

  case TextureFormat::RGBA_F16:
    return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;

  case TextureFormat::RGB10_A2_UNorm_Rev:
    return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;

  case TextureFormat::Z_UNorm16:
    return AHARDWAREBUFFER_FORMAT_D16_UNORM;

  case TextureFormat::Z_UNorm24:
    return AHARDWAREBUFFER_FORMAT_D24_UNORM;

  case TextureFormat::Z_UNorm32:
    return AHARDWAREBUFFER_FORMAT_D32_FLOAT;

  case TextureFormat::S8_UInt_Z24_UNorm:
    return AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT;

  case TextureFormat::S_UInt8:
    return AHARDWAREBUFFER_FORMAT_S8_UINT;

  case TextureFormat::YUV_NV12:
    return AHARDWAREBUFFER_FORMAT_YCbCr_420_SP_VENUS;

  case TextureFormat::YUV_420p:
    return AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;

  default:
    return 0;
  }
}

uint32_t getNativeHWBufferUsage(TextureDesc::TextureUsage iglUsage) {
  uint64_t bufferUsage = 0;

  if (iglUsage & TextureDesc::TextureUsageBits::Sampled) {
    bufferUsage |= AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
  }
  if (iglUsage & TextureDesc::TextureUsageBits::Storage) {
    bufferUsage |= AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
  }
  if (iglUsage & TextureDesc::TextureUsageBits::Attachment) {
    bufferUsage |= AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
  }

  return bufferUsage;
}

TextureFormat getIglFormat(uint32_t nativeFormat) {
  // note that Native HW buffer has compute specific format but is not added here.
  switch (nativeFormat) {
  case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
    return TextureFormat::RGBX_UNorm8;

  case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
    return TextureFormat::RGBA_UNorm8;

  case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
    return TextureFormat::R5G6B5_UNorm;

  case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
    return TextureFormat::RGBA_F16;

  case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
    return TextureFormat::RGB10_A2_UNorm_Rev;

  case AHARDWAREBUFFER_FORMAT_D16_UNORM:
    return TextureFormat::Z_UNorm16;

  case AHARDWAREBUFFER_FORMAT_D24_UNORM:
    return TextureFormat::Z_UNorm24;

  case AHARDWAREBUFFER_FORMAT_D32_FLOAT:
    return TextureFormat::Z_UNorm32;

  case AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT:
    return TextureFormat::S8_UInt_Z24_UNorm;

  case AHARDWAREBUFFER_FORMAT_S8_UINT:
    return TextureFormat::S_UInt8;

  case AHARDWAREBUFFER_FORMAT_YCbCr_420_SP_VENUS:
  case COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m:
    return TextureFormat::YUV_NV12;

  case AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420:
    return TextureFormat::YUV_420p;

  default:
    return TextureFormat::Invalid;
  }
}

TextureDesc::TextureUsage getIglBufferUsage(uint32_t nativeUsage) {
  TextureDesc::TextureUsage bufferUsage = 0;

  if (nativeUsage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) {
    bufferUsage |= TextureDesc::TextureUsageBits::Sampled;
  }
  if (nativeUsage &
      (AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN)) {
    bufferUsage |= TextureDesc::TextureUsageBits::Storage;
  }
  if (nativeUsage & AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) {
    bufferUsage |= TextureDesc::TextureUsageBits::Attachment;
  }

  return bufferUsage;
}

Result allocateNativeHWBuffer(const TextureDesc& desc,
                              bool surfaceComposite,
                              AHardwareBuffer** buffer) {
  AHardwareBuffer_Desc bufferDesc = {};
  bufferDesc.format = getNativeHWFormat(desc.format);
  bufferDesc.width = desc.width;
  bufferDesc.height = desc.height;
  bufferDesc.layers = 1;
  bufferDesc.usage = getNativeHWBufferUsage(desc.usage);
  bufferDesc.rfu0 = 0;
  bufferDesc.rfu1 = 0;

#if __ANDROID_MIN_SDK_VERSION__ >= 33
  bufferDesc.usage |= surfaceComposite ? AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY : 0;
#endif // __ANDROID_MIN_SDK_VERSION__ >= 33

  const auto code = AHardwareBuffer_allocate(&bufferDesc, buffer);
  if (code != 0) {
    return Result(Result::Code::RuntimeError, "AHardwareBuffer allocation failed");
  }

  return Result();
}

INativeHWTextureBuffer::LockGuard::~LockGuard() {
  if (hwBufferOwner_ != nullptr) {
    hwBufferOwner_->unlockHWBuffer();
  }
}

INativeHWTextureBuffer::LockGuard::LockGuard(const INativeHWTextureBuffer* hwBufferOwner) :
  hwBufferOwner_(hwBufferOwner) {}

INativeHWTextureBuffer::LockGuard::LockGuard(INativeHWTextureBuffer::LockGuard&& g) noexcept {
  hwBufferOwner_ = g.hwBufferOwner_;
  g.hwBufferOwner_ = nullptr;
}

Result INativeHWTextureBuffer::createWithHWBuffer(AHardwareBuffer* buffer) {
  if (hwBuffer_) {
    return Result{Result::Code::InvalidOperation, "Hardware buffer already provided"};
  }

  AHardwareBuffer_acquire(buffer);

  // textureDesc_ should be updated through createTextureInternal function.
  Result result = createTextureInternal(buffer);
  if (!result.isOk()) {
    AHardwareBuffer_release(buffer);
  } else {
    hwBuffer_ = buffer;
  }

  return result;
}

Result INativeHWTextureBuffer::createHWBuffer(const TextureDesc& desc,
                                              bool hasStorageAlready,
                                              bool surfaceComposite) {
  if (hwBuffer_) {
    IGL_LOG_ERROR("hw already provided");
    return Result{Result::Code::InvalidOperation, "Hardware buffer already provided"};
  }

  const bool isValid = desc.numLayers == 1 && desc.numSamples == 1 && desc.numMipLevels == 1 &&
                       desc.usage != 0 && desc.type == TextureType::TwoD &&
                       desc.tiling == igl::TextureDesc::TextureTiling::Optimal &&
                       igl::android::getNativeHWFormat(desc.format) > 0 && !hasStorageAlready &&
                       desc.storage == ResourceStorage::Shared;
  if (!isValid) {
    IGL_LOG_ERROR("invalid desc for HW");
    // failed on (1 1 1) (5 1 0) (1 0 0)
    IGL_LOG_ERROR("DESC: (%d %d %d) (%d %d %d) (%d %d %d)",
                  desc.numLayers, // 1
                  desc.numSamples, // 1
                  desc.numMipLevels, // 1

                  desc.usage, // != 0
                  desc.type, // 1 = TwoD
                  desc.tiling, // 0 = Optimal

                  igl::android::getNativeHWFormat(desc.format),
                  static_cast<int>(hasStorageAlready),
                  desc.storage // 2 = shared
    );

    return Result(Result::Code::Unsupported, "Invalid texture description");
  }

  AHardwareBuffer* buffer = nullptr;
  auto allocationResult = igl::android::allocateNativeHWBuffer(desc, surfaceComposite, &buffer);
  if (!allocationResult.isOk()) {
    IGL_LOG_ERROR("HW alloc failed");
    return allocationResult;
  }

  Result result = createTextureInternal(buffer);
  if (!result.isOk()) {
    IGL_LOG_ERROR("HW internal failed");
    AHardwareBuffer_release(buffer);
  } else {
    hwBuffer_ = buffer;
    textureDesc_ = desc;
  }

  return result;
}

INativeHWTextureBuffer::LockGuard INativeHWTextureBuffer::lockHWBuffer(std::byte * IGL_NULLABLE *
                                                                           IGL_NONNULL dst,
                                                                       RangeDesc& outRange,
                                                                       Result* outResult) const {
  Result result = lockHWBuffer(dst, outRange);
  Result::setResult(outResult, result);
  return {result.isOk() ? this : nullptr};
}

Result INativeHWTextureBuffer::lockHWBuffer(std::byte * IGL_NULLABLE * IGL_NONNULL dst,
                                            RangeDesc& outRange) const {
  AHardwareBuffer_Desc hwbDesc;
  AHardwareBuffer_describe(hwBuffer_, &hwbDesc);

  if (AHardwareBuffer_lock(hwBuffer_,
                           AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                           -1,
                           nullptr,
                           reinterpret_cast<void**>(dst))) {
    IGL_DEBUG_ABORT("Failed to lock hardware buffer");
    return Result(Result::Code::RuntimeError, "Failed to lock hardware buffer");
  }

  outRange.width = hwbDesc.width;
  outRange.height = hwbDesc.height;
  outRange.layer = 1;
  outRange.mipLevel = 1;
  outRange.stride = hwbDesc.stride;

  return Result();
}

Result INativeHWTextureBuffer::unlockHWBuffer() const {
  if (AHardwareBuffer_unlock(hwBuffer_, nullptr)) {
    IGL_DEBUG_ABORT("Failed to unlock hardware buffer");
    return Result(Result::Code::RuntimeError, "Failed to unlock hardware buffer");
  }
  return Result();
}

AHardwareBuffer* INativeHWTextureBuffer::getHardwareBuffer() const {
  return hwBuffer_;
}

TextureDesc INativeHWTextureBuffer::getTextureDesc() const {
  return textureDesc_;
}

Result INativeHWTextureBuffer::uploadToHWBuffer(const TextureFormatProperties& props,
                                                const TextureRangeDesc& range,
                                                const void* IGL_NULLABLE data,
                                                size_t bytesPerRow) const {
  if (hwBuffer_ == nullptr) {
    return Result{Result::Code::RuntimeError,
                  "INativeHWTextureBuffer: hardware buffer not initialized"};
  }
  if (data == nullptr) {
    IGL_LOG_ERROR("INativeHWTextureBuffer: source data is null");
    return Result{Result::Code::ArgumentInvalid, "INativeHWTextureBuffer: source data is null"};
  }

  std::byte* dst = nullptr;
  RangeDesc outRange;
  Result lockResult;
  auto lockGuard
      [[maybe_unused]] = lockHWBuffer(reinterpret_cast<std::byte**>(&dst), outRange, &lockResult);

  if (!lockResult.isOk()) {
    IGL_LOG_ERROR("INativeHWTextureBuffer: failed to lock hardware buffer: %s",
                  lockResult.message.c_str());
    return Result{Result::Code::RuntimeError,
                  "INativeHWTextureBuffer: failed to lock hardware buffer"};
  }
  if (dst == nullptr) {
    IGL_LOG_ERROR("INativeHWTextureBuffer: locked hardware buffer returned a null pointer");
    return Result{Result::Code::RuntimeError,
                  "INativeHWTextureBuffer: locked hardware buffer returned a null pointer"};
  }

  const size_t internalBpr = props.getBytesPerRow(outRange.stride);
  const size_t srcBpr = bytesPerRow ? bytesPerRow : props.getBytesPerRow(range);

  if (srcBpr > internalBpr) {
    IGL_LOG_ERROR(
        "INativeHWTextureBuffer: source bytes-per-row (%zu) exceeds hardware buffer "
        "bytes-per-row (%zu)",
        srcBpr,
        internalBpr);
    return Result{Result::Code::ArgumentInvalid,
                  "INativeHWTextureBuffer: source bytes-per-row exceeds hardware buffer "
                  "bytes-per-row"};
  }
  if (range.width != outRange.width || range.height != outRange.height) {
    IGL_LOG_ERROR(
        "INativeHWTextureBuffer: upload range (%zux%zu) does not match hardware buffer "
        "range (%zux%zu)",
        range.width,
        range.height,
        outRange.width,
        outRange.height);
    return Result{Result::Code::ArgumentInvalid,
                  "INativeHWTextureBuffer: upload range does not match hardware buffer range"};
  }

  const std::byte* src = static_cast<const std::byte*>(data);
  size_t srcOffset = 0;
  size_t dstOffset = 0;
  for (uint32_t i = 0; i < outRange.height; ++i) {
    checked_memcpy(dst + dstOffset, internalBpr, src + srcOffset, srcBpr);
    dstOffset += internalBpr;
    srcOffset += srcBpr;
  }
  return Result{};
}

} // namespace igl::android

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
