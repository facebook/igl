/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/egl/Context.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <igl/opengl/ComputeCommandAdapter.h>
#include <igl/opengl/HWDevice.h>
#include <igl/opengl/RenderCommandAdapter.h>

#include <cassert>
#include <igl/Macros.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/Texture.h>
#include <tuple>

#define CHECK_EGL_ERRORS() error_checking::checkForEGLErrors(__FILE__, __FUNCTION__, __LINE__)

namespace error_checking {

#define CASE_ERROR_CODE_IMPL(egl_error_code) \
  case egl_error_code:                       \
    errorStr = #egl_error_code;              \
    break;

EGLint checkForEGLErrors(IGL_MAYBE_UNUSED const char* fileName,
                         IGL_MAYBE_UNUSED const char* callerName,
                         IGL_MAYBE_UNUSED size_t lineNum) {
  const EGLint errorCode = eglGetError();
  if (errorCode != EGL_SUCCESS) {
    IGL_MAYBE_UNUSED const char* errorStr;
    switch (errorCode) {
      // https://www.khronos.org/files/egl-1-4-quick-reference-card.pdf
      CASE_ERROR_CODE_IMPL(EGL_NOT_INITIALIZED);
      CASE_ERROR_CODE_IMPL(EGL_BAD_ACCESS);
      CASE_ERROR_CODE_IMPL(EGL_BAD_ALLOC);
      CASE_ERROR_CODE_IMPL(EGL_BAD_ATTRIBUTE);
      CASE_ERROR_CODE_IMPL(EGL_BAD_CONFIG);
      CASE_ERROR_CODE_IMPL(EGL_BAD_CONTEXT);
      CASE_ERROR_CODE_IMPL(EGL_BAD_CURRENT_SURFACE);
      CASE_ERROR_CODE_IMPL(EGL_BAD_DISPLAY);
      CASE_ERROR_CODE_IMPL(EGL_BAD_MATCH);
      CASE_ERROR_CODE_IMPL(EGL_BAD_NATIVE_PIXMAP);
      CASE_ERROR_CODE_IMPL(EGL_BAD_NATIVE_WINDOW);
      CASE_ERROR_CODE_IMPL(EGL_BAD_PARAMETER);
      CASE_ERROR_CODE_IMPL(EGL_BAD_SURFACE);
      CASE_ERROR_CODE_IMPL(EGL_CONTEXT_LOST);
    default:
      errorStr = "<unknown EGL error>";
      break;
    }
    IGL_ASSERT_MSG(false,
                   "[IGL] EGL error [%s:%zu] in function: %s 0x%04X: %s\n",
                   fileName,
                   lineNum,
                   callerName,
                   errorCode,
                   errorStr);
  }
  return errorCode;
}

} // namespace error_checking

FOLLY_PUSH_WARNING
FOLLY_GNU_DISABLE_WARNING("-Wzero-as-null-pointer-constant")

namespace igl::opengl::egl {

namespace {
EGLDisplay getDefaultEGLDisplay() {
  auto* display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  CHECK_EGL_ERRORS();
  return display;
}

// typical high-quality attrib list
EGLint attribs[] = {
    // 32 bit color
    EGL_RED_SIZE,
    8,
    EGL_GREEN_SIZE,
    8,
    EGL_BLUE_SIZE,
    8,
    EGL_ALPHA_SIZE,
    8,
    // 16-bit depth
    EGL_DEPTH_SIZE,
    16,
    EGL_SURFACE_TYPE,
    EGL_PBUFFER_BIT,
    // want opengl-es 2.x conformant CONTEXT
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_ES2_BIT,
    EGL_NONE // Terminator
};
EGLint contextAttribs[] = {
    EGL_CONTEXT_CLIENT_VERSION,
    2,
    EGL_NONE // Terminator
};

std::pair<EGLDisplay, EGLContext> newEGLContext(EGLDisplay display,
                                                EGLContext shareContext,
                                                EGLConfig* config) {
  if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
    CHECK_EGL_ERRORS();
    // TODO: Handle error
    return std::make_pair(EGL_NO_DISPLAY, EGL_NO_CONTEXT);
  }

