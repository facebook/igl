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
#import <QuartzCore/CADisplayLink.h>
#import <UIKit/UIKit.h>
#import <igl/IGL.h>

#if IGL_BACKEND_METAL
#import <Metal/Metal.h>
#import <igl/metal/HWDevice.h>
#endif
#if IGL_BACKEND_OPENGL
#import <igl/opengl/ios/Context.h>
#import <igl/opengl/ios/Device.h>
#import <igl/opengl/ios/HWDevice.h>
#endif
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
#include <memory>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/RenderSession.h>

@interface RenderSessionController () {
  igl::BackendType backendType_;
#if IGL_BACKEND_OPENGL
  igl::opengl::RenderingAPI openglRenderingAPI_;
#endif
  CGRect frame_;
  CADisplayLink* renderTimer_;
  std::unique_ptr<igl::shell::RenderSession> session_;
  std::shared_ptr<igl::shell::Platform> platform_;
  IglShellPlatformAdapter platformAdapter_;
  id<IglSurfaceTexturesProvider> surfaceTexturesProvider_;
}
@end

@implementation RenderSessionController

- (instancetype)initWithIglBackend:(IglBackendType)backendType
                   surfaceProvider:(id<IglSurfaceTexturesProvider>)provider {
  if (self = [super init]) {
    backendType_ = (igl::BackendType)backendType;

    frame_ = CGRectMake(0, 0, 1024, 768); // choose some default

    // @fb-only
    surfaceTexturesProvider_ = provider;
  }
  return self;
}

- (instancetype)initWithOpenGLRenderingAPI:(IglOpenglRenderingAPI)renderingAPI
                           surfaceProvider:(id<IglSurfaceTexturesProvider>)provider {
  if (self = [self initWithIglBackend:(int)igl::BackendType::OpenGL surfaceProvider:provider]) {
    openglRenderingAPI_ = (igl::opengl::RenderingAPI)renderingAPI;
  }
  return self;
}

- (void)initializeDevice {
  igl::HWDeviceQueryDesc queryDesc(igl::HWDeviceType::DiscreteGpu);
  std::unique_ptr<igl::IDevice> device;

  switch (backendType_) {
  case igl::BackendType::Metal: {
#if IGL_BACKEND_METAL
    igl::metal::HWDevice hwDevice;
    auto hwDevices = hwDevice.queryDevices(queryDesc, nullptr);
    device = hwDevice.create(hwDevices[0], nullptr);
#endif
    break;
  }
  case igl::BackendType::OpenGL: {
#if IGL_BACKEND_OPENGL
    auto hwDevices = igl::opengl::ios::HWDevice().queryDevices(queryDesc, nullptr);
    device = igl::opengl::ios::HWDevice().create(hwDevices[0], openglRenderingAPI_, nullptr);
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
  default:
    IGL_ASSERT_MSG(0, "IGL Samples not set up for backend(%d)", (int)backendType_);
    break;
  }

  platform_ = std::make_shared<igl::shell::PlatformIos>(std::move(device));
  platformAdapter_.platform = platform_.get();
  session_ = igl::shell::createDefaultRenderSession(platform_);
  IGL_ASSERT_MSG(session_, "createDefaultRenderSession() must return a valid session");
  session_->initialize();
}

// @protocol IglShellPlatformAdapter
- (IglShellPlatformAdapterPtr)adapter {
  return &platformAdapter_;
}

- (void)start {
  if (backendType_ != igl::BackendType::Metal) {
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
  IglSurfaceTexturesAdapter* adapter = [surfaceTexturesProvider_ createSurfaceTextures];
  // @fb-only

  // process user input
  platform_->getInputDispatcher().processEvents();

  // draw
  if (backendType_ == igl::BackendType::Metal) {
    session_->setPixelsPerPoint((float)[UIScreen mainScreen].scale);
  } else if (backendType_ == igl::BackendType::OpenGL) {
    session_->setPixelsPerPoint(1.0f);
  }
  session_->update(adapter ? std::move(adapter->surfaceTextures) : igl::SurfaceTextures{});
}

- (void)setFrame:(CGRect)frame {
  frame_ = frame;
}

@end
