/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/Platform.h>

#include <shell/shared/extension/Extension.h>
#include <shell/shared/extension/ExtensionLoader.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/input/InputDispatcher.h>
#include <shell/shared/platform/DisplayContext.h>

namespace {

int gArgc = 0;
char** gArgv = nullptr;
#if IGL_PLATFORM_ANDROID
bool gArgsInitialized = true; // Android has no argc/argv to initialize with
#else
bool gArgsInitialized = false;
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
                                                TextureFormat format,
                                                igl::TextureDesc::TextureUsageBits usage) {
  auto imageData = getImageLoader().loadImageData(filename);

  return loadTexture(imageData, calculateMipmapLevels, format, usage, filename);
}

std::shared_ptr<ITexture> Platform::loadTexture(const ImageData& imageData,
                                                bool calculateMipmapLevels,
                                                TextureFormat format,
                                                igl::TextureDesc::TextureUsageBits usage,
                                                const char* debugName) {
  TextureDesc texDesc =
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
  IGL_DEBUG_ASSERT(gArgsInitialized, "Accessing command line args before they are initialized.");
  return gArgc;
}

char** Platform::argv() {
  IGL_DEBUG_ASSERT(gArgsInitialized, "Accessing command line args before they are initialized.");
  return gArgv;
}

void Platform::initializeCommandLineArgs(int argc, char** argv) {
  IGL_DEBUG_ASSERT(!gArgsInitialized, "Must not initialize command line arguments more than once.");
  gArgc = argc;
  gArgv = argv;
  gArgsInitialized = true;
}

} // namespace igl::shell
