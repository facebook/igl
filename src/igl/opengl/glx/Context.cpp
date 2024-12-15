/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/opengl/glx/Context.h>

#include <igl/opengl/Texture.h>

#include <X11/X.h>
#include <dlfcn.h>

#include <string>
#include <vector>

namespace {

int GetLastError() {
  return 0; // TODO: implement error handling
}

} // namespace

namespace igl {
namespace opengl {
namespace glx {

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_PBUFFER_HEIGHT 0x8040
#define GLX_PBUFFER_WIDTH 0x8041

typedef XID GLXPbuffer;
typedef struct __GLXFBConfig* GLXFBConfig;
typedef void (*__GLXextproc)(void);

typedef __GLXextproc (*PFNGLXGETPROCADDRESSPROC)(const GLubyte* procName);

typedef Display* (*PFNXOPENDISPLAY)(const char*);
typedef int (*PFNXCLOSEDISPLAY)(Display*);

typedef int (*PFNXFREE)(void*);
typedef GLXFBConfig* (*PFNGLXCHOOSEFBCONFIGPROC)(Display*, int, const int*, int*);
typedef GLXContext (
    *PFNGLXCREATECONTEXTATTRIBSARB)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef void (*PFNGLXDESTROYCONTEXT)(Display*, GLXContext);
typedef GLXPbuffer (*PFNGLXCREATEPBUFFERPROC)(Display*, GLXFBConfig, const int*);
typedef void (*PFNGLXDESTROYPBUFFER)(Display*, GLXPbuffer);
typedef Bool (*PFNGLXMAKECURRENTPROC)(Display*, GLXDrawable, GLXContext);
typedef void (*PFNGLXSWAPBUFFERSPROC)(Display*, GLXDrawable);

typedef GLXContext (*PFNGLXGETCURRENTCONTEXTPROC)();

struct GLXSharedModule {
  GLXSharedModule() {
    std::vector<std::string> libs = {
        "libGLX.so.0",
        "libGL.so.1",
        "libGL.so",

    };

    for (const auto& lib : libs) {
      module_ = dlopen(lib.c_str(), RTLD_LAZY | RTLD_LOCAL);
      if (module_) {
        break;
      }
    }

    IGL_DEBUG_ASSERT(module_ != nullptr, "[IGL] Failed to initialize GLX");

    XOpenDisplay = loadFunction<PFNXOPENDISPLAY>("XOpenDisplay");
    XCloseDisplay = loadFunction<PFNXCLOSEDISPLAY>("XCloseDisplay");
    XFree = loadFunction<PFNXFREE>("XFree");

    glXGetProcAddress = loadFunction<PFNGLXGETPROCADDRESSPROC>("glXGetProcAddress");
    glXGetProcAddressARB = loadFunction<PFNGLXGETPROCADDRESSPROC>("glXGetProcAddressARB");

    glXChooseFBConfig = loadGlxFunction<PFNGLXCHOOSEFBCONFIGPROC>("glXChooseFBConfig");
    glXCreateContextAttribsARB =
        loadGlxFunction<PFNGLXCREATECONTEXTATTRIBSARB>("glXCreateContextAttribsARB");

    glXDestroyContext = loadGlxFunction<PFNGLXDESTROYCONTEXT>("glXDestroyContext");
    glXCreatePbuffer = loadGlxFunction<PFNGLXCREATEPBUFFERPROC>("glXCreatePbuffer");
    glXDestroyPbuffer = loadGlxFunction<PFNGLXDESTROYPBUFFER>("glXDestroyPbuffer");
    glXMakeCurrent = loadGlxFunction<PFNGLXMAKECURRENTPROC>("glXMakeCurrent");
    glXSwapBuffers = loadGlxFunction<PFNGLXSWAPBUFFERSPROC>("glXSwapBuffers");
    glXGetCurrentContext = loadGlxFunction<PFNGLXGETCURRENTCONTEXTPROC>("glXGetCurrentContext");
  }

  ~GLXSharedModule() {
    if (module_) {
      dlclose(module_);
    }
  }

  template<typename T>
  T loadFunction(const char* func) {
    auto f = reinterpret_cast<T>(dlsym(module_, func));
    IGL_DEBUG_ASSERT(f != nullptr, "[IGL] Failed to initialize GLX, %s is not found", func);
    return f;
  }

  template<typename T>
  T loadGlxFunction(const char* func) {
    if (auto f = reinterpret_cast<T>(glXGetProcAddress(reinterpret_cast<const GLubyte*>(func)))) {
      return f;
    }
    if (auto f =
            reinterpret_cast<T>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(func)))) {
      return f;
    }
    return loadFunction<T>(func);
  }

  void* module_ = nullptr;

  PFNXOPENDISPLAY XOpenDisplay = nullptr;
  PFNXCLOSEDISPLAY XCloseDisplay = nullptr;
  PFNXFREE XFree = nullptr;

  PFNGLXGETPROCADDRESSPROC glXGetProcAddress = nullptr;
  PFNGLXGETPROCADDRESSPROC glXGetProcAddressARB = nullptr;

