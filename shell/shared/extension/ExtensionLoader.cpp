/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/extension/ExtensionLoader.h>

#include <igl/Core.h>
#include <shell/shared/extension/Extension.h>
#include <shell/shared/extension/SymbolFactoryLoader.h>

namespace igl::shell {

const char ExtensionLoader::kDefaultPrefix[] = "IGLShellExtension_New";

ExtensionLoader::ExtensionLoader() : ExtensionLoader(kDefaultPrefix) {}

ExtensionLoader::ExtensionLoader(std::string prefix) : prefix_(std::move(prefix)) {}

Extension* ExtensionLoader::create(const std::string& name) noexcept {
  IGL_DEBUG_ASSERT(name.length() > 0);

  const std::string symbolName = prefix_ + name;
  auto factoryFunc = SymbolFactoryLoader::find(symbolName);

  return factoryFunc ? static_cast<Extension*>((*factoryFunc)()) : nullptr;
}

Extension* ExtensionLoader::createAndInitialize(const std::string& name,
                                                Platform& platform) noexcept {
  auto* extension = create(name);
  if (extension) {
    extension->initialize(platform); // initialize extension
  } else {
    IGL_LOG_ERROR_ONCE("igl::shell::ExtensionLoader() Could not create extension(%s)\n",
                       name.c_str());
  }
  return extension;
}

} // namespace igl::shell
