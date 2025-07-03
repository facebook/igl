/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/opengl/egl/Context.h>

#include <EGL/egl.h>
#include <igl/opengl/HWDevice.h>

#include <array>
#include <cassert>
#include <tuple>
#include <igl/Macros.h>
#include <igl/opengl/Texture.h>

#define CHECK_EGL_ERRORS() error_checking::checkForEGLErrors(__FILE__, __FUNCTION__, __LINE__)

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

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
    IGL_MAYBE_UNUSED const char* errorStr = nullptr;
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
    IGL_DEBUG_ABORT("[IGL] EGL error [%s:%zu] in function: %s 0x%04X: %s\n",
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
constexpr std::array attribsOpenGLES2{EGLint{EGL_RED_SIZE},
                                      EGLint{8},
                                      EGLint{EGL_GREEN_SIZE},
                                      EGLint{8},
                                      EGLint{EGL_BLUE_SIZE},
                                      EGLint{8},
                                      EGLint{EGL_ALPHA_SIZE},
                                      EGLint{8},
                                      EGLint{EGL_DEPTH_SIZE},
                                      EGLint{16},
                                      EGLint{EGL_SURFACE_TYPE},
                                      EGLint{EGL_PBUFFER_BIT},
                                      EGLint{EGL_RENDERABLE_TYPE},
                                      EGLint{EGL_OPENGL_ES2_BIT},
                                      EGLint{EGL_NONE}};
constexpr std::array contextAttribsOpenGLES2{EGLint{EGL_CONTEXT_CLIENT_VERSION},
                                             EGLint{2},
                                             EGLint{EGL_NONE}};

constexpr std::array attribsOpenGLES3{EGLint{EGL_RED_SIZE},
                                      EGLint{8},
                                      EGLint{EGL_GREEN_SIZE},
                                      EGLint{8},
                                      EGLint{EGL_BLUE_SIZE},
                                      EGLint{8},
                                      EGLint{EGL_ALPHA_SIZE},
                                      EGLint{8},
                                      EGLint{EGL_DEPTH_SIZE},
                                      EGLint{16},
                                      EGLint{EGL_SURFACE_TYPE},
                                      EGLint{EGL_PBUFFER_BIT},
                                      EGLint{EGL_RENDERABLE_TYPE},
                                      EGLint{EGL_OPENGL_ES3_BIT},
                                      EGLint{EGL_NONE}};
constexpr std::array contextAttribsOpenGLES3{EGLint{EGL_CONTEXT_CLIENT_VERSION},
                                             EGLint{3},
                                             EGLint{EGL_NONE}};

std::pair<EGLDisplay, EGLContext> newEGLContext(uint8_t contextMajorVersion,
                                                EGLDisplay display,
                                                EGLContext shareContext,
                                                EGLConfig* config) {
  IGL_DEBUG_ASSERT(contextMajorVersion == 2 || contextMajorVersion == 3);
  if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
    CHECK_EGL_ERRORS();
    // TODO: Handle error
    return std::make_pair(EGL_NO_DISPLAY, EGL_NO_CONTEXT);
  }

  if (!config) {
    IGL_DEBUG_ABORT("config is nullptr");
    return std::make_pair(EGL_NO_DISPLAY, EGL_NO_CONTEXT);
  }

  EGLint numConfigs = 0;
  const auto& attribs = contextMajorVersion == 2 ? attribsOpenGLES2 : attribsOpenGLES3;
  const auto& contextAttribs = contextMajorVersion == 2 ? contextAttribsOpenGLES2
                                                        : contextAttribsOpenGLES3;
  if (!eglChooseConfig(display, attribs.data(), config, 1, &numConfigs)) {
    CHECK_EGL_ERRORS();
  }

  auto res = std::make_pair(
      display, eglCreateContext(display, *config, shareContext, contextAttribs.data()));
  CHECK_EGL_ERRORS();
  return res;
}

