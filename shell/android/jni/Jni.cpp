/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <jni.h>

#include "TinyRenderer.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <memory>

namespace igl::samples {

namespace {
[[maybe_unused]] std::string toString(std::optional<BackendVersion> backendVersion) {
  if (!backendVersion) {
    return "{}";
  }

  std::string str;
  switch (backendVersion->flavor) {
  case BackendFlavor::Invalid:
    str = "Invalid";
    break;
  case BackendFlavor::OpenGL:
    str = "OpenGL";
    break;
  case BackendFlavor::OpenGL_ES:
    str = "OpenGL_ES";
    break;
  case BackendFlavor::Metal:
    str = "Metal";
    break;
  case BackendFlavor::Vulkan:
    str = "Vulkan";
    break;
  // @fb-only
    // @fb-only
    // @fb-only
  }

  str += " " + std::to_string(static_cast<int>(backendVersion->majorVersion)) + " " +
         std::to_string(static_cast<int>(backendVersion->minorVersion));
  return str;
}

[[maybe_unused]] std::string toString(std::optional<size_t> rendererIndex) {
  if (!rendererIndex) {
    return "{}";
  }

  return std::to_string(*rendererIndex);
}

std::vector<std::unique_ptr<TinyRenderer>> renderers;
std::optional<BackendVersion> activeBackendVersion;

BackendFlavor toBackendFlavor(JNIEnv* env, jobject jbackendVersion) {
  auto* jclass = env->GetObjectClass(jbackendVersion);
  auto* jordinal = env->GetMethodID(jclass, "ordinal", "()I");
  const auto ordinal = env->CallIntMethod(jbackendVersion, jordinal);
  return static_cast<BackendFlavor>(static_cast<int>(ordinal));
}

std::optional<BackendVersion> toBackendVersion(JNIEnv* env, jobject jbackendVersion) {
  if (!jbackendVersion) {
    return {};
  }

  auto* jclass = env->GetObjectClass(jbackendVersion);
  auto* jflavor =
      env->GetFieldID(jclass, "flavor", "Lcom/facebook/igl/shell/SampleLib$BackendFlavor;");
  auto* jmajorVersion = env->GetFieldID(jclass, "majorVersion", "B");
  auto* jminorVersion = env->GetFieldID(jclass, "minorVersion", "B");

  return BackendVersion{toBackendFlavor(env, env->GetObjectField(jbackendVersion, jflavor)),
                        static_cast<uint8_t>(env->GetByteField(jbackendVersion, jmajorVersion)),
                        static_cast<uint8_t>(env->GetByteField(jbackendVersion, jminorVersion))};
}

std::optional<size_t> findRendererIndex(std::optional<BackendVersion> backendVersion) {
  if (!backendVersion) {
    return {};
  }

  for (size_t i = 0; i < renderers.size(); ++i) {
    if (renderers[i]->backendVersion() == backendVersion) {
      return i;
    }
  }

  return {};
}
} // namespace

extern "C" {
JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_init(JNIEnv* env,
                                                                  jobject obj,
                                                                  jobject jbackendVersion,
                                                                  jobject java_asset_manager,
                                                                  jobject surface);
JNIEXPORT jboolean JNICALL
Java_com_facebook_igl_shell_SampleLib_isBackendVersionSupported(JNIEnv* env,
                                                                jobject obj,
                                                                jobject jbackendVersion);
JNIEXPORT void JNICALL
Java_com_facebook_igl_shell_SampleLib_setActiveBackendVersion(JNIEnv* env,
                                                              jobject obj,
                                                              jobject jbackendVersion);
JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_surfaceChanged(JNIEnv* env,
                                                                            jobject obj,
                                                                            jobject surface,
                                                                            jint width,
                                                                            jint height);
JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_render(JNIEnv* env,
                                                                    jobject obj,
                                                                    jfloat displayScale);
JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_surfaceDestroyed(JNIEnv* env,
                                                                              jobject obj,
                                                                              jobject surface);
JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_touchEvent(JNIEnv* env,
                                                                        jobject obj,
                                                                        jboolean isDown,
                                                                        jfloat x,
                                                                        jfloat y,
                                                                        jfloat dx,
                                                                        jfloat dy);
JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_setClearColorValue(JNIEnv* env,
                                                                                jobject obj,
                                                                                jfloat r,
                                                                                jfloat g,
                                                                                jfloat b,
                                                                                jfloat a);
};

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_init(JNIEnv* env,
                                                                  jobject /*obj*/,
                                                                  jobject jbackendVersion,
                                                                  jobject java_asset_manager,
                                                                  jobject surface) {
  const auto backendVersion = toBackendVersion(env, jbackendVersion);
  const auto rendererIndex = findRendererIndex(backendVersion);

  if (backendVersion && !rendererIndex) {
    auto renderer = std::make_unique<TinyRenderer>();
    renderer->init(AAssetManager_fromJava(env, java_asset_manager),
                   surface ? ANativeWindow_fromSurface(env, surface) : nullptr,
                   *backendVersion);
    renderers.emplace_back(std::move(renderer));
    IGL_LOG_INFO("init: creating backend renderer: %s\n", toString(backendVersion).c_str());
  } else if (rendererIndex && backendVersion && backendVersion->flavor == BackendFlavor::Vulkan) {
    IGL_LOG_INFO("init: Updating backend renderer: %s\n", toString(backendVersion).c_str());
    renderers[*rendererIndex]->recreateSwapchain(ANativeWindow_fromSurface(env, surface), true);
  } else {
    IGL_LOG_INFO("init: no changes: %s\n", toString(backendVersion).c_str());
  }

  activeBackendVersion = backendVersion;
}

JNIEXPORT jboolean JNICALL
Java_com_facebook_igl_shell_SampleLib_isBackendVersionSupported(JNIEnv* env,
                                                                jobject /*obj*/,
                                                                jobject jbackendVersion) {
  const auto backendVersion = toBackendVersion(env, jbackendVersion);
  IGL_LOG_INFO("isBackendVersionSupported: %s\n", toString(backendVersion).c_str());
  (void)backendVersion;
#if IGL_BACKEND_OPENGL
  if (backendVersion && backendVersion->flavor == BackendFlavor::OpenGL_ES) {
    return JNI_TRUE;
  }
#endif
#if IGL_BACKEND_VULKAN
  if (backendVersion && backendVersion->flavor == BackendFlavor::Vulkan) {
    return JNI_TRUE;
  }
#endif
  return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_facebook_igl_shell_SampleLib_setActiveBackendVersion(JNIEnv* env,
                                                              jobject /*obj*/,
                                                              jobject jbackendVersion) {
  activeBackendVersion = toBackendVersion(env, jbackendVersion);
  IGL_LOG_INFO("setActiveBackendVersion: %s activeRenderIndex: %s\n",
               toString(*activeBackendVersion).c_str(),
               toString(findRendererIndex(activeBackendVersion)).c_str());
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_surfaceChanged(JNIEnv* env,
                                                                            jobject /*obj*/,
                                                                            jobject surface,
                                                                            jint width,
                                                                            jint height) {
  const auto activeRendererIndex = findRendererIndex(activeBackendVersion);
  IGL_LOG_INFO("surfaceChanged: %s rendererIndex: %s\n",
               toString(activeBackendVersion).c_str(),
               toString(activeRendererIndex).c_str());
  if (!activeRendererIndex) {
    return;
  }

  renderers[*activeRendererIndex]->onSurfacesChanged(
      surface ? ANativeWindow_fromSurface(env, surface) : nullptr, width, height);
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_render(JNIEnv* /*env*/,
                                                                    jobject /*obj*/,
                                                                    jfloat displayScale) {
  const auto activeRendererIndex = findRendererIndex(activeBackendVersion);
  if (!activeRendererIndex) {
    return;
  }

  renderers[*activeRendererIndex]->render(displayScale);
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_surfaceDestroyed(JNIEnv* env,
                                                                              jobject /*obj*/,
                                                                              jobject surface) {}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_touchEvent(JNIEnv* /*env*/,
                                                                        jobject /*obj*/,
                                                                        jboolean isDown,
                                                                        jfloat x,
                                                                        jfloat y,
                                                                        jfloat dx,
                                                                        jfloat dy) {
  const auto activeRendererIndex = findRendererIndex(activeBackendVersion);
  if (!activeRendererIndex) {
    return;
  }

  renderers[*activeRendererIndex]->touchEvent(isDown != 0u, x, y, dx, dy);
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_setClearColorValue(JNIEnv* /*env*/,
                                                                                jobject /*obj*/,
                                                                                jfloat r,
                                                                                jfloat g,
                                                                                jfloat b,
                                                                                jfloat a) {
  const auto activeRendererIndex = findRendererIndex(activeBackendVersion);
  if (!activeRendererIndex) {
    return;
  }

  renderers[*activeRendererIndex]->setClearColorValue(r, g, b, a);
}

} // namespace igl::samples
