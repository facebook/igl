/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <igl/Macros.h>

#if defined(FORCE_USE_ANGLE) || IGL_PLATFORM_LINUX
#include <igl/opengl/egl/HWDevice.h>
#else
#include <igl/opengl/wgl/HWDevice.h>
#endif // FORCE_USE_ANGLE

#include <memory>

namespace igl::shell::util {
template<typename THWDevice>
std::shared_ptr<::igl::opengl::Device> createDevice(igl::opengl::RenderingAPI renderingAPI) {
  THWDevice hwDevice;
  auto context = hwDevice.createContext(renderingAPI, IGL_EGL_NULL_WINDOW, nullptr);
  return hwDevice.createWithContext(std::move(context), nullptr);
}
template<typename THWDevice>
std::shared_ptr<::igl::opengl::Device> createOffscreenDevice(
    igl::opengl::RenderingAPI renderingAPI) {
  THWDevice hwDevice;
  auto context = hwDevice.createOffscreenContext(renderingAPI, 640, 380, nullptr);
  return hwDevice.createWithContext(std::move(context), nullptr);
}

//
// createTestDevice
//
// Used by clients to get an IGL device. The backend is determined by
// the IGL_BACKEND_TYPE compiler flag in the BUCK file
//
std::shared_ptr<::igl::IDevice> createTestDevice() {
  std::shared_ptr<igl::IDevice> iglDev = nullptr;
  auto renderingAPI = igl::opengl::RenderingAPI::GL;

#if defined(FORCE_USE_ANGLE) || IGL_PLATFORM_LINUX
  iglDev = createOffscreenDevice<::igl::opengl::egl::HWDevice>(renderingAPI);
#else
  iglDev = createOffscreenDevice<::igl::opengl::wgl::HWDevice>(renderingAPI);
#endif // FORCE_USE_ANGLE

  return std::static_pointer_cast<igl::IDevice>(iglDev);
}

void createTestDeviceAndQueue(std::shared_ptr<IDevice>& dev, std::shared_ptr<ICommandQueue>& cq) {
  Result ret;

  // Create Device
  dev = createTestDevice();
  IGL_ASSERT(dev != nullptr);

  // Create Command Queue
  CommandQueueDesc cqDesc = {CommandQueueType::Graphics};
  cq = dev->createCommandQueue(cqDesc, &ret);

  IGL_ASSERT(ret.code == Result::Code::Ok);
  IGL_ASSERT(cq != nullptr); // Shouldn't trigger if above is okay
}

igl::SurfaceTextures createSurfaceTextures(igl::IDevice& device,
                                           size_t width,
                                           size_t height,
                                           igl::TextureFormat format) {
  if (IGL_VERIFY(device.getBackendType() == igl::BackendType::OpenGL)) {
    igl::opengl::Device& oglDevice = static_cast<igl::opengl::Device&>(device);
    oglDevice.getContext().setCurrent();
    TextureDesc desc = {
        width,
        height,
        1,
        1,
        1,
        TextureDesc::TextureUsageBits::Attachment,
        1,
        TextureType::TwoD,
        format,
    };
    auto color =
        std::make_shared<igl::opengl::ViewTextureTarget>(oglDevice.getContext(), desc.format);
    color->create(desc, true);
    desc.format = TextureFormat::Z_UNorm24;
    auto depth =
        std::make_shared<igl::opengl::ViewTextureTarget>(oglDevice.getContext(), desc.format);
    depth->create(desc, true);
    return igl::SurfaceTextures{std::move(color), std::move(depth)};
  }

  return igl::SurfaceTextures{};
}

// Windows spawns a window through glfw and this doesn't seem to fly with validation.
// This mode is similar to what is being done to the unittests where we spawn a device but no
// window.
void RunScreenshotTestsMode(igl::shell::ShellParams shellParams) {
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  createTestDeviceAndQueue(iglDev_, cmdQueue_);
  auto glShellPlatform = std::make_shared<igl::shell::PlatformWin>(std::move(iglDev_));

  {
    std::unique_ptr<igl::shell::RenderSession> glSession;
    glSession = igl::shell::createDefaultRenderSession(glShellPlatform);
    IGL_ASSERT_MSG(glSession, "createDefaultRenderSession() must return a valid session");
    glSession->initialize();

    auto surfaceTextures = createSurfaceTextures(glShellPlatform->getDevice(),
                                                 static_cast<size_t>(shellParams.viewportSize.x),
                                                 static_cast<size_t>(shellParams.viewportSize.y),
                                                 shellParams.defaultColorFramebufferFormat);
    IGL_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);

    while (!glSession->appParams().exitRequested) {
#if defined(_WIN32)
      static_cast<WGLDevice&>(glShellPlatform->getDevice()).getContext().setCurrent();
#elif IGL_PLATFORM_LINUX
      static_cast<GLXDevice&>(glShellPlatform->getDevice()).getContext().setCurrent();
#endif
      glShellPlatform->getInputDispatcher().processEvents();
      glSession->update(surfaceTextures);
    }
  }
}

} // namespace igl::shell::util
