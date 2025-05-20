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
#include <igl/opengl/ios/Device.h>
#include <igl/opengl/ios/HWDevice.h>
#endif
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
#include <memory>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/renderSession/IRenderSessionFactory.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/RenderSessionConfig.h>

@interface RenderSessionController () {
  igl::BackendVersion backendVersion_;
  CGRect frame_;
  CADisplayLink* renderTimer_;
  std::unique_ptr<igl::shell::RenderSession> session_;
  std::shared_ptr<igl::shell::Platform> platform_;
  IglShellPlatformAdapter platformAdapter_;
  igl::shell::IRenderSessionFactory* factory_;
  id<IglSurfaceTexturesProvider> surfaceTexturesProvider_;
}
- (igl::BackendVersion)toBackendVersion:(BackendVersion*)version;
@end

@implementation RenderSessionController

- (instancetype)initWithBackendVersion:(BackendVersion*)backendVersion
                       factoryProvider:(RenderSessionFactoryProvider*)factoryProvider
                       surfaceProvider:(id<IglSurfaceTexturesProvider>)provider {
  if (self = [super init]) {
    backendVersion_ = [self toBackendVersion:backendVersion];
    factory_ = [factoryProvider adapter]->factory;
    frame_ = CGRectMake(0, 0, 1024, 768); // choose some default

    // @fb-only
                     // @fb-only
    surfaceTexturesProvider_ = provider;
  }
  return self;
}

- (void)initializeDevice {
  igl::HWDeviceQueryDesc queryDesc(igl::HWDeviceType::DiscreteGpu);
  std::unique_ptr<igl::IDevice> device;

  switch (backendVersion_.flavor) {
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
    device = igl::opengl::ios::HWDevice().create(backendVersion_, nullptr);
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
    IGL_DEBUG_ABORT("IGL Samples not set up for backend(%d)", (int)backendVersion_.flavor);
    break;
  }

  platform_ = std::make_shared<igl::shell::PlatformIos>(std::move(device));
  platformAdapter_.platform = platform_.get();
  session_ = factory_->createRenderSession(platform_);
  IGL_DEBUG_ASSERT(session_, "createDefaultRenderSession() must return a valid session");
  session_->initialize();
}

// @protocol IglShellPlatformAdapter
- (IglShellPlatformAdapterPtr)adapter {
  return &platformAdapter_;
}

- (void)start {
  if (backendVersion_.flavor != igl::BackendFlavor::Metal) {
    // Render at 60hz
    renderTimer_ = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick)];
    [renderTimer_ addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  }
}

- (void)stop {
  [renderTimer_ removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  renderTimer_ = nullptr;
}

- (void)tick {
  igl::DeviceScope scope(platform_->getDevice());

  // @fb-only
                   // @fb-only
  IglSurfaceTexturesAdapter* adapter = [surfaceTexturesProvider_ createSurfaceTextures];
  // @fb-only
                   // @fb-only

  // process user input
  platform_->getInputDispatcher().processEvents();

  // draw
  if (backendVersion_.flavor == igl::BackendFlavor::Metal) {
    session_->setPixelsPerPoint((float)[UIScreen mainScreen].scale);
  } else if (backendVersion_.flavor == igl::BackendFlavor::OpenGL) {
    session_->setPixelsPerPoint(1.0f);
  }
  session_->update(adapter ? std::move(adapter->surfaceTextures) : igl::SurfaceTextures{});
}

- (void)releaseSessionFrameBuffer {
  if (session_) {
    session_->releaseFramebuffer();
  }
}

- (void)setFrame:(CGRect)frame {
  frame_ = frame;
}

- (igl::BackendVersion)toBackendVersion:(BackendVersion*)version {
  return {.flavor = static_cast<igl::BackendFlavor>(version.flavor),
          .majorVersion = version.majorVersion,
          .minorVersion = version.minorVersion};
}

@end
