/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "RenderSessionController.h"

#import "IglShellPlatformAdapterInternal.hpp"
#import "IglSurfaceTexturesAdapterInternal.hpp"
#import "RenderSessionFactoryAdapterInternal.hpp"

#import <QuartzCore/CADisplayLink.h>
#import <UIKit/UIKit.h>
#import <shell/shared/input/InputDispatcher.h>
#import <igl/IGL.h>

#if IGL_BACKEND_METAL
#import <Metal/Metal.h>
#import <igl/metal/HWDevice.h>
#endif
#if IGL_BACKEND_OPENGL
#include <igl/opengl/ios/Context.h>
#include <igl/opengl/ios/HWDevice.h>
#endif
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
#include <memory>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/RenderSessionConfig.h>

@interface RenderSessionController () {
  igl::BackendVersion backendVersion;
  CGRect frame;
  CADisplayLink* renderTimer;
  std::unique_ptr<igl::shell::RenderSession> session;
  std::shared_ptr<igl::shell::Platform> platform;
  IglShellPlatformAdapter platformAdapter;
  igl::shell::IRenderSessionFactory* factory;
  id<IglSurfaceTexturesProvider> surfaceTexturesProvider;
}
- (igl::BackendVersion)toBackendVersion:(BackendVersion*)version;
@end

@implementation RenderSessionController

- (instancetype)initWithBackendVersion:(BackendVersion*)backendVersion
                       factoryProvider:(RenderSessionFactoryProvider*)factoryProvider
                       surfaceProvider:(id<IglSurfaceTexturesProvider>)provider {
  if (self = [super init]) {
    self->backendVersion = [self toBackendVersion:backendVersion];
    factory = [factoryProvider adapter]->factory;
    frame = CGRectMake(0, 0, 1024, 768); // choose some default

    // @fb-only
                     // @fb-only
    surfaceTexturesProvider = provider;
  }
  return self;
}

- (void)initializeDevice {
  igl::HWDeviceQueryDesc queryDesc(igl::HWDeviceType::DiscreteGpu);
  std::unique_ptr<igl::IDevice> device;

  switch (backendVersion.flavor) {
  case igl::BackendFlavor::Metal: {
#if IGL_BACKEND_METAL
    igl::metal::HWDevice hwDevice;
    auto hwDevices = hwDevice.queryDevices(queryDesc, nullptr);
    device = hwDevice.create(hwDevices[0], nullptr);
#endif
    break;
  }
  case igl::BackendFlavor::OpenGL_ES: {
#if IGL_BACKEND_OPENGL
    device = igl::opengl::ios::HWDevice().create(backendVersion, nullptr);
#endif
    break;
  }
// @fb-only
  // @fb-only
    // @fb-only
    // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
        // @fb-only
    // @fb-only
  // @fb-only
// @fb-only
  default:
    IGL_DEBUG_ABORT("IGL Samples not set up for backend(%d)", (int)backendVersion.flavor);
    break;
  }

  platform = std::make_shared<igl::shell::PlatformIos>(std::move(device));
  platformAdapter.platform = platform.get();
  session = factory->createRenderSession(platform);
  IGL_DEBUG_ASSERT(session, "createDefaultRenderSession() must return a valid session");
  session->initialize();
}

// @protocol IglShellPlatformAdapter
- (IglShellPlatformAdapterPtr)adapter {
  return &platformAdapter;
}

- (void)start {
  if (backendVersion.flavor != igl::BackendFlavor::Metal) {
    // Render at 60hz
    renderTimer = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick)];
    [renderTimer addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  }
}

- (void)stop {
  [renderTimer removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  renderTimer = nullptr;
}

- (void)tick {
  igl::DeviceScope scope(platform->getDevice());

  // @fb-only
                   // @fb-only
  IglSurfaceTexturesAdapter* adapter = [surfaceTexturesProvider createSurfaceTextures];
  // @fb-only
                   // @fb-only

  // process user input
  platform->getInputDispatcher().processEvents();

  // draw
  if (backendVersion.flavor == igl::BackendFlavor::Metal) {
    session->setPixelsPerPoint((float)[UIScreen mainScreen].scale);
  } else if (backendVersion.flavor == igl::BackendFlavor::OpenGL) {
    session->setPixelsPerPoint(1.0f);
  }
  session->update(adapter ? std::move(adapter->surfaceTextures) : igl::SurfaceTextures{});
}

- (void)releaseSessionFrameBuffer {
  if (session) {
    session->releaseFramebuffer();
  }
}

- (void)setFrame:(CGRect)frame {
  self->frame = frame;
}

- (igl::BackendVersion)toBackendVersion:(BackendVersion*)version {
  return {.flavor = static_cast<igl::BackendFlavor>(version.flavor),
          .majorVersion = version.majorVersion,
          .minorVersion = version.minorVersion};
}

@end
