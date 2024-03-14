/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "NativeHWBuffer.h"

#include "../Context.h"

#include <android/api-level.h>
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <igl/Macros.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/GLIncludes.h>

#if IGL_PLATFORM_ANDROID && __ANDROID_MIN_SDK_VERSION__ >= 26

#if defined(IGL_API_LOG) && (IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS))
#define APILOG_DEC_DRAW_COUNT() \
  if (apiLogDrawsLeft_) {       \
    apiLogDrawsLeft_--;         \
  }
#define APILOG(format, ...)                 \
  if (apiLogDrawsLeft_ || apiLogEnabled_) { \
    IGL_DEBUG_LOG(format, ##__VA_ARGS__);   \
  }
#else
#define APILOG_DEC_DRAW_COUNT() static_cast<void>(0)
#define APILOG(format, ...) static_cast<void>(0)
#endif // defined(IGL_API_LOG) && (IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS))

namespace igl::opengl::egl::android {

struct AHardwareBufferContext {
  EGLDisplay display;
  EGLImageKHR elgImage;
};

namespace {

uint32_t toNativeHWFormat(TextureFormat iglFomrat) {
  // note that Native HW buffer has compute specific format but is not added here.
  switch (iglFomrat) {
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

    // format mismatch
    //  case TextureFormat::S8_UInt_Z32_UNorm:
    //    return AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT;

  case TextureFormat::S_UInt8:
    return AHARDWAREBUFFER_FORMAT_S8_UINT;

  default:
    return 0;
  }
  return 0;
}

uint32_t getBufferUsage(TextureDesc::TextureUsage usage) {
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

} // namespace

NativeHWTextureBuffer::~NativeHWTextureBuffer() {
  GLuint textureID = getId();
  if (textureID != 0) {
    if (getContext().isLikelyValidObject()) {
      getContext().deleteTextures({textureID});
    }
  }

  auto context = std::static_pointer_cast<AHardwareBufferContext>(hwBufferHelper_);
  if (context) {
    eglDestroyImageKHR(context->display, context->elgImage);
    if (hwBuffer_) {
      AHardwareBuffer_release(hwBuffer_);
    }
    hwBuffer_ = nullptr;
  }
}

uint64_t NativeHWTextureBuffer::getTextureId() const {
  return getId();
}

bool NativeHWTextureBuffer::supportsUpload() const {
  return true;
}

Result NativeHWTextureBuffer::create(const TextureDesc& desc, bool hasStorageAlready) {
  return createHWBuffer(desc, hasStorageAlready, false);
}

Result NativeHWTextureBuffer::createHWBuffer(const TextureDesc& desc,
                                             bool hasStorageAlready,
                                             bool surfaceComposite) {
  if (getTextureId() != 0) {
    return Result{Result::Code::RuntimeError, "NativeHWTextureBuffer alreayd created"};
  }
  auto nativeHWFormat = toNativeHWFormat(desc.format);
  auto isValid = desc.numLayers == 1 && desc.numSamples == 1 && desc.numMipLevels == 1 &&
                 desc.type == TextureType::TwoD && nativeHWFormat > 0 &&
                 hasStorageAlready == false && desc.storage == ResourceStorage::Shared;
  auto result = Super::create(desc, false);

  if (result.isOk() && nativeHWFormat > 0 && isValid) {
    AHardwareBuffer_Desc descHW = {};
    descHW.format = nativeHWFormat;
    descHW.width = desc.width;
    descHW.height = desc.height;
    descHW.layers = 1;
    descHW.usage = getBufferUsage(desc.usage);
// USAGE_COMPOSER_OVERLAY API 33
#if __ANDROID_MIN_SDK_VERSION__ >= 33
    descHW.usage |= surfaceComposite ? AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY : 0;
#endif

    auto hwResult = AHardwareBuffer_allocate(&descHW, &hwBuffer_);
    if (hwResult != 0) {
      return Result{Result::Code::RuntimeError, "AHardwareBuffer allocation failed"};
    }

    EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(hwBuffer_);
    EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE, EGL_NONE};

    EGLDisplay display = ((egl::Context*)&getContext())->getDisplay();
    // eglCreateImageKHR will add a ref to the AHardwareBuffer
    EGLImageKHR eglImage = eglCreateImageKHR(
        display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribs);
    APILOG("eglCreateImageKHR()\n");

    if (EGL_NO_IMAGE_KHR == eglImage) {
      return Result{Result::Code::RuntimeError, "Could not create EGL image, err"};
    }
    getContext().checkForErrors(__FUNCTION__, __LINE__);

    IGL_REPORT_ERROR(getContext().isCurrentContext() || getContext().isCurrentSharegroup());

    GLuint tid = 0;
    getContext().genTextures(1, &tid);
    if (!tid) {
      eglDestroyImageKHR(display, eglImage);
      return Result{Result::Code::RuntimeError,
                    "NativeHWTextureBuffer failes to generate GL texture ID"};
    }

    setTextureBufferProperties(tid, GL_TEXTURE_2D);
    getContext().bindTexture(getTarget(), getId());

    if (getContext().checkForErrors(__FUNCTION__, __LINE__) != GL_NO_ERROR) {
      getContext().deleteTextures({getId()});
      eglDestroyImageKHR(display, eglImage);
      return Result{Result::Code::RuntimeError,
                    "NativeHWTextureBuffer GL error during bindTexture"};
    }

    glEGLImageTargetTexture2DOES(getTarget(), static_cast<GLeglImageOES>(eglImage));
    APILOG("glEGLImageTargetTexture2DOES(%u, %#x)\n",
           GL_TEXTURE_2D,
           static_cast<GLeglImageOES>(eglImage));

    getContext().checkForErrors(__FUNCTION__, __LINE__);

    std::shared_ptr<AHardwareBufferContext> hwBufferCtx =
        std::make_shared<AHardwareBufferContext>();
    hwBufferCtx->display = display;
    hwBufferCtx->elgImage = eglImage;
    hwBufferHelper_ = std::static_pointer_cast<AHardwareBufferHelper>(hwBufferCtx);
    return Result{};
  }
  return Result{Result::Code::RuntimeError,
                "Could not create hardware texture, texture desc is not valid"};
}

void NativeHWTextureBuffer::bind() {
  getContext().bindTexture(getTarget(), getId());
  auto context = std::static_pointer_cast<AHardwareBufferContext>(hwBufferHelper_);

  getContext().checkForErrors(__FUNCTION__, __LINE__);

  glEGLImageTargetTexture2DOES(getTarget(), context->elgImage);
  APILOG("glEGLImageTargetTexture2DOES(%u, %#x)\n",
         getTarget(),
         static_cast<GLeglImageOES>(context->elgImage));

  getContext().checkForErrors(__FUNCTION__, __LINE__);
}

void NativeHWTextureBuffer::bindImage(size_t unit) {
  IGL_ASSERT_MSG(0, "bindImage not Native Hardware Buffer Textuees.");
}

// upload data into the given mip level
// a sub-rect of the texture may be specified to only upload the sub-rect
Result NativeHWTextureBuffer::uploadInternal(TextureType /*type*/,
                                             const TextureRangeDesc& range,
                                             const void* IGL_NULLABLE data,
                                             size_t bytesPerRow) const {
  // not optimal pass

  std::byte* dst = nullptr;
  NativeHWTextureBuffer::RangeDesc outRange;
  auto result = lockHWBuffer(reinterpret_cast<std::byte**>(&dst), outRange);
  auto internalBpr = getProperties().getBytesPerRow(outRange.stride);

  if (result.isOk() && dst != nullptr && bytesPerRow <= internalBpr &&
      range.width == outRange.width && range.height == outRange.height) {
    const std::byte* src = (const std::byte*)data;
    uint32_t srcOffset = 0;
    uint32_t dstOffset = 0;
    for (int i = 0; i < outRange.height; ++i) {
      memcpy((void*)(dst + dstOffset), (void*)(src + srcOffset), bytesPerRow);
      dstOffset += internalBpr;
      srcOffset += bytesPerRow;
    }

    return Result{};
  }

  IGL_ASSERT_MSG(0, "Cannot upload buffer for HW texture for Native Hardware Buffer Textuees.");
  return Result{Result::Code::Unsupported, "NativeHWTextureBuffer upload not supported"};
}

Result NativeHWTextureBuffer::lockHWBuffer(std::byte* _Nullable* _Nonnull dst,
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

  return Result{};
}

Result NativeHWTextureBuffer::unlockHWBuffer() const {
  if (AHardwareBuffer_unlock(hwBuffer_, nullptr)) {
    IGL_ASSERT_MSG(0, "Failed to unlock hardware buffer");

    return Result{Result::Code::RuntimeError, "Failed to unlock hardware buffer"};
  }
  return Result{};
}

bool NativeHWTextureBuffer::isValidFormat(TextureFormat format) {
  return toNativeHWFormat(format) > 0;
}

Result NativeHWTextureBuffer::bindTextureWithHWBuffer(IContext& context,
                                                      GLuint target,
                                                      const AHardwareBuffer* hwb) noexcept {
  EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(hwb);
  EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE, EGL_NONE};

  EGLDisplay display = ((egl::Context*)&context)->getDisplay();
  // eglCreateImageKHR will add a ref to the AHardwareBuffer
  EGLImageKHR eglImage =
      eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribs);
  APILOG("eglCreateImageKHR()\n");

  if (EGL_NO_IMAGE_KHR == eglImage) {
    return Result{Result::Code::RuntimeError, "Could not create EGL image, err"};
  }
  context.checkForErrors(__FUNCTION__, __LINE__);

  IGL_REPORT_ERROR(context.isCurrentContext() || context.isCurrentSharegroup());

  glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(eglImage));
  APILOG("glEGLImageTargetTexture2DOES(%u, %#x)\n",
         GL_TEXTURE_2D,
         static_cast<GLeglImageOES>(eglImage));

  context.checkForErrors(__FUNCTION__, __LINE__);

  return Result{};
}

} // namespace igl::opengl::egl::android

#endif