  PFNGLXCHOOSEFBCONFIGPROC glXChooseFBConfig = nullptr;
  PFNGLXCREATECONTEXTATTRIBSARB glXCreateContextAttribsARB = nullptr;
  PFNGLXDESTROYCONTEXT glXDestroyContext = nullptr;
  PFNGLXCREATEPBUFFERPROC glXCreatePbuffer = nullptr;
  PFNGLXDESTROYPBUFFER glXDestroyPbuffer = nullptr;
  PFNGLXMAKECURRENTPROC glXMakeCurrent = nullptr;
  PFNGLXSWAPBUFFERSPROC glXSwapBuffers = nullptr;
  PFNGLXGETCURRENTCONTEXTPROC glXGetCurrentContext = nullptr;
};

Context::Context(std::shared_ptr<GLXSharedModule> module,
                 bool offscreen /* = false */,
                 uint32_t width /* = 0 */,
                 uint32_t height /* = 0 */) :
  contextOwned_(true), offscreen_(offscreen), module_(std::move(module)) {
  if (!module_) {
    module_ = std::make_shared<GLXSharedModule>();
  }

  static int visualAttribs[] = {None};
  int contextAttribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 4, GLX_CONTEXT_MINOR_VERSION_ARB, 6, None};

  GLXPbuffer pbuf;

  if (display_ = module_->XOpenDisplay(0); display_ != nullptr) {
    int fbcount = 0;
    if (GLXFBConfig* fbc = module_->glXChooseFBConfig(
            display_, DefaultScreen(display_), visualAttribs, &fbcount)) {
      if (contextHandle_ =
              module_->glXCreateContextAttribsARB(display_, fbc[0], 0, True, contextAttribs);

          contextHandle_ != nullptr) {
        IContext::registerContext((void*)contextHandle_, this);
      } else {
        IGL_DEBUG_ABORT("[IGL] Failed to create GLX context");
      }

      if (offscreen_) {
        int pbufferAttribs[] = {GLX_PBUFFER_WIDTH,
                                static_cast<int>(width),
                                GLX_PBUFFER_HEIGHT,
                                static_cast<int>(height),
                                None};

        windowHandle_ = module_->glXCreatePbuffer(display_, fbc[0], pbufferAttribs);
      }

      module_->XFree(fbc);

      // Set current, since creation doesn't really mean it's current yet.
      setCurrent();

      // Initialize through base class.
      igl::Result result;
      initialize(&result);
      IGL_DEBUG_ASSERT(result.isOk(), result.message.c_str());
    } else {
      IGL_DEBUG_ABORT("[IGL] Failed to get GLX framebuffer configs");
    }
  } else {
    IGL_DEBUG_ABORT("[IGL] Failed to open display");
  }
}

Context::Context(std::shared_ptr<GLXSharedModule> module,
                 Display* display,
                 GLXDrawable windowHandle,
                 GLXContext contextHandle) :
  contextOwned_(false),
  module_(std::move(module)),
  display_(display),
  windowHandle_(windowHandle),
  contextHandle_(contextHandle) {
  if (!module_) {
    module_ = std::make_shared<GLXSharedModule>();
  }

  IContext::registerContext((void*)contextHandle_, this);

  // Set current, since creation doesn't really mean it's current yet.
  setCurrent();

  // Initialize through base class.
  igl::Result result;
  initialize(&result);
  IGL_DEBUG_ASSERT(result.isOk(), result.message.c_str());
}

Context::~Context() {
  // Clear pool explicitly, since it might have reference back to IContext.
  getAdapterPool().clear();

  // Unregister GLX Context.
  IContext::unregisterContext(contextHandle_);

  // Destroy GLX.
  if (contextOwned_) {
    if (offscreen_) {
      module_->glXDestroyPbuffer(display_, windowHandle_);
      windowHandle_ = 0;
    }
    if (contextHandle_) {
      module_->glXDestroyContext(display_, contextHandle_);
      contextHandle_ = nullptr;
    }
    if (display_) {
      module_->XCloseDisplay(display_);
      display_ = nullptr;
    }
  }
}

void Context::setCurrent() {
  if (!module_->glXMakeCurrent(display_, windowHandle_, contextHandle_)) {
    IGL_DEBUG_ABORT("[IGL] Failed to activate OpenGL render context. GLX error 0x%08X:\n",
                    GetLastError());
  }
  flushDeletionQueue();
}

void Context::clearCurrentContext() const {
  if (!module_->glXMakeCurrent(display_, None, nullptr)) {
    IGL_DEBUG_ASSERT(
        false, "[IGL] Failed to clear OpenGL render context. GLX error 0x%08X:\n", GetLastError());
  }
}

bool Context::isCurrentContext() const {
  return module_->glXGetCurrentContext() == contextHandle_;
}

bool Context::isCurrentSharegroup() const {
  return true;
}

void Context::present(std::shared_ptr<ITexture> surface) const {
  module_->glXSwapBuffers(display_, windowHandle_);
  module_->glXMakeCurrent(display_, windowHandle_, contextHandle_);
}

std::unique_ptr<IContext> Context::createShareContext(Result* outResult) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  Result::setResult(outResult, Result::Code::Unimplemented, "Implement as needed");
  return nullptr;
}

std::shared_ptr<GLXSharedModule> Context::getSharedModule() const {
  return module_;
}

} // namespace glx
} // namespace opengl
} // namespace igl
