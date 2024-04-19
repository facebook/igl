/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "ViewController.h"

#import "View.h"

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
#include <memory>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/RenderSession.h>
// @fb-only
// @fb-only
// @fb-only
@interface ViewController () <TouchDelegate> {
  igl::BackendType backendType_;
#if IGL_BACKEND_OPENGL
  igl::opengl::RenderingAPI openglRenderingAPI_;
#endif
  CADisplayLink* renderTimer_;
  CALayer* layer_;
  CGRect frame_;
  id<CAMetalDrawable> currentDrawable_;
  id<MTLTexture> depthStencilTexture_;

  std::unique_ptr<igl::shell::RenderSession> session_;
  std::shared_ptr<igl::shell::Platform> platform_;
}
@end

@implementation ViewController

- (void)drawInMTKView:(nonnull MTKView*)view {
  currentDrawable_ = view.currentDrawable;
  depthStencilTexture_ = view.depthStencilTexture;
  [self tick];
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
}

- (instancetype)initForMetal:(CGRect)frame {
  if (self = [super initWithNibName:nil bundle:nil]) {
    backendType_ = igl::BackendType::Metal;
    frame_ = frame;
  }
  return self;
}

#if IGL_BACKEND_OPENGL
- (instancetype)initForOpenGL:(igl::opengl::RenderingAPI)renderingAPI frame:(CGRect)frame {
  if (self = [super initWithNibName:nil bundle:nil]) {
    backendType_ = igl::BackendType::OpenGL;
    openglRenderingAPI_ = renderingAPI;
    frame_ = frame;
  }
  return self;
}
#endif

- (void)initShell {
  igl::HWDeviceQueryDesc queryDesc(igl::HWDeviceType::DiscreteGpu);
  std::unique_ptr<igl::IDevice> device;
  if (backendType_ == igl::BackendType::Metal) {
#if IGL_BACKEND_METAL
    igl::metal::HWDevice hwDevice;
    auto hwDevices = hwDevice.queryDevices(queryDesc, nullptr);
    device = hwDevice.create(hwDevices[0], nullptr);
#endif
  } else if (backendType_ == igl::BackendType::OpenGL) {
#if IGL_BACKEND_OPENGL
    auto hwDevices = igl::opengl::ios::HWDevice().queryDevices(queryDesc, nullptr);
    device = igl::opengl::ios::HWDevice().create(hwDevices[0], openglRenderingAPI_, nullptr);
#endif
  } else {
    IGL_ASSERT_NOT_REACHED();
  }

  platform_ = std::make_shared<igl::shell::PlatformIos>(std::move(device));
  session_ = igl::shell::createDefaultRenderSession(platform_);
  IGL_ASSERT_MSG(session_, "createDefaultRenderSession() must return a valid session");
  session_->initialize();
}

- (igl::SurfaceTextures)createOpenGLSurfaceTextures {
#if IGL_BACKEND_OPENGL
  IGL_ASSERT(backendType_ == igl::BackendType::OpenGL);
  auto& device = platform_->getDevice();
  auto platformDevice = device.getPlatformDevice<igl::opengl::ios::PlatformDevice>();
  IGL_ASSERT(platformDevice);
  auto color = platformDevice->createTextureFromNativeDrawable((CAEAGLLayer*)layer_, nullptr);
  auto depth = platformDevice->createTextureFromNativeDepth((CAEAGLLayer*)layer_, nullptr);
  return igl::SurfaceTextures{std::move(color), std::move(depth)};
#else
  return igl::SurfaceTextures{};
#endif
}

- (igl::SurfaceTextures)createMetalSurfaceTextures {
#if IGL_BACKEND_METAL
  IGL_ASSERT(backendType_ == igl::BackendType::Metal);
  auto& device = platform_->getDevice();
  auto platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
  IGL_ASSERT(platformDevice);
  auto color = platformDevice->createTextureFromNativeDrawable(currentDrawable_, nullptr);
  auto depth = platformDevice->createTextureFromNativeDepth(depthStencilTexture_, nullptr);
  return igl::SurfaceTextures{std::move(color), std::move(depth)};
#else
  return igl::SurfaceTextures{};
#endif
}

