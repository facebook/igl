/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "SamplesCommon.h"
#include "IGLCommon.h"

// GLFW
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#elif __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#else
#error Unsupported OS
#endif

#include <GLFW/glfw3native.h>

#include <cstdio>

namespace igl::sample {

CallbackKeyboard callbackKeyboard_ = nullptr;
CallbackMouseButton callbackMouseButton_ = nullptr;
CallbackMousePos callbackMousePos_ = nullptr;

GLFWwindow* window_ = nullptr;
int *width_, *height_;
int *fbWidth_, *fbHeight_;
IDevice* device_;

Keys glfw2sampleKey(int key) {
  switch (key) {
  case GLFW_KEY_SPACE:
    return Keys::Key_SPACE;
  case GLFW_KEY_APOSTROPHE:
    return Keys::Key_APOSTROPHE; /* ' */
  case GLFW_KEY_COMMA:
    return Keys::Key_COMMA; /* , */
  case GLFW_KEY_MINUS:
    return Keys::Key_MINUS; /* - */
  case GLFW_KEY_PERIOD:
    return Keys::Key_PERIOD; /* . */
  case GLFW_KEY_SLASH:
    return Keys::Key_SLASH; /* / */
  case GLFW_KEY_0:
    return Keys::Key_0;
  case GLFW_KEY_1:
    return Keys::Key_1;
  case GLFW_KEY_2:
    return Keys::Key_2;
  case GLFW_KEY_3:
    return Keys::Key_3;
  case GLFW_KEY_4:
    return Keys::Key_4;
  case GLFW_KEY_5:
    return Keys::Key_5;
  case GLFW_KEY_6:
    return Keys::Key_6;
  case GLFW_KEY_7:
    return Keys::Key_7;
  case GLFW_KEY_8:
    return Keys::Key_8;
  case GLFW_KEY_9:
    return Keys::Key_9;
  case GLFW_KEY_SEMICOLON:
    return Keys::Key_SEMICOLON; /* ; */
  case GLFW_KEY_EQUAL:
    return Keys::Key_EQUAL; /* = */
  case GLFW_KEY_A:
    return Keys::Key_A;
  case GLFW_KEY_B:
    return Keys::Key_B;
  case GLFW_KEY_C:
    return Keys::Key_C;
  case GLFW_KEY_D:
    return Keys::Key_D;
  case GLFW_KEY_E:
    return Keys::Key_E;
  case GLFW_KEY_F:
    return Keys::Key_F;
  case GLFW_KEY_G:
    return Keys::Key_G;
  case GLFW_KEY_H:
    return Keys::Key_H;
  case GLFW_KEY_I:
    return Keys::Key_I;
  case GLFW_KEY_J:
    return Keys::Key_J;
  case GLFW_KEY_K:
    return Keys::Key_K;
  case GLFW_KEY_L:
    return Keys::Key_L;
  case GLFW_KEY_M:
    return Keys::Key_M;
  case GLFW_KEY_N:
    return Keys::Key_N;
  case GLFW_KEY_O:
    return Keys::Key_O;
  case GLFW_KEY_P:
    return Keys::Key_P;
  case GLFW_KEY_Q:
    return Keys::Key_Q;
  case GLFW_KEY_R:
    return Keys::Key_R;
  case GLFW_KEY_S:
    return Keys::Key_S;
  case GLFW_KEY_T:
    return Keys::Key_T;
  case GLFW_KEY_U:
    return Keys::Key_U;
  case GLFW_KEY_V:
    return Keys::Key_V;
  case GLFW_KEY_W:
    return Keys::Key_W;
  case GLFW_KEY_X:
    return Keys::Key_X;
  case GLFW_KEY_Y:
    return Keys::Key_Y;
  case GLFW_KEY_Z:
    return Keys::Key_Z;
  case GLFW_KEY_LEFT_BRACKET:
    return Keys::Key_LEFT_BRACKET; /* [ */
  case GLFW_KEY_BACKSLASH:
    return Keys::Key_BACKSLASH; /* \ */
  case GLFW_KEY_RIGHT_BRACKET:
    return Keys::Key_RIGHT_BRACKET; /* ] */
  case GLFW_KEY_GRAVE_ACCENT:
    return Keys::Key_GRAVE_ACCENT; /* ` */
  case GLFW_KEY_WORLD_1:
    return Keys::Key_WORLD_1; /* non-US #1 */
  case GLFW_KEY_WORLD_2:
    return Keys::Key_WORLD_2; /* non-US #2 */
  case GLFW_KEY_ESCAPE:
    return Keys::Key_ESCAPE;
  case GLFW_KEY_ENTER:
    return Keys::Key_ENTER;
  case GLFW_KEY_TAB:
    return Keys::Key_TAB;
  case GLFW_KEY_BACKSPACE:
    return Keys::Key_BACKSPACE;
  case GLFW_KEY_INSERT:
    return Keys::Key_INSERT;
  case GLFW_KEY_DELETE:
    return Keys::Key_DELETE;
  case GLFW_KEY_RIGHT:
    return Keys::Key_RIGHT;
  case GLFW_KEY_LEFT:
    return Keys::Key_LEFT;
  case GLFW_KEY_DOWN:
    return Keys::Key_DOWN;
  case GLFW_KEY_UP:
    return Keys::Key_UP;
  case GLFW_KEY_PAGE_UP:
    return Keys::Key_PAGE_UP;
  case GLFW_KEY_PAGE_DOWN:
    return Keys::Key_PAGE_DOWN;
  case GLFW_KEY_HOME:
    return Keys::Key_HOME;
  case GLFW_KEY_END:
    return Keys::Key_END;
  case GLFW_KEY_CAPS_LOCK:
    return Keys::Key_CAPS_LOCK;
  case GLFW_KEY_SCROLL_LOCK:
    return Keys::Key_SCROLL_LOCK;
  case GLFW_KEY_NUM_LOCK:
    return Keys::Key_NUM_LOCK;
  case GLFW_KEY_PRINT_SCREEN:
    return Keys::Key_PRINT_SCREEN;
  case GLFW_KEY_PAUSE:
    return Keys::Key_PAUSE;
  case GLFW_KEY_F1:
    return Keys::Key_F1;
  case GLFW_KEY_F2:
    return Keys::Key_F2;
  case GLFW_KEY_F3:
    return Keys::Key_F3;
  case GLFW_KEY_F4:
    return Keys::Key_F4;
  case GLFW_KEY_F5:
    return Keys::Key_F5;
  case GLFW_KEY_F6:
    return Keys::Key_F6;
  case GLFW_KEY_F7:
    return Keys::Key_F7;
  case GLFW_KEY_F8:
    return Keys::Key_F8;
  case GLFW_KEY_F9:
    return Keys::Key_F9;
  case GLFW_KEY_F10:
    return Keys::Key_F10;
  case GLFW_KEY_F11:
    return Keys::Key_F11;
  case GLFW_KEY_F12:
    return Keys::Key_F12;
  case GLFW_KEY_F13:
    return Keys::Key_F13;
  case GLFW_KEY_F14:
    return Keys::Key_F14;
  case GLFW_KEY_F15:
    return Keys::Key_F15;
  case GLFW_KEY_F16:
    return Keys::Key_F16;
  case GLFW_KEY_F17:
    return Keys::Key_F17;
  case GLFW_KEY_F18:
    return Keys::Key_F18;
  case GLFW_KEY_F19:
    return Keys::Key_F19;
  case GLFW_KEY_F20:
    return Keys::Key_F20;
  case GLFW_KEY_F21:
    return Keys::Key_F21;
  case GLFW_KEY_F22:
    return Keys::Key_F22;
  case GLFW_KEY_F23:
    return Keys::Key_F23;
  case GLFW_KEY_F24:
    return Keys::Key_F24;
  case GLFW_KEY_F25:
    return Keys::Key_F25;
  case GLFW_KEY_KP_0:
    return Keys::Key_KP_0;
  case GLFW_KEY_KP_1:
    return Keys::Key_KP_1;
  case GLFW_KEY_KP_2:
    return Keys::Key_KP_2;
  case GLFW_KEY_KP_3:
    return Keys::Key_KP_3;
  case GLFW_KEY_KP_4:
    return Keys::Key_KP_4;
  case GLFW_KEY_KP_5:
    return Keys::Key_KP_5;
  case GLFW_KEY_KP_6:
    return Keys::Key_KP_6;
  case GLFW_KEY_KP_7:
    return Keys::Key_KP_7;
  case GLFW_KEY_KP_8:
    return Keys::Key_KP_8;
  case GLFW_KEY_KP_9:
    return Keys::Key_KP_9;
  case GLFW_KEY_KP_DECIMAL:
    return Keys::Key_KP_DECIMAL;
  case GLFW_KEY_KP_DIVIDE:
    return Keys::Key_KP_DIVIDE;
  case GLFW_KEY_KP_MULTIPLY:
    return Keys::Key_KP_MULTIPLY;
  case GLFW_KEY_KP_SUBTRACT:
    return Keys::Key_KP_SUBTRACT;
  case GLFW_KEY_KP_ADD:
    return Keys::Key_KP_ADD;
  case GLFW_KEY_KP_ENTER:
    return Keys::Key_KP_ENTER;
  case GLFW_KEY_KP_EQUAL:
    return Keys::Key_KP_EQUAL;
  case GLFW_KEY_LEFT_SHIFT:
    return Keys::Key_LEFT_SHIFT;
  case GLFW_KEY_LEFT_CONTROL:
    return Keys::Key_LEFT_CONTROL;
  case GLFW_KEY_LEFT_ALT:
    return Keys::Key_LEFT_ALT;
  case GLFW_KEY_LEFT_SUPER:
    return Keys::Key_LEFT_SUPER;
  case GLFW_KEY_RIGHT_SHIFT:
    return Keys::Key_RIGHT_SHIFT;
  case GLFW_KEY_RIGHT_CONTROL:
    return Keys::Key_RIGHT_CONTROL;
  case GLFW_KEY_RIGHT_ALT:
    return Keys::Key_RIGHT_ALT;
  case GLFW_KEY_RIGHT_SUPER:
    return Keys::Key_RIGHT_SUPER;
  case GLFW_KEY_MENU:
    return Keys::Key_MENU;
  default:
    return Keys::Key_Unknown;
  }
}

KeyMods glfw2sampleKeyMods(int mods) {
  int result = (int)KeyMods::None;
  result |= (mods & GLFW_MOD_SHIFT) ? (int)KeyMods::Shift : 0;
  result |= (mods & GLFW_MOD_ALT) ? (int)KeyMods::Alt : 0;
  result |= (mods & GLFW_MOD_CONTROL) ? (int)KeyMods::Control : 0;
  result |= (mods & GLFW_MOD_SUPER) ? (int)KeyMods::Meta : 0;
  return static_cast<KeyMods>(result);
}

MouseButton glfw2sampleMouseButton(int button) {
  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT:
    return MouseButton::Left;
  case GLFW_MOUSE_BUTTON_RIGHT:
    return MouseButton::Right;
  case GLFW_MOUSE_BUTTON_MIDDLE:
    return MouseButton::Middle;
  default:
    return MouseButton::Unknown;
  }
}

