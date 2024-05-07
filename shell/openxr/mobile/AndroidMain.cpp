/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <android_native_app_glue.h>
#include <igl/Common.h>
#include <igl/Macros.h>
#include <jni.h>
#include <memory>
#include <string>
#include <vector>
// #define ATTACH_DEBUGGER

#ifdef ATTACH_DEBUGGER
#include <unistd.h>
#endif

#include <shell/openxr/XrApp.h>

namespace {
std::vector<std::string> gActionViewQueue;
} // namespace

extern "C" {
void processActionView(JNIEnv* env, jstring data) {
  if (env == nullptr || data == nullptr) {
    return;
  }
  const jsize stringLength = env->GetStringUTFLength(data);
  const char* stringChars = env->GetStringUTFChars(data, nullptr);
  if (stringLength == 0 || stringChars == nullptr) {
    return;
  }
  gActionViewQueue.emplace_back(stringChars, stringLength);
  env->ReleaseStringUTFChars(data, stringChars);
}

JNIEXPORT void JNICALL
Java_com_facebook_igl_shell_openxr_vulkan_MainActivity_onActionView(JNIEnv* env,
                                                                    jclass /*clazz*/,
                                                                    jstring data) {
  processActionView(env, data);
}

JNIEXPORT void JNICALL
Java_com_facebook_igl_shell_openxr_gles_MainActivity_onActionView(JNIEnv* env,
                                                                  jclass /*clazz*/,
                                                                  jstring data) {
  processActionView(env, data);
}
}

using namespace igl::shell::openxr;

void handleInitWindow(const struct android_app* app) {
  auto xrApp = static_cast<igl::shell::openxr::XrApp*>(app->userData);
  if (xrApp) {
    xrApp->setNativeWindow(app->window);
  }
}
void handleTermWindow(const struct android_app* app) {
  auto xrApp = static_cast<igl::shell::openxr::XrApp*>(app->userData);
  if (xrApp) {
    xrApp->setNativeWindow(nullptr);
  }
}

void handleResume(const struct android_app* app) {
  auto xrApp = static_cast<igl::shell::openxr::XrApp*>(app->userData);
  if (xrApp) {
    xrApp->setResumed(true);
  }
}

void handlePause(const struct android_app* app) {
  auto xrApp = static_cast<igl::shell::openxr::XrApp*>(app->userData);
  if (xrApp) {
    xrApp->setResumed(false);
  }
}

void handleDestroy(const struct android_app* app) {
  auto xrApp = static_cast<igl::shell::openxr::XrApp*>(app->userData);
  if (xrApp) {
    xrApp->setNativeWindow(nullptr);
  }
}

void handleAppCmd(struct android_app* app, int32_t appCmd) {
  switch (appCmd) {
  case APP_CMD_INIT_WINDOW:
    IGL_LOG_INFO("APP_CMD_INIT_WINDOW");
    handleInitWindow(app);
    break;
  case APP_CMD_TERM_WINDOW:
    IGL_LOG_INFO("APP_CMD_TERM_WINDOW");
    handleTermWindow(app);
    break;
  case APP_CMD_RESUME:
    IGL_LOG_INFO("APP_CMD_RESUME");
    handleResume(app);
    break;
  case APP_CMD_PAUSE:
    IGL_LOG_INFO("APP_CMD_PAUSE");
    handlePause(app);
    break;
  case APP_CMD_STOP:
    IGL_LOG_INFO("APP_CMD_PAUSE");
    break;
  case APP_CMD_DESTROY:
    IGL_LOG_INFO("APP_CMD_DESTROY");
    handleDestroy(app);
    break;
  }
}

#if defined(USE_VULKAN_BACKEND)
#include "vulkan/XrAppImplVulkan.h"
#elif defined(USE_OPENGL_BACKEND)
#include "opengl/XrAppImplGLES.h"
// @fb-only
// @fb-only
#endif

XrInstance gInstance_;
XrInstance getXrInstance() {
  return gInstance_;
}

// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only

void android_main(struct android_app* app) {
  JNIEnv* Env;
  app->activity->vm->AttachCurrentThread(&Env, nullptr);

#ifdef ATTACH_DEBUGGER
  sleep(20);
#endif

#if defined(USE_VULKAN_BACKEND)
  auto xrApp = std::make_unique<XrApp>(std::make_unique<mobile::XrAppImplVulkan>());
#elif defined(USE_OPENGL_BACKEND)
  auto xrApp = std::make_unique<XrApp>(std::make_unique<mobile::XrAppImplGLES>());
// @fb-only
  // @fb-only
  // @fb-only
#endif
  if (!xrApp->initialize(app, {})) {
    return;
  }

  gInstance_ = xrApp->instance();

  app->onAppCmd = handleAppCmd;
  app->userData = xrApp.get();

  while (app->destroyRequested == 0) {
    for (;;) {
      int events;
      struct android_poll_source* source = nullptr;
      // If the timeout is zero, returns immediately without blocking.
      // If the timeout is negative, waits indefinitely until an event appears.
      const int timeout =
          (!xrApp->resumed() && !xrApp->sessionActive() && app->destroyRequested == 0) ? -1 : 0;
      if (ALooper_pollAll(timeout, nullptr, &events, (void**)&source) < 0) {
        break;
      }
      if (source != nullptr) {
        source->process(app, source);
      }
    }

    xrApp->handleXrEvents();
    if (!xrApp->sessionActive()) {
      continue;
    }

    for (const auto& actionView : gActionViewQueue) {
      xrApp->handleActionView(actionView);
    }
    gActionViewQueue.clear();

    xrApp->update();
  }

  app->activity->vm->DetachCurrentThread();
}
