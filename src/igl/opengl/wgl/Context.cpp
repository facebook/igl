/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/wgl/Context.h>

#include <igl/opengl/Texture.h>

#define WIN32_LEAN_AND_MEAN
#if !defined(NOMINMAX)
#define NOMINMAX
#endif // NOMINMAX
#include <windows.h>
#ifdef DISABLE_WGL_VSYNC
#include <GL/wglext.h>
#endif

#if defined(IGL_WITH_TRACY_GPU)
#include "tracy/TracyOpenGL.hpp"
#endif

namespace igl {
namespace opengl {
namespace wgl {

Context::Context(RenderingAPI api) : contextOwned_(true) {
  // This ctor path will own the wgl render context. Therefore creation to the window, DC & render
  // context must be done and in sequence. Creating a dummy window is necessary to get the device
  // context. We let wgl choose the appropriate pixel format for us to retrieve the valid render
  // context after setting up the correct pixel format. Until then, we can properly create the right
  // render context.

  WNDCLASSA window_class = {};
  window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  window_class.cbClsExtra = window_class.cbWndExtra = 0;
  window_class.lpfnWndProc = DefWindowProcA;
  window_class.hInstance = GetModuleHandle(nullptr);
  window_class.hIcon = nullptr;
  window_class.hCursor = nullptr;
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = "Dummy_WGL";

  if (!RegisterClassA(&window_class)) {
    auto lastError = GetLastError();
    if (lastError != ERROR_CLASS_ALREADY_EXISTS) {
      IGL_DEBUG_ABORT("[IGL] WGL error 0x%08X:\n", GetLastError());
    }
  }

  dummyWindow_ = CreateWindowExA(0,
                                 window_class.lpszClassName,
                                 "Dummy OpenGL Window",
                                 0,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 nullptr,
                                 nullptr,
                                 window_class.hInstance,
                                 nullptr);

  IGL_DEBUG_ASSERT(dummyWindow_ != nullptr,
                   "[IGL] Failed to create dummy OpenGL window. WGL error 0x%08X:\n",
                   GetLastError());

  deviceContext_ = GetDC(dummyWindow_);

  PIXELFORMATDESCRIPTOR pfd;
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cAlphaBits = 8;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;

  int pixel_format = ChoosePixelFormat(deviceContext_, &pfd);
  IGL_DEBUG_ASSERT(pixel_format != 0,
                   "[IGL] Failed to find a suitable pixel format. WGL error 0x%08X:\n",
                   GetLastError());

  if (!SetPixelFormat(deviceContext_, pixel_format, &pfd)) {
    IGL_DEBUG_ABORT("[IGL] Failed to set the pixel format. WGL error 0x%08X:\n", GetLastError());
  }

  renderContext_ = wglCreateContext(deviceContext_);
  if (!renderContext_) {
    IGL_DEBUG_ABORT("[IGL] Failed to create a dummy OpenGL rendering context. WGL error 0x%08X:\n",
                    GetLastError());
  }

  IContext::registerContext((void*)renderContext_, this);

  // Set current, since creation doesn't really mean it's current yet.
  setCurrent();

  // Init WGL, we need to initialize this first before running any GL calls
  glewInit();

  igl::Result result;
  // Initialize through base class.
  initialize(&result);
  IGL_DEBUG_ASSERT(result.isOk());
}

Context::Context(HDC deviceContext, HGLRC renderContext) :
  contextOwned_(false), deviceContext_(deviceContext), renderContext_(renderContext) {
  IContext::registerContext((void*)renderContext_, this);

  // Set current, since creation doesn't really mean it's current yet.
  setCurrent();

  // Init WGL, we need to initialize this first before running any GL calls
  glewInit();

  igl::Result result;
  // Initialize through base class.
  initialize(&result);
  IGL_DEBUG_ASSERT(result.isOk());
}

Context::Context(HDC deviceContext, HGLRC renderContext, std::vector<HGLRC> shareContexts) :
  contextOwned_(false),
  deviceContext_(deviceContext),
  renderContext_(renderContext),
  sharegroup_(shareContexts) {
  IContext::registerContext((void*)renderContext_, this);

  // Set current, since creation doesn't really mean it's current yet.
  setCurrent();

  // Init WGL, we need to initialize this first before running any GL calls
  glewInit();

  igl::Result result;
  // Initialize through base class.
  initialize(&result);
  IGL_DEBUG_ASSERT(result.isOk());
}

Context::~Context() {
  // Clear pool explicitly, since it might have reference back to IContext.
  getAdapterPool().clear();
  getComputeAdapterPool().clear();

  // Unregister wglContext
  IContext::unregisterContext(renderContext_);

  if (contextOwned_) {
    wglMakeCurrent(deviceContext_, nullptr);
    wglDeleteContext(renderContext_);
    ReleaseDC(dummyWindow_, deviceContext_);
    DestroyWindow(dummyWindow_);
  }
}

void Context::setCurrent() {
  if (!wglMakeCurrent(deviceContext_, renderContext_)) {
    IGL_DEBUG_ABORT("[IGL] Failed to activate OpenGL render context. WGL error 0x%08X:\n",
                    GetLastError());
  }
  flushDeletionQueue();

#ifdef DISABLE_WGL_VSYNC
  static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
      (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
  if (IGL_DEBUG_VERIFY(wglSwapIntervalEXT)) {
    wglSwapIntervalEXT(0);
  }
#endif
}

void Context::clearCurrentContext() const {
  if (!wglMakeCurrent(nullptr, nullptr)) {
    IGL_DEBUG_ABORT("[IGL] Failed to clear OpenGL render context. WGL error 0x%08X:\n",
                    GetLastError());
  }
}

bool Context::isCurrentContext() const {
  return wglGetCurrentContext() == renderContext_;
}

bool Context::isCurrentSharegroup() const {
  auto it = std::find(sharegroup_.begin(), sharegroup_.end(), wglGetCurrentContext());
  return it != sharegroup_.end();
}

void Context::present(std::shared_ptr<ITexture> surface) const {
  SwapBuffers(deviceContext_);
  wglMakeCurrent(deviceContext_, renderContext_);

#if defined(IGL_WITH_TRACY_GPU)
  TracyGpuCollect;
#endif
}

std::unique_ptr<IContext> Context::createShareContext(Result* outResult) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  Result::setResult(outResult, Result::Code::Unimplemented, "Implement as needed");
  return nullptr;
}

} // namespace wgl
} // namespace opengl
} // namespace igl
