/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Device.h>
#include <igl/opengl/IContext.h>
#include <memory>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/platform/android/PlatformAndroid.h>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/platform/mac/PlatformMac.h>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <shell/shared/testShell/TestShell.h>

namespace igl::shell {

void TestShellBase::SetUp(ScreenSize screenSize) {
  // Create igl device for requested backend
  const std::string backendTypeOption = IGL_BACKEND_TYPE;
  std::unique_ptr<igl::IDevice> iglDevice = nullptr;
  if (backendTypeOption == "ogl") {
    iglDevice = iglu::device::OpenGLFactory::create(igl::opengl::RenderingAPI::GLES3);
    ASSERT_TRUE(iglDevice != nullptr);
#if defined(IGL_PLATFORM_APPLE) && IGL_PLATFORM_APPLE
  } else if (backendTypeOption == "metal") {
    iglDevice = iglu::device::MetalFactory::create();
    ASSERT_TRUE(iglDevice != nullptr);
#endif
  }

  // Create platform shell to run the tests with
#if defined(IGL_PLATFORM_MACOS) && IGL_PLATFORM_MACOS
  platform_ = std::make_shared<igl::shell::PlatformMac>(std::move(iglDevice));
  IGL_ASSERT(platform_);
#elif defined(IGL_PLATFORM_IOS) && IGL_PLATFORM_IOS
  platform_ = std::make_shared<igl::shell::PlatformIos>(std::move(iglDevice));
  IGL_ASSERT(platform_);
#elif defined(IGL_PLATFORM_WIN) && IGL_PLATFORM_WIN
  platform_ = std::make_shared<igl::shell::PlatformWin>(std::move(iglDevice));
  IGL_ASSERT(platform_);
#elif defined(IGL_PLATFORM_ANDROID) && IGL_PLATFORM_ANDROID
  platform_ =
      std::make_shared<igl::shell::PlatformAndroid>(std::move(iglDevice), true /*useFakeLoader*/);
  IGL_ASSERT(platform_);
#endif

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
};

void TestShell::run(igl::shell::RenderSession& session, size_t numFrames) {
  ShellParams shellParams;
  session.setShellParams(shellParams);
  session.initialize();
  for (size_t i = 0; i < numFrames; ++i) {
    session.update({offscreenTexture_, offscreenDepthTexture_});
  }
  session.teardown();
}

} // namespace igl::shell