- (igl::SurfaceTextures)createSurfaceTextures {
  if (backendType_ == igl::BackendType::Metal) {
    return [self createMetalSurfaceTextures];
  } else if (backendType_ == igl::BackendType::OpenGL) {
    return [self createOpenGLSurfaceTextures];
  } else {
    IGL_ASSERT_NOT_REACHED();
    return igl::SurfaceTextures{};
  }
}

- (void)tick {
  igl::DeviceScope scope(platform_->getDevice());

// @fb-only
  // @fb-only
// @fb-only
  // process user input
  platform_->getInputDispatcher().processEvents();

  // draw
  igl::SurfaceTextures surfaceTextures = [self createSurfaceTextures];
  auto backend = platform_->getDevice().getBackendType();
  if (backend == igl::BackendType::Metal) {
    session_->setPixelsPerPoint((float)[UIScreen mainScreen].scale);
  } else if (backend == igl::BackendType::OpenGL) {
    session_->setPixelsPerPoint(1.0f);
  }
  session_->update(std::move(surfaceTextures));
}

- (void)loadView {
  switch (backendType_) {
  case igl::BackendType::Invalid:
    IGL_ASSERT_NOT_REACHED();
    break;
  case igl::BackendType::Metal: {
#if IGL_BACKEND_METAL
    [self initShell];
    auto d = static_cast<igl::metal::Device&>(platform_->getDevice()).get();

    auto metalView = [[MetalView alloc] initWithFrame:frame_ device:d];
    metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    metalView.delegate = self;
    [metalView setTouchDelegate:self];
    self.view = metalView;
    layer_ = metalView.layer;
#endif
    break;
  }
  case igl::BackendType::OpenGL:
#if IGL_BACKEND_OPENGL
    self.view = [[OpenGLView alloc] initWithTouchDelegate:self];
#endif
    break;
  case igl::BackendType::Vulkan:
    IGL_ASSERT_MSG(0, "IGL Samples not set up for Vulkan backend");
    break;
  // @fb-only
    // @fb-only
    // @fb-only
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (backendType_ != igl::BackendType::Metal) {
    layer_ = self.view.layer;
    [self initShell];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
// @fb-only
  // @fb-only
// @fb-only
  if (backendType_ == igl::BackendType::OpenGL) {
    // Render at 60hz
    renderTimer_ = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick)];
    [renderTimer_ addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
// @fb-only
  // @fb-only
// @fb-only
  [renderTimer_ removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  renderTimer_ = nullptr;
}

- (void)touchBegan:(UITouch*)touch {
  CGPoint curPoint = [touch locationInView:self.view];
  CGPoint lastPoint = [touch previousLocationInView:self.view];
  platform_->getInputDispatcher().queueEvent(igl::shell::TouchEvent(
      true, curPoint.x, curPoint.y, curPoint.x - lastPoint.x, curPoint.y - lastPoint.y));
}

- (void)touchEnded:(UITouch*)touch {
  CGPoint curPoint = [touch locationInView:self.view];
  CGPoint lastPoint = [touch previousLocationInView:self.view];
  platform_->getInputDispatcher().queueEvent(igl::shell::TouchEvent(
      false, curPoint.x, curPoint.y, curPoint.x - lastPoint.x, curPoint.y - lastPoint.y));
}

- (void)touchMoved:(UITouch*)touch {
  CGPoint curPoint = [touch locationInView:self.view];
  CGPoint lastPoint = [touch previousLocationInView:self.view];
  platform_->getInputDispatcher().queueEvent(igl::shell::TouchEvent(
      true, curPoint.x, curPoint.y, curPoint.x - lastPoint.x, curPoint.y - lastPoint.y));
}

@end
