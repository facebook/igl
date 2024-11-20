/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/TestDevice.h>
#include <memory>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/platform/android/PlatformAndroid.h>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/platform/linux/PlatformLinux.h>
#include <shell/shared/platform/mac/PlatformMac.h>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <shell/shared/testShell/TestShell.h>

namespace igl::shell {

namespace {

std::shared_ptr<::igl::IDevice> createTestDevice() {
  const std::string backend(IGL_BACKEND_TYPE);

  if (backend == "ogl") {
#ifdef IGL_UNIT_TESTS_GLES_VERSION
    std::string backendApi(IGL_UNIT_TESTS_GLES_VERSION);
#else
    const std::string backendApi("3.0es");
#endif
    return tests::util::device::createTestDevice(::igl::BackendType::OpenGL, backendApi);
  } else if (backend == "metal") {
    return tests::util::device::createTestDevice(::igl::BackendType::Metal);
  } else if (backend == "vulkan") {
    return tests::util::device::createTestDevice(::igl::BackendType::Vulkan);
  // @fb-only
    // @fb-only
  // @fb-only
    return nullptr;
  }
}

void ensureCommandLineArgsInitialized() {
  // Fake initialization of command line args so sessions don't assert when accessing them.
  // Only do it once, otherwise it triggers an internal assert.

#if IGL_PLATFORM_ANDROID
  static bool s_initialized = true; // Android prohibids initialization of command line args
#else
  static bool s_initialized = false;
#endif
  if (!s_initialized) {
    s_initialized = true;
    igl::shell::Platform::initializeCommandLineArgs(0, nullptr);
  }
}

} // namespace

void TestShellBase::SetUp(ScreenSize screenSize) {
  ensureCommandLineArgsInitialized();

  // Create igl device for requested backend
  std::shared_ptr<igl::IDevice> iglDevice = createTestDevice();
  ASSERT_TRUE(iglDevice != nullptr);
  // Create platform shell to run the tests with
#if defined(IGL_PLATFORM_MACOS) && IGL_PLATFORM_MACOS
  platform_ = std::make_shared<igl::shell::PlatformMac>(std::move(iglDevice));
#elif defined(IGL_PLATFORM_IOS) && IGL_PLATFORM_IOS
  platform_ = std::make_shared<igl::shell::PlatformIos>(std::move(iglDevice));
#elif defined(IGL_PLATFORM_WIN) && IGL_PLATFORM_WIN
  platform_ = std::make_shared<igl::shell::PlatformWin>(std::move(iglDevice));
#elif defined(IGL_PLATFORM_ANDROID) && IGL_PLATFORM_ANDROID
  platform_ = std::make_shared<igl::shell::PlatformAndroid>(std::move(iglDevice));
#elif defined(IGL_PLATFORM_LINUX) && IGL_PLATFORM_LINUX
  platform_ = std::make_shared<igl::shell::PlatformLinux>(std::move(iglDevice));
#endif

  IGL_DEBUG_ASSERT(platform_);

  if (platform_->getDevice().getBackendType() == igl::BackendType::OpenGL) {
    auto version = platform_->getDevice().getBackendVersion();
    if (version.majorVersion < 2) {
      GTEST_SKIP() << "OpenGL version " << (int)version.majorVersion << "."
                   << (int)version.minorVersion << " is too low";
    }
  }
  // Create an offscreen texture to render to
  igl::Result ret;
  igl::TextureDesc texDesc = igl::TextureDesc::new2D(
      platform_->getDevice().getBackendType() == igl::BackendType::Metal
          ? igl::TextureFormat::BGRA_SRGB
          : igl::TextureFormat::RGBA_SRGB,
      screenSize.width,
      screenSize.height,
      igl::TextureDesc::TextureUsageBits::Sampled | igl::TextureDesc::TextureUsageBits::Attachment);
  offscreenTexture_ = platform_->getDevice().createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(offscreenTexture_ != nullptr);
  igl::TextureDesc depthDextureDesc = igl::TextureDesc::new2D(
      igl::TextureFormat::Z_UNorm24,
      screenSize.width,
      screenSize.height,
      igl::TextureDesc::TextureUsageBits::Sampled | igl::TextureDesc::TextureUsageBits::Attachment);
  depthDextureDesc.storage = igl::ResourceStorage::Private;
  offscreenDepthTexture_ = platform_->getDevice().createTexture(depthDextureDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(offscreenDepthTexture_ != nullptr);
}

void TestShell::run(igl::shell::RenderSession& session, size_t numFrames) {
  ShellParams shellParams;
  session.setShellParams(shellParams);
  session.initialize();
  for (size_t i = 0; i < numFrames; ++i) {
    const igl::DeviceScope scope(platform_->getDevice());
    session.update({offscreenTexture_, offscreenDepthTexture_});
  }
  session.teardown();
}

} // namespace igl::shell