EGLConfig chooseConfig(uint8_t contextMajorVersion, EGLDisplay display) {
  IGL_DEBUG_ASSERT(contextMajorVersion == 2 || contextMajorVersion == 3);
  const auto& attribs = contextMajorVersion == 2 ? attribsOpenGLES2 : attribsOpenGLES3;
  EGLConfig config{nullptr};
  EGLint numConfigs{0};
  const EGLBoolean status = eglChooseConfig(display, attribs.data(), &config, 1, &numConfigs);
  CHECK_EGL_ERRORS();
  if (!status) {
    IGL_DEBUG_ASSERT(status == EGL_TRUE, "eglChooseConfig failed");
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

Context::Context(EGLNativeWindowType window) :
  Context(kDefaultEGLBackendVersion, EGL_NO_CONTEXT, nullptr, false, window, {0, 0}) {}

Context::Context(BackendVersion backendVersion, EGLNativeWindowType window) :
  Context(backendVersion, EGL_NO_CONTEXT, nullptr, false, window, {0, 0}) {}

Context::Context(size_t width, size_t height) :
  Context(kDefaultEGLBackendVersion,
          EGL_NO_CONTEXT,
          nullptr,
          true,
          IGL_EGL_NULL_WINDOW,
          {static_cast<EGLint>(width), static_cast<EGLint>(height)}) {}

Context::Context(const Context& sharedContext) :
  Context(sharedContext.backendVersion_,
          sharedContext.context_,
          sharedContext.sharegroup_,
          true,
          IGL_EGL_NULL_WINDOW,
          sharedContext.getDrawSurfaceDimensions(nullptr)) {}

Context::Context(BackendVersion backendVersion,
                 EGLContext shareContext,
                 std::shared_ptr<std::vector<EGLContext>> sharegroup,
                 bool offscreen,
                 EGLNativeWindowType window,
                 std::pair<EGLint, EGLint> dimensions) :
  backendVersion_(backendVersion) {
  IGL_DEBUG_ASSERT(backendVersion.flavor == BackendFlavor::OpenGL_ES);
  IGL_DEBUG_ASSERT(
      (shareContext == EGL_NO_CONTEXT && sharegroup == nullptr) ||
          (shareContext != EGL_NO_CONTEXT && sharegroup != nullptr &&
           std::find(sharegroup->begin(), sharegroup->end(), shareContext) != sharegroup->end()),
      "shareContext and sharegroup values must be consistent");
  EGLConfig config{nullptr};
  auto contextDisplay =
      newEGLContext(backendVersion.majorVersion, getDefaultEGLDisplay(), shareContext, &config);
  IGL_DEBUG_ASSERT(contextDisplay.second != EGL_NO_CONTEXT, "newEGLContext failed");

  contextOwned_ = true;
  display_ = contextDisplay.first;
  context_ = contextDisplay.second;
  IContext::registerContext((void*)context_, this);
  if (!window) {
    if (offscreen) {
      std::array pbufferAttribs{EGLint{EGL_WIDTH},
                                EGLint{dimensions.first},
                                EGLint{EGL_HEIGHT},
                                EGLint{dimensions.second},
                                EGLint{EGL_NONE}};
      readSurface_ = drawSurface_ =
          eglCreatePbufferSurface(display_, config, pbufferAttribs.data());
      surfacesOwned_ = true;
      CHECK_EGL_ERRORS();
    } else {
      readSurface_ = eglGetCurrentSurface(EGL_READ);
      CHECK_EGL_ERRORS();
      drawSurface_ = eglGetCurrentSurface(EGL_DRAW);
      CHECK_EGL_ERRORS();
      surfacesOwned_ = false;
    }
  } else {
    surface_ = eglCreateWindowSurface(display_, config, window, nullptr);
    CHECK_EGL_ERRORS();
    readSurface_ = surface_;
    drawSurface_ = surface_;
    surfacesOwned_ = true;
  }
  config_ = config;

  if (sharegroup != nullptr) {
    sharegroup_ = std::move(sharegroup);
  } else {
    sharegroup_ = std::make_shared<std::vector<EGLContext>>();
  }
  sharegroup_->emplace_back(context_);

  initialize();
}

std::unique_ptr<IContext> Context::createShareContext(Result* /*outResult*/) {
  return std::make_unique<Context>(*this);
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
Context::Context(EGLDisplay display,
                 EGLContext context,
                 EGLSurface readSurface,
                 EGLSurface drawSurface,
                 EGLConfig config,
                 bool ownsContext,
                 bool ownsSurfaces) :
  // NOLINTEND(bugprone-easily-swappable-parameters)
  contextOwned_(ownsContext),
  surfacesOwned_(ownsSurfaces),
  display_(display),
  context_(context),
  readSurface_(readSurface),
  drawSurface_(drawSurface),
  config_(config),
  backendVersion_(kDefaultEGLBackendVersion) {
  IContext::registerContext((void*)context_, this);
  initialize();
  sharegroup_ = std::make_shared<std::vector<EGLContext>>();
  sharegroup_->emplace_back(context_);
}

void Context::updateSurface(NativeWindowType window) {
  surface_ = eglCreateWindowSurface(
      display_, chooseConfig(backendVersion_.majorVersion, display_), window, nullptr);
  CHECK_EGL_ERRORS();
  readSurface_ = surface_;
  drawSurface_ = surface_;
  surfacesOwned_ = true;
}

Context::~Context() {
  willDestroy((void*)context_);
  IContext::unregisterContext((void*)context_);
  if (surfacesOwned_) {
    if (surface_ != nullptr) {
      eglDestroySurface(display_, surface_);
      CHECK_EGL_ERRORS();
    }
    if (drawSurface_ != nullptr && drawSurface_ != surface_) {
      eglDestroySurface(display_, drawSurface_);
      CHECK_EGL_ERRORS();
    }
    if (readSurface_ != nullptr && readSurface_ != surface_ && readSurface_ != drawSurface_) {
      eglDestroySurface(display_, readSurface_);
      CHECK_EGL_ERRORS();
    }
  }
  if (contextOwned_ && context_ != EGL_NO_CONTEXT) {
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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Context::updateSurfaces(EGLSurface readSurface, EGLSurface drawSurface) {
  readSurface_ = readSurface;
  drawSurface_ = drawSurface;
  surfacesOwned_ = false;
  // We need this here because we need to call eglSetCurrent() with the new surface(s) in order to
  // bind them, but it's not the ideal place for it. Outside code could come in and make a
  // different context current at any time.
  setCurrent();
}

EGLSurface Context::createSurface(NativeWindowType window) {
  auto* surface = eglCreateWindowSurface(
      display_, chooseConfig(backendVersion_.majorVersion, display_), window, nullptr);
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
  EGLint hwBufferAttribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE, EGL_NONE};

  EGLDisplay display = this->getDisplay();
  // eglCreateImageKHR will add a ref to the AHardwareBuffer
  EGLImageKHR eglImage = eglCreateImageKHR(
      display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, hwBufferAttribs);
  IGL_LOG_DEBUG("eglCreateImageKHR(%p, %x, %x, %p, {%d, %d, %d, %d, %d})\n",
                display,
                EGL_NO_CONTEXT,
                EGL_NATIVE_BUFFER_ANDROID,
                clientBuffer,
                hwBufferAttribs[0],
                hwBufferAttribs[1],
                hwBufferAttribs[2],
                hwBufferAttribs[3],
                hwBufferAttribs[4]);

  this->checkForErrors(__FUNCTION__, __LINE__);

  IGL_SOFT_ASSERT(this->isCurrentContext() || this->isCurrentSharegroup());

  return eglImage;
}

void Context::imageTargetTexture(EGLImageKHR eglImage, GLenum target) const {
  glEGLImageTargetTexture2DOES(target, static_cast<GLeglImageOES>(eglImage));
  IGL_LOG_DEBUG("glEGLImageTargetTexture2DOES(%u, %#x)\n",
                GL_TEXTURE_2D,
                static_cast<GLeglImageOES>(eglImage));
  this->checkForErrors(__FUNCTION__, __LINE__);
}
#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

} // namespace igl::opengl::egl
FOLLY_POP_WARNING
