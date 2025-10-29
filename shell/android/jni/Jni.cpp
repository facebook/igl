/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "TinyRenderer.h"

#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <memory>
#include <shell/shared/renderSession/DefaultRenderSessionFactory.h>
#include <shell/shared/renderSession/IRenderSessionFactory.h>
#include <igl/Common.h>
#include <igl/TextureFormat.h>

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

std::unique_ptr<shell::IRenderSessionFactory> factory;
std::vector<std::unique_ptr<TinyRenderer>> renderers;
std::optional<BackendVersion> activeBackendVersion;

constexpr auto* kBackendFlavorClassName = "com/facebook/igl/shell/SampleLib$BackendFlavor";
constexpr auto* kBackendVersionClassName = "com/facebook/igl/shell/SampleLib$BackendVersion";
constexpr auto* kRenderSessionConfigClassName =
    "com/facebook/igl/shell/SampleLib$RenderSessionConfig";

std::string toTypeSignature(const char* className) {
  return std::string("L") + className + std::string(";");
}

BackendFlavor toBackendFlavor(JNIEnv* env, jobject jbackendVersion) {
  auto* jclass = env->GetObjectClass(jbackendVersion);
  auto* jordinal = env->GetMethodID(jclass, "ordinal", "()I");
  const auto ordinal = env->CallIntMethod(jbackendVersion, jordinal);
  return static_cast<BackendFlavor>(static_cast<int>(ordinal));
}

jobject toJava(JNIEnv* env, BackendFlavor backendFlavor) {
  jclass jclass = env->FindClass(kBackendFlavorClassName);
  const std::string returnType = std::string("()[") + toTypeSignature(kBackendFlavorClassName);
  jmethodID values = env->GetStaticMethodID(jclass, "values", returnType.c_str());
  auto* backendFlavorValues = (jobjectArray)env->CallStaticObjectMethod(jclass, values);

  jobject backendFlavorValue =
      env->GetObjectArrayElement(backendFlavorValues, static_cast<int>(backendFlavor));

  env->DeleteLocalRef(backendFlavorValues);
  return backendFlavorValue;
}

std::optional<BackendVersion> toBackendVersion(JNIEnv* env, jobject jbackendVersion) {
  if (!jbackendVersion) {
    return {};
  }

  auto* jclass = env->GetObjectClass(jbackendVersion);
  auto* jflavor =
      env->GetFieldID(jclass, "flavor", toTypeSignature(kBackendFlavorClassName).c_str());
  auto* jmajorVersion = env->GetFieldID(jclass, "majorVersion", "B");
  auto* jminorVersion = env->GetFieldID(jclass, "minorVersion", "B");

  return BackendVersion{
      .flavor = toBackendFlavor(env, env->GetObjectField(jbackendVersion, jflavor)),
      .majorVersion = static_cast<uint8_t>(env->GetByteField(jbackendVersion, jmajorVersion)),
      .minorVersion = static_cast<uint8_t>(env->GetByteField(jbackendVersion, jminorVersion))};
}

jobject toJava(JNIEnv* env, BackendVersion backendVersion) {
  jclass jclass = env->FindClass(kBackendVersionClassName);
  const std::string methodSignature =
      std::string("(") + toTypeSignature(kBackendFlavorClassName) + "BB)V";
  jmethodID constructor = env->GetMethodID(jclass, "<init>", methodSignature.c_str());

  jobject jbackendFlavor = toJava(env, backendVersion.flavor);
  jobject ret = env->NewObject(jclass,
                               constructor,
                               jbackendFlavor,
                               backendVersion.majorVersion,
                               backendVersion.minorVersion);
  env->DeleteLocalRef(jbackendFlavor);
  return ret;
}

jobject toJava(JNIEnv* env, const shell::RenderSessionConfig& config) {
  jclass jclass = env->FindClass(kRenderSessionConfigClassName);
  const std::string methodSignature =
      std::string("(Ljava/lang/String;") + toTypeSignature(kBackendVersionClassName) + "I)V";
  jmethodID constructor = env->GetMethodID(jclass, "<init>", methodSignature.c_str());

  jstring jdisplayName = env->NewStringUTF(config.displayName.c_str());
  jobject jbackendVersion = toJava(env, config.backendVersion);
  const jint jswapchainColorTextureFormat = static_cast<int>(config.swapchainColorTextureFormat);
  jobject ret = env->NewObject(
      jclass, constructor, jdisplayName, jbackendVersion, jswapchainColorTextureFormat);
  env->DeleteLocalRef(jdisplayName);
  env->DeleteLocalRef(jbackendVersion);
  return ret;
}