  if (!config) {
    IGL_ASSERT_MSG(0, "config is nullptr");
    return std::make_pair(EGL_NO_DISPLAY, EGL_NO_CONTEXT);
  }

  EGLint numConfigs;
  if (!eglChooseConfig(display, attribs, config, 1, &numConfigs)) {
    CHECK_EGL_ERRORS();
  }

  auto res =
      std::make_pair(display, eglCreateContext(display, *config, shareContext, contextAttribs));
  CHECK_EGL_ERRORS();
  return res;
}

EGLConfig chooseConfig(EGLDisplay display) {
  EGLConfig config{nullptr};
  EGLint numConfigs{0};
  const EGLBoolean status = eglChooseConfig(display, attribs, &config, 1, &numConfigs);
  CHECK_EGL_ERRORS();
  if (!status) {
    IGL_ASSERT_MSG(status == EGL_TRUE, "eglChooseConfig failed");
  }
  return config;
}

} // namespace

///--------------------------------------
/// MARK: - Context

/*static*/ std::unique_ptr<Context> Context::createShareContext(Context& existingContext,
                                                                EGLContext newContext,
                                                                EGLSurface readSurface,
                                                                EGLSurface drawSurface,
                                                                Result* outResult) {
  if (newContext == EGL_NO_CONTEXT || readSurface == EGL_NO_SURFACE ||
      drawSurface == EGL_NO_SURFACE) {
    Result::setResult(outResult, Result(Result::Code::ArgumentInvalid));
    return nullptr;
  }

  auto sharegroup = existingContext.sharegroup_;
  if (!sharegroup) {
    sharegroup = std::make_shared<std::vector<EGLContext>>();
    sharegroup->reserve(2);
    sharegroup->emplace_back(existingContext.context_);

    existingContext.sharegroup_ = sharegroup;
  }
  sharegroup->emplace_back(newContext);

  auto context = std::make_unique<Context>(
      existingContext.display_, newContext, readSurface, drawSurface, existingContext.getConfig());
  context->sharegroup_ = std::move(sharegroup);

  Result::setOk(outResult);
  return context;
}

Context::Context(RenderingAPI api, EGLNativeWindowType window) :
  Context(api, EGL_NO_CONTEXT, nullptr, false, window, {0, 0}) {}

Context::Context(RenderingAPI api, size_t width, size_t height) :
  Context(api,
          EGL_NO_CONTEXT,
          nullptr,
          true,
          IGL_EGL_NULL_WINDOW,
          {static_cast<EGLint>(width), static_cast<EGLint>(height)}) {}

Context::Context(const Context& sharedContext) :
  Context(sharedContext.api_,
          sharedContext.context_,
          sharedContext.sharegroup_,
          true,
          IGL_EGL_NULL_WINDOW,
          sharedContext.getDrawSurfaceDimensions(nullptr)) {}

