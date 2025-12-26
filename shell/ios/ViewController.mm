/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "ViewController.h"

#import "BackendVersion.h"
#import "IglShellPlatformAdapter.h"
#import "IglShellPlatformAdapterInternal.hpp" // IWYU pragma: keep
#import "IglSurfaceTexturesAdapter.h"
#import "IglSurfaceTexturesAdapterInternal.hpp" // IWYU pragma: keep
#import "RenderSessionController.h" // IWYU pragma: keep
#import "RenderSessionFactoryProvider.h"
#import "View.h"

#import <shell/shared/input/InputDispatcher.h>
#include <shell/shared/platform/Platform.h>
#import <igl/IGL.h> // IWYU pragma: keep
#include <igl/Texture.h>

#if IGL_BACKEND_METAL
#import <Metal/Metal.h>
#include <igl/metal/Device.h>
#include <igl/metal/Texture.h>
#endif

#if IGL_BACKEND_OPENGL
#include <igl/opengl/ios/Context.h>
#include <igl/opengl/ios/PlatformDevice.h>
#endif

// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only

#include <memory>
#include <shell/shared/input/TouchListener.h>
#include <shell/shared/platform/ios/PlatformIos.h>
#include <shell/shared/renderSession/RenderSessionConfig.h>
#include <igl/DeviceFeatures.h>

@interface ViewController () <TouchDelegate, ViewSizeChangeDelegate, IglSurfaceTexturesProvider> {
  igl::shell::RenderSessionConfig config;
  CALayer* layer;
  CGRect frame;
  id<CAMetalDrawable> currentDrawable;
  id<MTLTexture> depthStencilTexture;

  RenderSessionController* renderSessionController;
  IglSurfaceTexturesAdapter surfaceTexturesAdapter;
}
- (BackendVersion*)toBackendVersion:(igl::BackendVersion)iglBackendVersion;
@end

@implementation ViewController

- (void)drawInMTKView:(nonnull MTKView*)view {
  currentDrawable = view.currentDrawable;
  depthStencilTexture = view.depthStencilTexture;

  IGL_DEBUG_ASSERT(renderSessionController);
  [renderSessionController tick];
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
  [renderSessionController releaseSessionFrameBuffer];
}

- (void)onViewSizeChange {
  [renderSessionController releaseSessionFrameBuffer];
}

- (instancetype)init:(igl::shell::RenderSessionConfig)config
     factoryProvider:(RenderSessionFactoryProvider*)factoryProvider
               frame:(CGRect)frame {
  if (self = [super initWithNibName:nil bundle:nil]) {
    self->config = config;
    self->frame = frame;
    renderSessionController = [[RenderSessionController alloc]
        initWithBackendVersion:[self toBackendVersion:config.backendVersion]
               factoryProvider:factoryProvider
               surfaceProvider:self];
  }
  return self;
}

- (void)initRenderSessionController {
  IGL_DEBUG_ASSERT(renderSessionController);

// @fb-only
  // @fb-only
    // @fb-only
    // @fb-only
  // @fb-only
// @fb-only

  [renderSessionController initializeDevice];
}

- (igl::shell::Platform*)platform {
  IglShellPlatformAdapter* adapter = [renderSessionController adapter];
  IGL_DEBUG_ASSERT(adapter);
  return adapter->platform;
}

// clang-format off
- (igl::SurfaceTextures)createSurfaceTexturesInternal {
  [[maybe_unused]] auto& device = [self platform]->getDevice();
  switch (config.backendVersion.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto *platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    return igl::SurfaceTextures{
        .color = platformDevice->createTextureFromNativeDrawable(currentDrawable, nullptr),
        .depth = platformDevice->createTextureFromNativeDepth(depthStencilTexture, nullptr),
    };
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL_ES: {
    auto *platformDevice = device.getPlatformDevice<igl::opengl::ios::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    return igl::SurfaceTextures{
        .color = platformDevice->createTextureFromNativeDrawable((CAEAGLLayer*)layer, nullptr),
        .depth = platformDevice->createTextureFromNativeDepth((CAEAGLLayer*)layer, config.depthTextureFormat, nullptr),
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
  surfaceTexturesAdapter.surfaceTextures = [self createSurfaceTexturesInternal];
  return &surfaceTexturesAdapter;
}

- (void)loadView {
  switch (config.backendVersion.flavor) {
  case igl::BackendFlavor::Invalid:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    break;
  case igl::BackendFlavor::Metal: {
#if IGL_BACKEND_METAL
    [self initRenderSessionController];
    auto d = static_cast<igl::metal::Device&>([self platform]->getDevice()).get();

    auto metalView = [[MetalView alloc] initWithFrame:frame device:d];
    metalView.colorPixelFormat =
        igl::metal::Texture::textureFormatToMTLPixelFormat(config.swapchainColorTextureFormat);
    metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    metalView.delegate = self;
    [metalView setTouchDelegate:self];
    self.view = metalView;
    layer = metalView.layer;
#endif
    break;
  }
  case igl::BackendFlavor::OpenGL_ES: {
#if IGL_BACKEND_OPENGL
    auto openGLView = [[OpenGLView alloc] initWithTouchDelegate:self];
    openGLView.viewSizeChangeDelegate = self;

    NSString* drawablePropertyColorFormat = kEAGLColorFormatRGBA8;

    switch (config.swapchainColorTextureFormat) {
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
  case igl::BackendFlavor::D3D12:
    IGL_DEBUG_ABORT("IGL Samples not set up for D3D12 backend");
    break;
  // @fb-only
    // @fb-only
    // @fb-only
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (config.backendVersion.flavor != igl::BackendFlavor::Metal) {
    layer = self.view.layer;
    [self initRenderSessionController];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (config.backendVersion.flavor != igl::BackendFlavor::Metal) {
    IGL_DEBUG_ASSERT(renderSessionController);
    [renderSessionController start];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  if (config.backendVersion.flavor != igl::BackendFlavor::Metal) {
    IGL_DEBUG_ASSERT(renderSessionController);
    [renderSessionController stop];
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
