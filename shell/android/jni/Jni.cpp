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
std::unordered_map<BackendTypeID, std::unique_ptr<TinyRenderer>> renderers;
BackendTypeID activeBackendTypeID = BackendTypeID::GLES3; // default backend type is GLES3
} // namespace

extern "C" {
JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_init(JNIEnv* env,
                                                                  jobject obj,
                                                                  jobject java_asset_manager,
                                                                  jobject surface);
JNIEXPORT jboolean JNICALL
Java_com_facebook_igl_shell_SampleLib_isBackendTypeIDSupported(JNIEnv* env,
                                                               jobject obj,
                                                               jint backendTypeID);
JNIEXPORT void JNICALL
Java_com_facebook_igl_shell_SampleLib_setActiveBackendTypeID(JNIEnv* env,
                                                             jobject obj,
                                                             jint backendTypeID);
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
                                                                  jobject java_asset_manager,
                                                                  jobject surface) {
  if (!renderers[activeBackendTypeID]) {
    renderers[activeBackendTypeID] = std::make_unique<TinyRenderer>();
    renderers[activeBackendTypeID]->init(AAssetManager_fromJava(env, java_asset_manager),
                                         surface ? ANativeWindow_fromSurface(env, surface)
                                                 : nullptr,
                                         activeBackendTypeID);
  }
}

JNIEXPORT jboolean JNICALL
Java_com_facebook_igl_shell_SampleLib_isBackendTypeIDSupported(JNIEnv* /*env*/,
                                                               jobject /*obj*/,
                                                               jint backendTypeID) {
  const auto backend = (BackendTypeID)backendTypeID;
  (void)backend;
#if IGL_BACKEND_OPENGL
  if (backend == BackendTypeID::GLES2 || backend == BackendTypeID::GLES3) {
    return JNI_TRUE;
  }
#endif
#if IGL_BACKEND_VULKAN
  if (backend == BackendTypeID::Vulkan) {
    return JNI_TRUE;
  }
#endif
  return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_facebook_igl_shell_SampleLib_setActiveBackendTypeID(JNIEnv* /*env*/,
                                                             jobject /*obj*/,
                                                             jint backendTypeID) {
  activeBackendTypeID = (BackendTypeID)backendTypeID;
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_surfaceChanged(JNIEnv* env,
                                                                            jobject /*obj*/,
                                                                            jobject surface,
                                                                            jint width,
                                                                            jint height) {
  renderers[activeBackendTypeID]->onSurfacesChanged(
      surface ? ANativeWindow_fromSurface(env, surface) : nullptr, width, height);
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_render(JNIEnv* /*env*/,
                                                                    jobject /*obj*/,
                                                                    jfloat displayScale) {
  if (renderers[activeBackendTypeID] != nullptr) {
    renderers[activeBackendTypeID]->render(displayScale);
  }
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_surfaceDestroyed(JNIEnv* env,
                                                                              jobject /*obj*/,
                                                                              jobject surface) {
  // For Vulkan we deallocate the whole renderer and recreate it when surface is re-created.
  // This is done because we currently don't have an easy way to destroy the surface + swapchain
  // independent of the whole device.
  // This is not needed for GL
  if (activeBackendTypeID == BackendTypeID::Vulkan && renderers[activeBackendTypeID] != nullptr) {
    renderers[activeBackendTypeID]->onSurfaceDestroyed(
        surface ? ANativeWindow_fromSurface(env, surface) : nullptr);
    auto& renderer = renderers[activeBackendTypeID];
    renderer.reset();
    renderers[activeBackendTypeID] = nullptr;
  }
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_touchEvent(JNIEnv* /*env*/,
                                                                        jobject /*obj*/,
                                                                        jboolean isDown,
                                                                        jfloat x,
                                                                        jfloat y,
                                                                        jfloat dx,
                                                                        jfloat dy) {
  if (renderers[activeBackendTypeID] != nullptr) {
    renderers[activeBackendTypeID]->touchEvent(isDown != 0u, x, y, dx, dy);
  }
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_setClearColorValue(JNIEnv* /*env*/,
                                                                                jobject /*obj*/,
                                                                                jfloat r,
                                                                                jfloat g,
                                                                                jfloat b,
                                                                                jfloat a) {
  if (renderers[activeBackendTypeID] != nullptr) {
    renderers[activeBackendTypeID]->setClearColorValue(r, g, b, a);
  }
}

} // namespace igl::samples