Context::Context(RenderingAPI api,
                 EGLContext shareContext,
                 std::shared_ptr<std::vector<EGLContext>> sharegroup,
                 bool offscreen,
                 EGLNativeWindowType window,
                 std::pair<EGLint, EGLint> dimensions) {
  EGLConfig config{nullptr};
  auto contextDisplay = newEGLContext(getDefaultEGLDisplay(), shareContext, &config);
  IGL_ASSERT_MSG(contextDisplay.second != EGL_NO_CONTEXT, "newEGLContext failed");

  contextOwned_ = true;
  api_ = api;
  display_ = contextDisplay.first;
  context_ = contextDisplay.second;
  IContext::registerContext((void*)context_, this);
  if (!window) {
    if (offscreen) {
      EGLint pbufferAttribs[] = {
          EGL_WIDTH,
          dimensions.first,
          EGL_HEIGHT,
          dimensions.second,
          EGL_NONE, // Terminator. I'll be back!
      };
      readSurface_ = drawSurface_ = eglCreatePbufferSurface(display_, config, pbufferAttribs);
      CHECK_EGL_ERRORS();
    } else {
      readSurface_ = eglGetCurrentSurface(EGL_READ);
      CHECK_EGL_ERRORS();
      drawSurface_ = eglGetCurrentSurface(EGL_DRAW);
      CHECK_EGL_ERRORS();
    }
  } else {
    surface_ = eglCreateWindowSurface(display_, config, window, nullptr);
    CHECK_EGL_ERRORS();
    readSurface_ = surface_;
    drawSurface_ = surface_;
  }
  config_ = config;

  if (sharegroup != nullptr) {
    sharegroup_ = std::move(sharegroup);
  } else {
    sharegroup_ = std::make_shared<std::vector<EGLContext>>();
  }
  sharegroup_->push_back(context_);

  initialize();
}

std::unique_ptr<IContext> Context::createShareContext(Result* /*outResult*/) {
  return std::make_unique<Context>(*this);
}

Context::Context(EGLDisplay display,
                 EGLContext context,
                 EGLSurface readSurface,
                 EGLSurface drawSurface,
                 EGLConfig config) :
  display_(display),
  context_(context),
  readSurface_(readSurface),
  drawSurface_(drawSurface),
  config_(config) {
  IContext::registerContext((void*)context_, this);
  initialize();
}

void Context::updateSurface(NativeWindowType window) {
  surface_ = eglCreateWindowSurface(display_, chooseConfig(display_), window, nullptr);
  CHECK_EGL_ERRORS();
  readSurface_ = surface_;
  drawSurface_ = surface_;
}

Context::~Context() {
  willDestroy((void*)context_);
  IContext::unregisterContext((void*)context_);
  if (contextOwned_ && context_ != EGL_NO_CONTEXT) {
    if (surface_ != nullptr) {
      eglDestroySurface(display_, surface_);
      CHECK_EGL_ERRORS();
    }

    eglDestroyContext(display_, context_);
    CHECK_EGL_ERRORS();
  }
}

void Context::setCurrent() {
  eglMakeCurrent(display_, drawSurface_, readSurface_, context_);
  CHECK_EGL_ERRORS();
  flushDeletionQueue();
}

void Context::clearCurrentContext() const {
  eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  CHECK_EGL_ERRORS();
}

bool Context::isCurrentContext() const {
  auto* curContext = eglGetCurrentContext();
  return curContext == context_;
  CHECK_EGL_ERRORS();
}

bool Context::isCurrentSharegroup() const {
  // EGL doesn't seem to provide a way to check if two contexts are in the same group.
  // For now we can at least check some trivial cases before hitting the assertion below.
  EGLContext currentContext = eglGetCurrentContext();
  CHECK_EGL_ERRORS();
  if (currentContext == context_) {
    return true;
  }
  if (currentContext == EGL_NO_CONTEXT) {
    return false;
  }
  if (sharegroup_) {
    const auto& sharegroup = *sharegroup_;
    auto it = std::find(sharegroup.begin(), sharegroup.end(), currentContext);
    return it != sharegroup.end();
  }
  return false;
}

void Context::present(std::shared_ptr<ITexture> /*surface*/) const {
#if defined(FORCE_USE_ANGLE)
  // Enforce swapbuffers for Angle to be able to use GPU tracing in RenderDoc
#if IGL_DEBUG
  eglSwapBuffers(display_, drawSurface_);
  CHECK_EGL_ERRORS();
#endif
  eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  CHECK_EGL_ERRORS();
#else
  if (drawSurface_ != EGL_NO_SURFACE) {
    eglSwapBuffers(display_, drawSurface_);
    // CHECK_EGL_ERRORS();
  }
#endif
}

