/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Platform.h"

#include <shell/shared/extension/Extension.h>
#include <shell/shared/extension/ExtensionLoader.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/input/InputDispatcher.h>
#include <shell/shared/platform/DisplayContext.h>

namespace {

int g_argc = 0;
char** g_argv = nullptr;
#if IGL_PLATFORM_ANDROID
bool g_argsInitialized = true; // Android has no argc/argv to initialize with
#else
bool g_argsInitialized = false;
#endif

} // namespace

namespace igl::shell {

struct Platform::State {
  ExtensionLoader extensionLoader;
  InputDispatcher inputDispatcher;
  DisplayContext displayContext;
};

Platform::Platform() noexcept : state_(std::make_unique<State>()) {}

Platform::~Platform() = default;

Extension* Platform::createAndInitializeExtension(const char* name) noexcept {
  return state_->extensionLoader.createAndInitialize(name, *this);
}

InputDispatcher& Platform::getInputDispatcher() noexcept {
  return state_->inputDispatcher;
}

[[nodiscard]] DisplayContext& Platform::getDisplayContext() noexcept {
  return state_->displayContext;
}

std::shared_ptr<ITexture> Platform::loadTexture(const char* filename,
                                                bool calculateMipmapLevels,
                                                igl::TextureFormat format,
                                                igl::TextureDesc::TextureUsageBits usage) {
  auto imageData = getImageLoader().loadImageData(filename);

  return loadTexture(imageData, calculateMipmapLevels, format, usage, filename);
}

std::shared_ptr<ITexture> Platform::loadTexture(const ImageData& imageData,
                                                bool calculateMipmapLevels,
                                                igl::TextureFormat format,
                                                igl::TextureDesc::TextureUsageBits usage,
                                                const char* debugName) {
  igl::TextureDesc texDesc =
      igl::TextureDesc::new2D(format, imageData.desc.width, imageData.desc.height, usage);
  texDesc.numMipLevels =
      calculateMipmapLevels ? igl::TextureDesc::calcNumMipLevels(texDesc.width, texDesc.height) : 1;
  texDesc.debugName = debugName;

  Result res;
  auto tex = getDevice().createTexture(texDesc, &res);
  IGL_DEBUG_ASSERT(res.isOk(), res.message.c_str());
  IGL_DEBUG_ASSERT(tex != nullptr, "createTexture returned null for some reason");
  tex->upload(tex->getFullRange(), imageData.data->data());
  return tex;
}

int Platform::argc() {
  IGL_DEBUG_ASSERT(g_argsInitialized, "Accessing command line args before they are initialized.");
  return g_argc;
}

char** Platform::argv() {
  IGL_DEBUG_ASSERT(g_argsInitialized, "Accessing command line args before they are initialized.");
  return g_argv;
}

void Platform::initializeCommandLineArgs(int argc, char** argv) {
  IGL_DEBUG_ASSERT(!g_argsInitialized,
                   "Must not initialize command line arguments more than once.");
  g_argc = argc;
  g_argv = argv;
  g_argsInitialized = true;
}

} // namespace igl::shell
