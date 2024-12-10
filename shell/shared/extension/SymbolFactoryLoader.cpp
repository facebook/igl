/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/extension/SymbolFactoryLoader.h>

#include <igl/Core.h>

// clang-format off
// Default behavior if project doesn't override
#if !defined(IGL_DL_UNIX) && !defined(IGL_DL_DLL)
  #if (defined(IGL_PLATFORM_APPLE) && IGL_PLATFORM_APPLE)
    #define IGL_DL_UNIX 1
  #elif (defined(IGL_PLATFORM_ANDROID) && IGL_PLATFORM_ANDROID)
    #define IGL_DL_UNIX 1
  #elif (defined(IGL_PLATFORM_WINDOWS) && IGL_PLATFORM_WINDOWS) && !defined(IGL_PLATFORM_UWP)
    #define IGL_DL_DLL 1
  #endif
#endif // !defined(IGL_DL_UNIX) || !defined(IGL_DL_DLL)
// clang-format on

#if IGL_DL_UNIX
#include <dlfcn.h>
#elif IGL_DL_DLL
#include <windows.h>
#endif

namespace igl::shell {

IGLShellSymbol_NewCFunction SymbolFactoryLoader::find(const char* name) noexcept {
#if IGL_DL_UNIX
  auto factoryFunc = (IGLShellSymbol_NewCFunction)dlsym(RTLD_DEFAULT, name);
#elif IGL_DL_DLL
  auto factoryFunc = (IGLShellSymbol_NewCFunction)GetProcAddress(GetModuleHandle(NULL), name);
#else
  IGL_LOG_ERROR("IGL WARNING: Runtime symbol lookup *not* supported on this platform\n");
  auto factoryFunc = (IGLShellSymbol_NewCFunction) nullptr;
#endif

  if (!factoryFunc) {
    IGL_LOG_ERROR_ONCE("SymbolFactoryLoader::find() Could not load symbol(%s)\n", name);
  }

  return factoryFunc;
}

IGLShellSymbol_NewCFunction SymbolFactoryLoader::find(const std::string& name) noexcept {
  return find(name.c_str());
}

} // namespace igl::shell
