/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only
// @fb-only

#include <igl/opengl/egl/android/NativeHWBuffer.h>

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

#include <igl/opengl/Config.h>
#include <igl/opengl/egl/Context.h>

#if IGL_API_LOG && IGL_LOGGING_ENABLED
#define APILOG_DEC_DRAW_COUNT() \
  if (apiLogDrawsLeft_) {       \
    apiLogDrawsLeft_--;         \
  }
#define APILOG(format, ...)                 \
  if (apiLogDrawsLeft_ || apiLogEnabled_) { \
    IGL_LOG_DEBUG(format, ##__VA_ARGS__);   \
  }
#else
#define APILOG_DEC_DRAW_COUNT() static_cast<void>(0)
#define APILOG(format, ...) static_cast<void>(0)
#endif // IGL_API_LOGs && IGL_LOGGING_ENABLED

namespace igl::opengl::egl::android {

struct AHardwareBufferContext {
  EGLDisplay display;
  EGLImageKHR elgImage;
};

NativeHWTextureBuffer::~NativeHWTextureBuffer() {
  GLuint textureId = getId();
  if (textureId != 0) {
    if (getContext().isLikelyValidObject()) {
      getContext().deleteTextures(1, &textureId);
    }
  }

  auto* context = static_cast<AHardwareBufferContext*>(hwBufferHelper_.get());
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

Result NativeHWTextureBuffer::createTextureInternal(AHardwareBuffer* buffer) {
  AHardwareBuffer_Desc hwbDesc;
  AHardwareBuffer_describe(buffer, &hwbDesc);

  auto desc = TextureDesc::newNativeHWBufferImage(igl::android::getIglFormat(hwbDesc.format),
                                                  igl::android::getIglBufferUsage(hwbDesc.usage),
                                                  hwbDesc.width,
                                                  hwbDesc.height);
  auto result = Super::create(desc, false);
  if (!result.isOk()) {
    return result;
  }

  EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(buffer);
  EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE, EGL_NONE};

  EGLDisplay display = static_cast<Context*>(&getContext())->getDisplay();
  // eglCreateImageKHR will add a ref to the AHardwareBuffer
  EGLImageKHR eglImage =
      eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribs);
  APILOG("eglCreateImageKHR(%p, %x, %x, %p, {%d, %d, %d, %d, %d})\n",
         display,
         EGL_NO_CONTEXT,
         EGL_NATIVE_BUFFER_ANDROID,
         clientBuffer,
         attribs[0],
         attribs[1],
         attribs[2],
         attribs[3],
         attribs[4]);

  if (EGL_NO_IMAGE_KHR == eglImage) {
    return Result{Result::Code::RuntimeError, "Could not create EGL image, err"};
  }
  getContext().checkForErrors(__FUNCTION__, __LINE__);

  IGL_SOFT_ASSERT(getContext().isCurrentContext() || getContext().isCurrentSharegroup());

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
    GLuint textureId = getId();
    getContext().deleteTextures(1, &textureId);
    eglDestroyImageKHR(display, eglImage);
    return Result{Result::Code::RuntimeError, "NativeHWTextureBuffer GL error during bindTexture"};
  }

  glEGLImageTargetTexture2DOES(getTarget(), static_cast<GLeglImageOES>(eglImage));
  APILOG("glEGLImageTargetTexture2DOES(%u, %#x)\n",
         GL_TEXTURE_2D,
         static_cast<GLeglImageOES>(eglImage));

  getContext().checkForErrors(__FUNCTION__, __LINE__);

  std::shared_ptr<AHardwareBufferContext> hwBufferCtx = std::make_shared<AHardwareBufferContext>();
  hwBufferCtx->display = display;
  hwBufferCtx->elgImage = eglImage;
  hwBufferHelper_ = hwBufferCtx; // Implicit upcast

  textureDesc_ = desc;

  return Result{};
}

void NativeHWTextureBuffer::bind() {
  getContext().bindTexture(getTarget(), getId());
  auto* context = static_cast<AHardwareBufferContext*>(hwBufferHelper_.get());

  getContext().checkForErrors(__FUNCTION__, __LINE__);

  glEGLImageTargetTexture2DOES(getTarget(), context->elgImage);
  APILOG("glEGLImageTargetTexture2DOES(%u, %#x)\n",
         getTarget(),
         static_cast<GLeglImageOES>(context->elgImage));

  getContext().checkForErrors(__FUNCTION__, __LINE__);
}

void NativeHWTextureBuffer::bindImage(size_t unit) {
  IGL_DEBUG_ABORT("bindImage not Native Hardware Buffer Textures.");
}

// upload data into the given mip level
// a sub-rect of the texture may be specified to only upload the sub-rect
Result NativeHWTextureBuffer::uploadInternal(TextureType /*type*/,
                                             const TextureRangeDesc& range,
                                             const void* IGL_NULLABLE data,
                                             size_t bytesPerRow,
                                             const uint32_t* IGL_NULLABLE /*mipLevelBytes*/) const {
  auto result = uploadToHWBuffer(getProperties(), range, data, bytesPerRow);
  if (!result.isOk()) {
    IGL_DEBUG_ABORT("Cannot upload buffer for HW texture for Native Hardware Buffer Textures.");
    // Preserve the original error contract: callers historically observe
    // Result::Code::Unsupported on any upload failure here.
    return Result{Result::Code::Unsupported, "NativeHWTextureBuffer upload not supported"};
  }
  return result;
}

bool NativeHWTextureBuffer::isValidFormat(TextureFormat format) {
  return igl::android::getNativeHWFormat(format) > 0;
}

} // namespace igl::opengl::egl::android

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
