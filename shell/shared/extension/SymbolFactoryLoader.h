/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

/**
 * @brief C-function signature for factory that instantiates instance of symbol
 */
using IGLShellSymbol_NewCFunction = void* (*)();

namespace igl::shell {

struct SymbolFactoryLoader {
  /**
   * @brief Finds a factory function that creates symbol corresponding to `name`
   *
   * @return C function if symbol exists; nullptr otherwise.
   */
  static IGLShellSymbol_NewCFunction find(const char* name) noexcept;
  static IGLShellSymbol_NewCFunction find(const std::string& name) noexcept;
};

} // namespace igl::shell
