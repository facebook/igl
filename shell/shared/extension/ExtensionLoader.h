/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

namespace igl::shell {

class Extension;
class Platform;

/**
 * @brief Loads extensions using platform-specific symbol lookup utilities
 */
class ExtensionLoader {
 public:
  static const char kDefaultPrefix[];

  ExtensionLoader();

  explicit ExtensionLoader(std::string prefix);

  ~ExtensionLoader() = default;

  /**
   * @brief Creates Extension. Does not initialize.
   *
   * @return Returns new allocation of Extension. Not initialized.
   *
   * @note Relies on a function with signature IGLShellExtension_NewCFunction that instantiates
   * the extension. The name of this C function is: <prefix> + name.
   *
   * @see IGLShellExtension_NewCFunction
   */
  // E is the Extension type. Requirements on E
  // 1. The static method `const char* E::Name() noexcept` must exist
  // 2. E must subclass from igl::shell::Extension
  Extension* create(const std::string& name) noexcept;

  /**
   * @brief Creates Extension and initializes it
   *
   * @return Returns new instance of Extension (i.e. alloc'd and init'd)
   *
   * @see ExtensionLoader::create()
   */
  Extension* createAndInitialize(const std::string& name, Platform& platform) noexcept;

 private:
  std::string prefix_;
};

} // namespace igl::shell
