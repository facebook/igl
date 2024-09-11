/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "ViewController.h"

#import "IglShellPlatformAdapterInternal.hpp"
#import "IglSurfaceTexturesAdapterInternal.hpp"
#import "RenderSessionController.h"
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

// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only

#include <memory>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/RenderSession.h>

// @fb-only
// @fb-only
// @fb-only

@interface ViewController () <TouchDelegate, ViewSizeChangeDelegate, IglSurfaceTexturesProvider> {
  igl::BackendVersion backendVersion_;
  CALayer* layer_;
  CGRect frame_;
  id<CAMetalDrawable> currentDrawable_;
  id<MTLTexture> depthStencilTexture_;

  RenderSessionController* renderSessionController_;
  IglSurfaceTexturesAdapter surfaceTexturesAdapter_;
}
@end

@implementation ViewController

- (void)drawInMTKView:(nonnull MTKView*)view {
  currentDrawable_ = view.currentDrawable;
  depthStencilTexture_ = view.depthStencilTexture;

  IGL_ASSERT(renderSessionController_);
  [renderSessionController_ tick];
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
  [renderSessionController_ releaseSessionFrameBuffer];
}

- (void)onViewSizeChange {
  [renderSessionController_ releaseSessionFrameBuffer];
}

- (instancetype)init:(igl::BackendVersion)backendVersion frame:(CGRect)frame {
  if (self = [super initWithNibName:nil bundle:nil]) {
    backendVersion_ = backendVersion;
    frame_ = frame;
    renderSessionController_ = [[RenderSessionController alloc]
        initWithBackendFlavor:(IglBackendFlavor)backendVersion_.flavor
                 majorVersion:backendVersion_.majorVersion
                 minorVersion:backendVersion_.minorVersion
              surfaceProvider:self];
  }
  return self;
}

- (void)initRenderSessionController {
  IGL_ASSERT(renderSessionController_);

// @fb-only
  // @fb-only
    // @fb-only
    // @fb-only
  // @fb-only
// @fb-only

  [renderSessionController_ initializeDevice];
}

- (igl::shell::Platform*)platform {
  IglShellPlatformAdapter* adapter = [renderSessionController_ adapter];
  IGL_ASSERT(adapter);
  return adapter->platform;
}

// clang-format off
- (igl::SurfaceTextures)createSurfaceTexturesInternal {
  [[maybe_unused]] auto& device = [self platform]->getDevice();
  switch (backendVersion_.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto *platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_ASSERT(platformDevice);
    return igl::SurfaceTextures{
        .color = platformDevice->createTextureFromNativeDrawable(currentDrawable_, nullptr),
        .depth = platformDevice->createTextureFromNativeDepth(depthStencilTexture_, nullptr),
    };
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL_ES: {
    auto *platformDevice = device.getPlatformDevice<igl::opengl::ios::PlatformDevice>();
    IGL_ASSERT(platformDevice);
    return igl::SurfaceTextures{
        .color = platformDevice->createTextureFromNativeDrawable((CAEAGLLayer*)layer_, nullptr),
        .depth = platformDevice->createTextureFromNativeDepth((CAEAGLLayer*)layer_, igl::TextureFormat::Z_UNorm16, nullptr),
    };
  }
#endif

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

  default: {
    IGL_ASSERT_NOT_REACHED();
    return igl::SurfaceTextures{};
  }
  }
}
// clang-format on

// Protocol IglSurfaceTexturesProvider
- (IglSurfacesTextureAdapterPtr)createSurfaceTextures {
  surfaceTexturesAdapter_.surfaceTextures = [self createSurfaceTexturesInternal];
  return &surfaceTexturesAdapter_;
}

- (void)loadView {
  switch (backendVersion_.flavor) {
  case igl::BackendFlavor::Invalid:
    IGL_ASSERT_NOT_REACHED();
    break;
  case igl::BackendFlavor::Metal: {
#if IGL_BACKEND_METAL
    [self initRenderSessionController];
    auto d = static_cast<igl::metal::Device&>([self platform]->getDevice()).get();

    auto metalView = [[MetalView alloc] initWithFrame:frame_ device:d];
    metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    metalView.delegate = self;
    [metalView setTouchDelegate:self];
    self.view = metalView;
    layer_ = metalView.layer;
#endif
    break;
  }
  case igl::BackendFlavor::OpenGL_ES: {
#if IGL_BACKEND_OPENGL
    auto openGLView = [[OpenGLView alloc] initWithTouchDelegate:self];
    openGLView.viewSizeChangeDelegate = self;
    self.view = openGLView;
#endif
    break;
  }
  case igl::BackendFlavor::OpenGL:
    IGL_ASSERT_MSG(0, "IGL Samples not set up for Desktop OpenGL backend");
    break;
  case igl::BackendFlavor::Vulkan:
    IGL_ASSERT_MSG(0, "IGL Samples not set up for Vulkan backend");
    break;
  // @fb-only
    // @fb-only
    // @fb-only
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (backendVersion_.flavor != igl::BackendFlavor::Metal) {
    layer_ = self.view.layer;
    [self initRenderSessionController];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
// @fb-only
  // @fb-only
// @fb-only
  if (backendVersion_.flavor != igl::BackendFlavor::Metal) {
    IGL_ASSERT(renderSessionController_);
    [renderSessionController_ start];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
// @fb-only
  // @fb-only
// @fb-only

  if (backendVersion_.flavor != igl::BackendFlavor::Metal) {
    IGL_ASSERT(renderSessionController_);
    [renderSessionController_ stop];
  }
}

- (void)touchBegan:(UITouch*)touch {
  CGPoint curPoint = [touch locationInView:self.view];
  CGPoint lastPoint = [touch previousLocationInView:self.view];
  [self platform]->getInputDispatcher().queueEvent(igl::shell::TouchEvent(
      true, curPoint.x, curPoint.y, curPoint.x - lastPoint.x, curPoint.y - lastPoint.y));
}

- (void)touchEnded:(UITouch*)touch {
  CGPoint curPoint = [touch locationInView:self.view];
  CGPoint lastPoint = [touch previousLocationInView:self.view];
  [self platform]->getInputDispatcher().queueEvent(igl::shell::TouchEvent(
      false, curPoint.x, curPoint.y, curPoint.x - lastPoint.x, curPoint.y - lastPoint.y));
}

- (void)touchMoved:(UITouch*)touch {
  CGPoint curPoint = [touch locationInView:self.view];
  CGPoint lastPoint = [touch previousLocationInView:self.view];
  [self platform]->getInputDispatcher().queueEvent(igl::shell::TouchEvent(
      true, curPoint.x, curPoint.y, curPoint.x - lastPoint.x, curPoint.y - lastPoint.y));
}

@end
