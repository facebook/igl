/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/Factory.h>

#if IGL_PLATFORM_ANDROID
#include <shell/shared/platform/android/PlatformAndroid.h>
#elif IGL_PLATFORM_LINUX
#include <shell/shared/platform/linux/PlatformLinux.h>
#elif IGL_PLATFORM_MACOS
#include <shell/shared/platform/mac/PlatformMac.h>
#elif IGL_PLATFORM_WINDOWS
#include <shell/shared/platform/win/PlatformWin.h>
#elif IGL_PLATFORM_IOS
#include <shell/shared/platform/ios/PlatformIos.h>
#endif

namespace igl::shell {

std::shared_ptr<Platform> createPlatform(std::shared_ptr<IDevice> device) {
#if IGL_PLATFORM_ANDROID
  return std::make_shared<PlatformAndroid>(std::move(device));
#elif IGL_PLATFORM_LINUX
  return std::make_shared<PlatformLinux>(std::move(device));
#elif IGL_PLATFORM_MACOS
  return std::make_shared<PlatformMac>(std::move(device));
#elif IGL_PLATFORM_WINDOWS
  return std::make_shared<PlatformWin>(std::move(device));
#elif IGL_PLATFORM_IOS
  return std::make_shared<PlatformIos>(std::move(device));
#else
  return nullptr;
#endif
}

} // namespace igl::shell
