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
  igl::BackendVersion _backendVersion;
  CGRect _frame;
  CADisplayLink* _renderTimer;
  std::unique_ptr<igl::shell::RenderSession> _session;
  std::shared_ptr<igl::shell::Platform> _platform;
  IglShellPlatformAdapter _platformAdapter;
  igl::shell::IRenderSessionFactory* _factory;
  id<IglSurfaceTexturesProvider> _surfaceTexturesProvider;
}
- (igl::BackendVersion)toBackendVersion:(BackendVersion*)version;
@end

@implementation RenderSessionController

- (instancetype)initWithBackendVersion:(BackendVersion*)backendVersion
                       factoryProvider:(RenderSessionFactoryProvider*)factoryProvider
                       surfaceProvider:(id<IglSurfaceTexturesProvider>)provider {
  if (self = [super init]) {
    self->_backendVersion = [self toBackendVersion:backendVersion];
    _factory = [factoryProvider adapter]->factory;
    _frame = CGRectMake(0, 0, 1024, 768); // choose some default

    // @fb-only
                     // @fb-only
    _surfaceTexturesProvider = provider;
  }
  return self;
}

- (void)initializeDevice {
  igl::HWDeviceQueryDesc queryDesc(igl::HWDeviceType::DiscreteGpu);
  std::unique_ptr<igl::IDevice> device;

  switch (_backendVersion.flavor) {
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
    device = igl::opengl::ios::HWDevice().create(_backendVersion, nullptr);
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
    IGL_DEBUG_ABORT("IGL Samples not set up for backend(%d)", (int)_backendVersion.flavor);
    break;
  }

  _platform = std::make_shared<igl::shell::PlatformIos>(std::move(device));
  _platformAdapter.platform = _platform.get();
  _session = _factory->createRenderSession(_platform);
  IGL_DEBUG_ASSERT(_session, "createDefaultRenderSession() must return a valid session");
  _session->initialize();
}

// @protocol IglShellPlatformAdapter
- (IglShellPlatformAdapterPtr)adapter {
  return &_platformAdapter;
}

- (void)start {
  if (_backendVersion.flavor != igl::BackendFlavor::Metal) {
    // Render at 60hz
    _renderTimer = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick)];
    [_renderTimer addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  }
}

- (void)stop {
  [_renderTimer removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  _renderTimer = nullptr;
}

- (void)tick {
  igl::DeviceScope scope(_platform->getDevice());

  // @fb-only
                   // @fb-only
  IglSurfaceTexturesAdapter* adapter = [_surfaceTexturesProvider createSurfaceTextures];
  // @fb-only
                   // @fb-only

  // process user input
  _platform->getInputDispatcher().processEvents();

  // draw
  if (_backendVersion.flavor == igl::BackendFlavor::Metal) {
    _session->setPixelsPerPoint((float)[UIScreen mainScreen].scale);
  } else if (_backendVersion.flavor == igl::BackendFlavor::OpenGL) {
    _session->setPixelsPerPoint(1.0f);
  }
  _session->update(adapter ? std::move(adapter->surfaceTextures) : igl::SurfaceTextures{});
}

- (void)releaseSessionFrameBuffer {
  if (_session) {
    _session->releaseFramebuffer();
  }
}

- (void)setFrame:(CGRect)frame {
  self->_frame = frame;
}

- (igl::BackendVersion)toBackendVersion:(BackendVersion*)version {
  return {.flavor = static_cast<igl::BackendFlavor>(version.flavor),
          .majorVersion = version.majorVersion,
          .minorVersion = version.minorVersion};
}

@end