jobjectArray toJava(JNIEnv* env, const std::vector<shell::RenderSessionConfig>& configs) {
  jobjectArray ret = nullptr;
  auto* jclass = env->FindClass(kRenderSessionConfigClassName);
  ret = env->NewObjectArray(configs.size(), jclass, nullptr);
  for (size_t i = 0; i < configs.size(); ++i) {
    env->SetObjectArrayElement(ret, i, toJava(env, configs[i]));
  }

  return ret;
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
JNIEXPORT jobjectArray JNICALL
Java_com_facebook_igl_shell_SampleLib_getRenderSessionConfigs(JNIEnv* env, jobject obj);
JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_init(JNIEnv* env,
                                                                  jobject obj,
                                                                  jobject jbackendVersion,
                                                                  jint jswapchainColorTextureFormat,
                                                                  jobject javaAssetManager,
                                                                  jobject surface,
                                                                  jobject intent);
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

JNIEXPORT bool JNICALL Java_com_facebook_igl_shell_SampleLib_isSRGBTextureFormat(JNIEnv* env,
                                                                                 jobject obj,
                                                                                 int textureFormat);
};

JNIEXPORT jobjectArray JNICALL
Java_com_facebook_igl_shell_SampleLib_getRenderSessionConfigs(JNIEnv* env, jobject /*obj*/) {
  if (!factory) {
    factory = shell::createDefaultRenderSessionFactory();
  }

  constexpr igl::TextureFormat kSwapchainColorTextureFormat = igl::TextureFormat::BGRA_SRGB;
  std::vector<igl::shell::RenderSessionConfig> suggestedConfigs = {
#if IGL_BACKEND_OPENGL
      {
          .displayName = "OpenGL ES 3",
          .backendVersion = {.flavor = igl::BackendFlavor::OpenGL_ES,
                             .majorVersion = 3,
                             .minorVersion = 0},
          .swapchainColorTextureFormat = kSwapchainColorTextureFormat,
      },
      {
          .displayName = "OpenGL ES 2",
          .backendVersion = {.flavor = igl::BackendFlavor::OpenGL_ES,
                             .majorVersion = 2,
                             .minorVersion = 0},
          .swapchainColorTextureFormat = kSwapchainColorTextureFormat,
      },
#endif
#if IGL_BACKEND_VULKAN
      {
          .displayName = "Vulkan",
          .backendVersion = {.flavor = igl::BackendFlavor::Vulkan,
                             .majorVersion = 1,
                             .minorVersion = 1},
          .swapchainColorTextureFormat = kSwapchainColorTextureFormat,
      },
#endif
  };

  const auto requestedConfigs =
      factory->requestedSessionConfigs(shell::ShellType::Android, std::move(suggestedConfigs));
  return toJava(env, requestedConfigs);
}

// Helper function to extract all Intent extras as command-line style arguments
[[maybe_unused]] static std::vector<std::string> extractIntentExtras(JNIEnv* env, jobject intent) {
  std::vector<std::string> extras;
  if (!intent) {
    return extras;
  }

  // Get Intent class and getExtras method
  jclass intentClass = env->GetObjectClass(intent);
  jmethodID getExtrasMethod = env->GetMethodID(intentClass, "getExtras", "()Landroid/os/Bundle;");

  if (!getExtrasMethod) {
    IGL_LOG_ERROR("Failed to get getExtras method\n");
    return extras;
  }

  // Get the Bundle containing extras
  jobject bundle = env->CallObjectMethod(intent, getExtrasMethod);
  if (!bundle) {
    IGL_LOG_INFO("No extras found in Intent\n");
    return extras;
  }

  // Get Bundle class and keySet method
  jclass bundleClass = env->GetObjectClass(bundle);
  jmethodID keySetMethod = env->GetMethodID(bundleClass, "keySet", "()Ljava/util/Set;");

  if (!keySetMethod) {
    IGL_LOG_ERROR("Failed to get keySet method\n");
    env->DeleteLocalRef(bundle);
    return extras;
  }

  // Get the Set of keys
  jobject keySet = env->CallObjectMethod(bundle, keySetMethod);
  if (!keySet) {
    IGL_LOG_INFO("No keys found in Bundle\n");
    env->DeleteLocalRef(bundle);
    return extras;
  }

  // Get Set class and iterator method
  jclass setClass = env->GetObjectClass(keySet);
  jmethodID iteratorMethod = env->GetMethodID(setClass, "iterator", "()Ljava/util/Iterator;");

  if (!iteratorMethod) {
    IGL_LOG_ERROR("Failed to get iterator method\n");
    env->DeleteLocalRef(keySet);
    env->DeleteLocalRef(bundle);
    return extras;
  }

  // Get the Iterator
  jobject iterator = env->CallObjectMethod(keySet, iteratorMethod);
  if (!iterator) {
    IGL_LOG_ERROR("Failed to get iterator\n");
    env->DeleteLocalRef(keySet);
    env->DeleteLocalRef(bundle);
    return extras;
  }

  // Get Iterator class methods
  jclass iteratorClass = env->GetObjectClass(iterator);
  jmethodID hasNextMethod = env->GetMethodID(iteratorClass, "hasNext", "()Z");
  jmethodID nextMethod = env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");

  if (!hasNextMethod || !nextMethod) {
    IGL_LOG_ERROR("Failed to get iterator methods\n");
    env->DeleteLocalRef(iterator);
    env->DeleteLocalRef(keySet);
    env->DeleteLocalRef(bundle);
    return extras;
  }

  // Get Bundle.get method to retrieve values
  jmethodID getMethod =
      env->GetMethodID(bundleClass, "get", "(Ljava/lang/String;)Ljava/lang/Object;");
  if (!getMethod) {
    IGL_LOG_ERROR("Failed to get Bundle.get method\n");
    env->DeleteLocalRef(iterator);
    env->DeleteLocalRef(keySet);
    env->DeleteLocalRef(bundle);
    return extras;
  }

  // Iterate through all keys
  while (env->CallBooleanMethod(iterator, hasNextMethod)) {
    jobject keyObj = env->CallObjectMethod(iterator, nextMethod);
    if (!keyObj) {
      continue;
    }

    // Convert key to string
    jstring keyStr = static_cast<jstring>(keyObj);
    const char* keyChars = env->GetStringUTFChars(keyStr, nullptr);
    std::string key(keyChars);
    env->ReleaseStringUTFChars(keyStr, keyChars);

    // Get the value for this key
    jobject valueObj = env->CallObjectMethod(bundle, getMethod, keyStr);
    std::string value;

    if (valueObj) {
      // Convert value to string (works for most common types)
      jclass objectClass = env->GetObjectClass(valueObj);
      jmethodID toStringMethod = env->GetMethodID(objectClass, "toString", "()Ljava/lang/String;");

      if (toStringMethod) {
        jstring valueStr = static_cast<jstring>(env->CallObjectMethod(valueObj, toStringMethod));
        if (valueStr) {
          const char* valueChars = env->GetStringUTFChars(valueStr, nullptr);
          value = std::string(valueChars);
          env->ReleaseStringUTFChars(valueStr, valueChars);
          env->DeleteLocalRef(valueStr);
        }
      }
      env->DeleteLocalRef(valueObj);
    } else {
      value = "null";
    }

    // Add the key as a command-line argument (with -- prefix)
    extras.emplace_back(key);
    // Add the value as a separate argument if it's not empty and not "null"
    if (!value.empty() && value != "null") {
      extras.emplace_back(value);
    }
    IGL_LOG_INFO("Intent extra: %s = %s\n", key.c_str(), value.c_str());

    env->DeleteLocalRef(keyObj);
  }

  // Clean up local references
  env->DeleteLocalRef(iterator);
  env->DeleteLocalRef(keySet);
  env->DeleteLocalRef(bundle);

  return extras;
}

