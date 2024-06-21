/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "NativeHWBuffer.h"

#if IGL_PLATFORM_ANDROID && __ANDROID_MIN_SDK_VERSION__ >= 26

#include <android/hardware_buffer.h>

namespace igl::android {

uint32_t getNativeHWFormat(TextureFormat iglFormat) {
  // note that Native HW buffer has compute specific format but is not added here.
  switch (iglFormat) {
  case TextureFormat::RGBX_UNorm8:
    return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;

  case TextureFormat::RGBA_UNorm8:
    return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;

  case TextureFormat::B5G6R5_UNorm:
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

  default:
    return 0;
  }
}

uint32_t getNativeHWBufferUsage(TextureDesc::TextureUsage usage) {
  uint64_t bufferUsage = 0;

  if (usage & TextureDesc::TextureUsageBits::Sampled) {
    bufferUsage |= AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
  }
  if (usage & TextureDesc::TextureUsageBits::Storage) {
    bufferUsage |= AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
  }
  if (usage & TextureDesc::TextureUsageBits::Attachment) {
    bufferUsage |= AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
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

// USAGE_COMPOSER_OVERLAY API 33
#if __ANDROID_MIN_SDK_VERSION__ >= 33
  bufferDesc.usage |= surfaceComposite ? AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY : 0;
#endif

  auto code = AHardwareBuffer_allocate(&bufferDesc, buffer);
  if (code != 0) {
    return Result{Result::Code::RuntimeError, "AHardwareBuffer allocation failed"};
  }

  return Result{Result::Code::Ok};
}

INativeHWTextureBuffer::~INativeHWTextureBuffer() {
  if (!isHwBufferExternal_) {
    AHardwareBuffer_release(hwBuffer_);
    hwBuffer_ = nullptr;
  }
}

Result INativeHWTextureBuffer::lockHWBuffer(std::byte* _Nullable* _Nonnull dst,
                                            RangeDesc& outRange) const {
  AHardwareBuffer_Desc hwbDesc;
  AHardwareBuffer_describe(hwBuffer_, &hwbDesc);

  if (AHardwareBuffer_lock(hwBuffer_,
                           AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                           -1,
                           nullptr,
                           reinterpret_cast<void**>(dst))) {
    IGL_ASSERT_MSG(0, "Failed to lock hardware buffer");
    return Result{Result::Code::RuntimeError, "Failed to lock hardware buffer"};
  }

  outRange.width = hwbDesc.width;
  outRange.height = hwbDesc.height;
  outRange.layer = 1;
  outRange.mipLevel = 1;
  outRange.stride = hwbDesc.stride;

  return Result{Result::Code::Ok};
}

Result INativeHWTextureBuffer::unlockHWBuffer() const {
  if (AHardwareBuffer_unlock(hwBuffer_, nullptr)) {
    IGL_ASSERT_MSG(0, "Failed to unlock hardware buffer");
    return Result{Result::Code::RuntimeError, "Failed to unlock hardware buffer"};
  }
  return Result{Result::Code::Ok};
}

} // namespace igl::android

#endif
