/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "ViewController.h"

#import "BackendVersion.h"
#import "IglShellPlatformAdapterInternal.hpp"
#import "IglSurfaceTexturesAdapterInternal.hpp"
#import "RenderSessionController.h"
#import "View.h"

#import <igl/IGL.h>
#import <shell/shared/input/InputDispatcher.h>

#if IGL_BACKEND_METAL
#import <Metal/Metal.h>
#import <igl/metal/HWDevice.h>
#include <igl/metal/Texture.h>
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

#include <igl/DeviceFeatures.h>
#include <memory>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/renderSession/RenderSession.h>

// @fb-only
// @fb-only
// @fb-only

@interface ViewController () <TouchDelegate, ViewSizeChangeDelegate, IglSurfaceTexturesProvider> {
  igl::shell::RenderSessionConfig config_;
  CALayer* layer_;
  CGRect frame_;
  id<CAMetalDrawable> currentDrawable_;
  id<MTLTexture> depthStencilTexture_;

  RenderSessionController* renderSessionController_;
  IglSurfaceTexturesAdapter surfaceTexturesAdapter_;
}
- (BackendVersion*)toBackendVersion:(igl::BackendVersion)iglBackendVersion;
@end

@implementation ViewController

- (void)drawInMTKView:(nonnull MTKView*)view {
  currentDrawable_ = view.currentDrawable;
  depthStencilTexture_ = view.depthStencilTexture;

  IGL_DEBUG_ASSERT(renderSessionController_);
  [renderSessionController_ tick];
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
  [renderSessionController_ releaseSessionFrameBuffer];
}

- (void)onViewSizeChange {
  [renderSessionController_ releaseSessionFrameBuffer];
}

- (instancetype)init:(igl::shell::RenderSessionConfig)config
     factoryProvider:(RenderSessionFactoryProvider*)factoryProvider
               frame:(CGRect)frame {
  if (self = [super initWithNibName:nil bundle:nil]) {
    config_ = config;
    frame_ = frame;
    renderSessionController_ = [[RenderSessionController alloc]
        initWithBackendVersion:[self toBackendVersion:config.backendVersion]
               factoryProvider:factoryProvider
               surfaceProvider:self];
  }
  return self;
}

- (void)initRenderSessionController {
  IGL_DEBUG_ASSERT(renderSessionController_);

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
  IGL_DEBUG_ASSERT(adapter);
  return adapter->platform;
}

// clang-format off
- (igl::SurfaceTextures)createSurfaceTexturesInternal {
  [[maybe_unused]] auto& device = [self platform]->getDevice();
  switch (config_.backendVersion.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto *platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    return igl::SurfaceTextures{
        .color = platformDevice->createTextureFromNativeDrawable(currentDrawable_, nullptr),
        .depth = platformDevice->createTextureFromNativeDepth(depthStencilTexture_, nullptr),
    };
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL_ES: {
    auto *platformDevice = device.getPlatformDevice<igl::opengl::ios::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    return igl::SurfaceTextures{
        .color = platformDevice->createTextureFromNativeDrawable((CAEAGLLayer*)layer_, nullptr),
        .depth = platformDevice->createTextureFromNativeDepth((CAEAGLLayer*)layer_, config_.depthTextureFormat, nullptr),
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
    IGL_DEBUG_ASSERT_NOT_REACHED();
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
  switch (config_.backendVersion.flavor) {
  case igl::BackendFlavor::Invalid:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    break;
  case igl::BackendFlavor::Metal: {
#if IGL_BACKEND_METAL
    [self initRenderSessionController];
    auto d = static_cast<igl::metal::Device&>([self platform]->getDevice()).get();

    auto metalView = [[MetalView alloc] initWithFrame:frame_ device:d];
    metalView.colorPixelFormat =
        igl::metal::Texture::textureFormatToMTLPixelFormat(config_.swapchainColorTextureFormat);
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

    NSString* drawablePropertyColorFormat = kEAGLColorFormatRGBA8;

    switch (config_.swapchainColorTextureFormat) {
    case igl::TextureFormat::BGRA_UNorm8:
      drawablePropertyColorFormat = kEAGLColorFormatRGBA8;
      break;

    case igl::TextureFormat::BGRA_SRGB:
      drawablePropertyColorFormat = kEAGLColorFormatSRGBA8;
      break;

    default:
      break;
    }

    ((CAEAGLLayer*)openGLView.layer).drawableProperties =
        [NSDictionary dictionaryWithObjectsAndKeys:drawablePropertyColorFormat,
                                                   kEAGLDrawablePropertyColorFormat,
                                                   nil];
    self.view = openGLView;
#endif
    break;
  }
  case igl::BackendFlavor::OpenGL:
    IGL_DEBUG_ABORT("IGL Samples not set up for Desktop OpenGL backend");
    break;
  case igl::BackendFlavor::Vulkan:
    IGL_DEBUG_ABORT("IGL Samples not set up for Vulkan backend");
    break;
  // @fb-only
    // @fb-only
    // @fb-only
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (config_.backendVersion.flavor != igl::BackendFlavor::Metal) {
    layer_ = self.view.layer;
    [self initRenderSessionController];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
// @fb-only
  // @fb-only
// @fb-only
  if (config_.backendVersion.flavor != igl::BackendFlavor::Metal) {
    IGL_DEBUG_ASSERT(renderSessionController_);
    [renderSessionController_ start];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
// @fb-only
  // @fb-only
// @fb-only

  if (config_.backendVersion.flavor != igl::BackendFlavor::Metal) {
    IGL_DEBUG_ASSERT(renderSessionController_);
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

- (BackendVersion*)toBackendVersion:(igl::BackendVersion)iglBackendVersion {
  return [[BackendVersion alloc] init:static_cast<BackendFlavor>(iglBackendVersion.flavor)
                         majorVersion:iglBackendVersion.majorVersion
                         minorVersion:iglBackendVersion.minorVersion];
}

@end
