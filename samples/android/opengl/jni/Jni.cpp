/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <jni.h>

#include <igl/samples/android/opengl/TinyRenderer.h>
#include <memory>

namespace igl_samples::android {

namespace {
std::unique_ptr<TinyRenderer> renderer;
} // namespace

extern "C" {
JNIEXPORT void JNICALL Java_com_facebook_igl_sample_opengl_SampleLib_init(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL Java_com_facebook_igl_sample_opengl_SampleLib_surfaceChanged(JNIEnv* env,
                                                                                    jobject obj);
JNIEXPORT void JNICALL Java_com_facebook_igl_sample_opengl_SampleLib_render(JNIEnv* env,
                                                                            jobject obj);
};

JNIEXPORT void JNICALL Java_com_facebook_igl_sample_opengl_SampleLib_init(JNIEnv* /*env*/,
                                                                          jobject /*obj*/) {
  renderer = std::make_unique<TinyRenderer>();
  renderer->init();
}

JNIEXPORT void JNICALL
Java_com_facebook_igl_sample_opengl_SampleLib_surfaceChanged(JNIEnv* /*env*/, jobject /*obj*/) {
  renderer->onSurfacesChanged();
}

JNIEXPORT void JNICALL Java_com_facebook_igl_sample_opengl_SampleLib_render(JNIEnv* /*env*/,
                                                                            jobject /*obj*/) {
  if (renderer != nullptr) {
    renderer->render();
  }
}

} // namespace igl_samples::android