bool initWindow(const char* windowTitle,
                bool fullscreen,
                int* widthPtr,
                int* heightPtr,
                int* fbWidthPtr,
                int* fbHeightPtr) {
  if (!glfwInit()) {
    return false;
  }

#if USE_OPENGL_BACKEND
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  glfwWindowHint(GLFW_VISIBLE, true);
  glfwWindowHint(GLFW_DOUBLEBUFFER, true);
  glfwWindowHint(GLFW_SRGB_CAPABLE, true);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
#else
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif
  glfwWindowHint(GLFW_RESIZABLE, fullscreen ? GLFW_FALSE : GLFW_TRUE);

  const char* title = windowTitle;

  int posX = 0;
  int posY = 0;
  int width = 1280;
  int height = 1024;

  if (fullscreen) {
    // render full screen without overlapping taskbar
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    width = mode->width;
    height = mode->height;

    glfwGetMonitorWorkarea(monitor, &posX, &posY, &width, &height);
  }

  GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);

  if (!window) {
    glfwTerminate();
    return false;
  }

  if (fullscreen) {
    glfwSetWindowPos(window, posX, posY);
  }

  glfwSetErrorCallback([](int error, const char* description) {
    printf("GLFW Error (%i): %s\n", error, description);
  });

  glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (callbackKeyboard_) {
      callbackKeyboard_(glfw2sampleKey(key), action == GLFW_PRESS, glfw2sampleKeyMods(mods));
    }
  });

  glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
    if (callbackMouseButton_) {
      double xpos, ypos;
      glfwGetCursorPos(window, &xpos, &ypos);

      callbackMouseButton_(glfw2sampleMouseButton(button), action == GLFW_PRESS, xpos, ypos);
    }
  });
  glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
    if (callbackMousePos_) {
      callbackMousePos_(xpos, ypos);
    }
  });

  width_ = widthPtr;
  height_ = heightPtr;
  fbWidth_ = fbWidthPtr;
  fbHeight_ = fbHeightPtr;

  // @lint-ignore CLANGTIDY
  glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
    printf("Window resized! width=%d, height=%d\n", width, height);
    *width_ = width;
    *height_ = height;
    glfwGetFramebufferSize(window, fbWidth_, fbHeight_);