JNIEXPORT void JNICALL Java_com_facebook_igl_shell_SampleLib_init(JNIEnv* env,
                                                                  jobject /*obj*/,
                                                                  jobject jbackendVersion,
                                                                  jint jtextureFormat,
                                                                  jobject javaAssetManager,
                                                                  jobject surface,
                                                                  jobject intent) {
  const auto backendVersion = toBackendVersion(env, jbackendVersion);
  const auto swapchainColorTextureFormat = static_cast<TextureFormat>(jtextureFormat);
  const auto rendererIndex = findRendererIndex(backendVersion);

  if (backendVersion && !rendererIndex) {
    auto renderer = std::make_unique<TinyRenderer>();
    auto cmdLine = extractIntentExtras(env, intent);
    IGL_LOG_INFO("init: creating backend renderer cmd line: %d\n", cmdLine.size());
    for (const auto& cmd : cmdLine) {
      IGL_LOG_INFO("Param: %s\n", cmd.c_str());
    }
    renderer->init(AAssetManager_fromJava(env, javaAssetManager),
                   surface ? ANativeWindow_fromSurface(env, surface) : nullptr,
                   *factory,
                   *backendVersion,
                   swapchainColorTextureFormat,
                   cmdLine);
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

// NOLINTBEGIN(misc-use-internal-linkage)
JNIEXPORT jboolean JNICALL
Java_com_facebook_igl_shell_SampleLib_isBackendVersionSupported(JNIEnv* env,
                                                                jobject /*obj*/,
                                                                jobject jbackendVersion) {
  [[maybe_unused]] const auto backendVersion = toBackendVersion(env, jbackendVersion);
  IGL_LOG_INFO("isBackendVersionSupported: %s\n", toString(backendVersion).c_str());
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
// NOLINTEND(misc-use-internal-linkage)

JNIEXPORT void JNICALL
Java_com_facebook_igl_shell_SampleLib_setActiveBackendVersion(JNIEnv* env,
                                                              jobject /*obj*/,
                                                              jobject jbackendVersion) {
  activeBackendVersion = toBackendVersion(env, jbackendVersion);
  IGL_LOG_INFO("setActiveBackendVersion: %s activeRenderIndex: %s\n",
               toString(activeBackendVersion).c_str(),
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

JNIEXPORT bool JNICALL
Java_com_facebook_igl_shell_SampleLib_isSRGBTextureFormat(JNIEnv* env,
                                                          jobject obj,
                                                          int textureFormat) {
  return textureFormat == (int)igl::TextureFormat::RGBA_SRGB ||
         textureFormat == (int)igl::TextureFormat::BGRA_SRGB;
}

} // namespace igl::samples