void Context::setPresentationTime(long long presentationTimeNs) {
  // This is a workaround that we cannot call the eglPresentationTimeANDROID directly from
  // <EGL/eglext.h> due to some EGL api bugs.
  // @fb-only
  bool (*eglPresentationTimeANDROID_)(
      EGLDisplay dpy, EGLSurface sur, khronos_stime_nanoseconds_t time) = nullptr;
  eglPresentationTimeANDROID_ =
      reinterpret_cast<bool (*)(EGLDisplay, EGLSurface, khronos_stime_nanoseconds_t)>(
          eglGetProcAddress("eglPresentationTimeANDROID"));
  CHECK_EGL_ERRORS();
  eglPresentationTimeANDROID_(display_, surface_, presentationTimeNs);
  CHECK_EGL_ERRORS();
}

void Context::updateSurfaces(EGLSurface readSurface, EGLSurface drawSurface) {
  readSurface_ = readSurface;
  drawSurface_ = drawSurface;
  // We need this here because we need to call eglSetCurrent() with the new surface(s) in order to
  // bind them, but it's not the ideal place for it. Outside code could come in and make a different
  // context current at any time.
  setCurrent();
}

EGLSurface Context::createSurface(NativeWindowType window) {
  auto* surface = eglCreateWindowSurface(display_, chooseConfig(display_), window, nullptr);
  CHECK_EGL_ERRORS();
  return surface;
}

EGLContext Context::get() const {
  return context_;
}

EGLDisplay Context::getDisplay() const {
  return display_;
}

EGLSurface Context::getReadSurface() const {
  return readSurface_;
}

EGLSurface Context::getDrawSurface() const {
  return drawSurface_;
}

std::pair<EGLint, EGLint> Context::getDrawSurfaceDimensions(Result* outResult) const {
  EGLint height = -1;
  eglQuerySurface(getDisplay(), getDrawSurface(), EGL_HEIGHT, &height);
  if (CHECK_EGL_ERRORS() != EGL_SUCCESS) {
    Result::setResult(
        outResult, Result::Code::InvalidOperation, "Error getting height of EGLSurface.");
  }
  EGLint width = -1;
  eglQuerySurface(getDisplay(), getDrawSurface(), EGL_WIDTH, &width);
  if (CHECK_EGL_ERRORS() != EGL_SUCCESS) {
    Result::setResult(
        outResult, Result::Code::InvalidOperation, "Error getting width of EGLSurface.");
  }
  return std::make_pair(width, height);
}

EGLConfig Context::getConfig() const {
  return config_;
}

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
EGLImageKHR Context::createImageFromAndroidHardwareBuffer(AHardwareBuffer* hwb) const {
  EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(hwb);
  EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE, EGL_NONE};

  EGLDisplay display = this->getDisplay();
  // eglCreateImageKHR will add a ref to the AHardwareBuffer
  EGLImageKHR eglImage =
      eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribs);
  IGL_DEBUG_LOG("eglCreateImageKHR(%p, %x, %x, %p, {%d, %d, %d, %d, %d})\n",
                display,
                EGL_NO_CONTEXT,
                EGL_NATIVE_BUFFER_ANDROID,
                clientBuffer,
                attribs[0],
                attribs[1],
                attribs[2],
                attribs[3],
                attribs[4]);

  this->checkForErrors(__FUNCTION__, __LINE__);

  IGL_REPORT_ERROR(this->isCurrentContext() || this->isCurrentSharegroup());

  return eglImage;
}

void Context::imageTargetTexture(EGLImageKHR eglImage, GLenum target) const {
  glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(eglImage));
  IGL_DEBUG_LOG("glEGLImageTargetTexture2DOES(%u, %#x)\n",
                GL_TEXTURE_2D,
                static_cast<GLeglImageOES>(eglImage));
  this->checkForErrors(__FUNCTION__, __LINE__);
}
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

} // namespace igl::opengl::egl
FOLLY_POP_WARNING