#if !USE_OPENGL_BACKEND
    IGL_ASSERT_MSG(device_, "Device should be set with sample::setDevice()");
    auto* vulkanDevice = static_cast<vulkan::Device*>(device_);
    auto& ctx = vulkanDevice->getVulkanContext();
    ctx.initSwapchain(width, height);
#endif
  });

  glfwGetWindowSize(window, width_, height_);
  glfwGetFramebufferSize(window, fbWidth_, fbHeight_);

  *&window_ = window;

  return true;
}

void setDevice(IDevice* device) {
  device_ = device;
}

void* getWindow() {
#if IGL_PLATFORM_WIN
  return (void*)glfwGetWin32Window(window_);
#elif IGL_PLATFORM_APPLE
  return (void*)glfwGetCocoaWindow(window_);
#elif IGL_PLATFORM_LINUX
  return (void*)glfwGetX11Window(window_);
#else
#error Unsupported OS
#endif
}

void* getDisplay() {
#if IGL_PLATFORM_WIN
  return nullptr;
#elif IGL_PLATFORM_APPLE
  return nullptr;
#elif IGL_PLATFORM_LINUX
  return (void*)glfwGetX11Display();
#else
#error Unsupported OS
#endif
}

void* getContext() {
#if IGL_PLATFORM_WIN
  return (void*)glfwGetWGLContext(window_);
#elif IGL_PLATFORM_APPLE
  return (void*)glfwGetNSGLContext(window_);
#elif IGL_PLATFORM_LINUX
  return (void*)glfwGetGLXContext(window_);
#else
#error Unsupported OS
#endif
}

bool isDone() {
  return glfwWindowShouldClose(window_);
}

void update() {
  glfwPollEvents();
}

void shutdown() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

double getTimeInSecs() {
  return glfwGetTime();
}

void setCallbackKeyboard(CallbackKeyboard callback) {
  callbackKeyboard_ = callback;
}

void setCallbackMouseButton(CallbackMouseButton callback) {
  callbackMouseButton_ = callback;
}

void setCallbackMousePos(CallbackMousePos callback) {
  callbackMousePos_ = callback;
}

} // namespace igl::sample
