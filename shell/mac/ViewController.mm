/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "ViewController.h"

#import "GLView.h"
#import "HeadlessView.h"
#import "MetalView.h"
#import "VulkanView.h"
// @fb-only

#import <igl/Common.h>
#import <igl/IGL.h>
#import <shell/shared/input/InputDispatcher.h>
#if IGL_BACKEND_METAL
#import <igl/metal/ColorSpace.h>
#import <igl/metal/HWDevice.h>
#import <igl/metal/Texture.h>
#import <igl/metal/macos/Device.h>
#endif
#if IGL_BACKEND_OPENGL
#import <igl/opengl/macos/Context.h>
#import <igl/opengl/macos/Device.h>
#import <igl/opengl/macos/HWDevice.h>
#endif
#include <shell/shared/platform/mac/PlatformMac.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/ShellParams.h>
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
#if IGL_BACKEND_VULKAN
#import <igl/vulkan/Device.h>
#import <igl/vulkan/HWDevice.h>
#import <igl/vulkan/VulkanContext.h>
#endif
#import <cmath>
#import <simd/simd.h>

using namespace igl;

@interface ViewController () {
  igl::shell::IRenderSessionFactory* factory_;
  igl::shell::RenderSessionConfig config_;
  igl::shell::ShellParams shellParams_;
  CGRect frame_;
  CVDisplayLinkRef displayLink_; // For OpenGL only
  id<CAMetalDrawable> currentDrawable_;
  id<MTLTexture> depthStencilTexture_;
  std::shared_ptr<igl::shell::Platform> shellPlatform_;
  std::unique_ptr<igl::shell::RenderSession> session_;
  float kMouseSpeed_;
}
@end

@implementation ViewController

///--------------------------------------
/// MARK: - Init
///--------------------------------------

- (instancetype)initWithFrame:(CGRect)frame
                      factory:(igl::shell::IRenderSessionFactory&)factory
                       config:(igl::shell::RenderSessionConfig)config {
  self = [super initWithNibName:nil bundle:nil];
  if (!self) {
    return self;
  }

  config_ = std::move(config);
  factory_ = &factory;
  shellParams_ = igl::shell::ShellParams();
  shellParams_.viewportSize.x = frame.size.width;
  shellParams_.viewportSize.y = frame.size.height;
  frame_ = frame;
  kMouseSpeed_ = 0.05f;
  currentDrawable_ = nil;
  depthStencilTexture_ = nil;

  return self;
}

- (void)initModule {
}

- (void)teardown {
  if (session_) {
    session_->teardown();
  }
  session_ = nullptr;
  shellPlatform_ = nullptr;
}

- (void)render {
  if (session_ == nullptr) {
    return;
  }

  const NSRect contentRect = self.view.frame;

  shellParams_.viewportSize = glm::vec2(contentRect.size.width, contentRect.size.height);
  shellParams_.viewportScale = self.view.window.backingScaleFactor;
  session_->setShellParams(shellParams_);
  // process user input
  shellPlatform_->getInputDispatcher().processEvents();

  igl::SurfaceTextures surfaceTextures;
  if (config_.backendVersion.flavor != igl::BackendFlavor::Invalid &&
      shellPlatform_->getDevicePtr() != nullptr) {
// @fb-only
    // @fb-only
    // @fb-only
      // @fb-only
          // @fb-only
      // @fb-only
      // @fb-only
    // @fb-only
// @fb-only

    // surface textures
    surfaceTextures = igl::SurfaceTextures{[self createTextureFromNativeDrawable],
                                           [self createTextureFromNativeDepth]};
    IGL_DEBUG_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);
    const auto& dims = surfaceTextures.color->getDimensions();
    shellParams_.nativeSurfaceDimensions = glm::ivec2{dims.width, dims.height};

    // update retina scale
    float pixelsPerPoint = shellParams_.nativeSurfaceDimensions.x / shellParams_.viewportSize.x;
    session_->setPixelsPerPoint(pixelsPerPoint);
// @fb-only
    // @fb-only
    // @fb-only
        // @fb-only
    // @fb-only
// @fb-only
  }
  // draw
  session_->update(std::move(surfaceTextures));
  if (session_->appParams().exitRequested) {
    [[NSApplication sharedApplication] terminate:nil];
  }
}

- (void)loadView {
  // We don't care about the device type, just
  // return something that works
  HWDeviceQueryDesc queryDesc(HWDeviceType::Unknown);

  switch (config_.backendVersion.flavor) {
  case igl::BackendFlavor::Invalid: {
    auto headlessView = [[HeadlessView alloc] initWithFrame:frame_];
    self.view = headlessView;

    // @fb-only
        // @fb-only

    // Headless platform does not run on a real device
    shellPlatform_ = std::make_shared<igl::shell::PlatformMac>(nullptr);

    [headlessView prepareHeadless];
    break;
  }

#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto hwDevices = metal::HWDevice().queryDevices(queryDesc, nullptr);
    auto device = metal::HWDevice().create(hwDevices[0], nullptr);

    auto d = static_cast<igl::metal::macos::Device*>(device.get())->get();

    auto metalView = [[MetalView alloc] initWithFrame:frame_ device:d];
    metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    metalView.delegate = self;

    metalView.colorPixelFormat =
        metal::Texture::textureFormatToMTLPixelFormat(config_.swapchainColorTextureFormat);
    metalView.colorspace = metal::colorSpaceToCGColorSpace(config_.swapchainColorSpace);

    metalView.framebufferOnly = NO;
    [metalView setViewController:self];
    self.view = metalView;
    shellPlatform_ = std::make_shared<igl::shell::PlatformMac>(std::move(device));
    break;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    const bool enableStencilBuffer =
        config_.depthTextureFormat == igl::TextureFormat::S8_UInt_Z24_UNorm ||
        config_.depthTextureFormat == igl::TextureFormat::S_UInt8;
    const NSOpenGLPixelFormatAttribute stencilSize = enableStencilBuffer ? 8 : 0;

    NSOpenGLPixelFormat* pixelFormat;
    if (config_.backendVersion.majorVersion == 4 && config_.backendVersion.minorVersion == 1) {
      static NSOpenGLPixelFormatAttribute attributes[] = {
          NSOpenGLPFADoubleBuffer,
          NSOpenGLPFAAllowOfflineRenderers,
          NSOpenGLPFAMultisample,
          1,
          NSOpenGLPFASampleBuffers,
          1,
          NSOpenGLPFASamples,
          4,
          NSOpenGLPFAColorSize,
          32,
          NSOpenGLPFADepthSize,
          24,
          NSOpenGLPFAStencilSize,
          stencilSize,
          NSOpenGLPFAOpenGLProfile,
          NSOpenGLProfileVersion4_1Core,
          0,
      };
      pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      IGL_DEBUG_ASSERT(pixelFormat, "Requested attributes not supported");
    } else if (config_.backendVersion.majorVersion == 3 &&
               config_.backendVersion.minorVersion == 2) {
      static NSOpenGLPixelFormatAttribute attributes[] = {
          NSOpenGLPFADoubleBuffer,
          NSOpenGLPFAAllowOfflineRenderers,
          NSOpenGLPFAMultisample,
          1,
          NSOpenGLPFASampleBuffers,
          1,
          NSOpenGLPFASamples,
          4,
          NSOpenGLPFAColorSize,
          32,
          NSOpenGLPFADepthSize,
          24,
          NSOpenGLPFAStencilSize,
          stencilSize,
          NSOpenGLPFAOpenGLProfile,
          NSOpenGLProfileVersion3_2Core,
          0,
      };
      pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    } else if (config_.backendVersion.majorVersion == 2 &&
               config_.backendVersion.minorVersion == 1) {
      static NSOpenGLPixelFormatAttribute attributes[] = {
          NSOpenGLPFADoubleBuffer,
          NSOpenGLPFAAllowOfflineRenderers,
          NSOpenGLPFAMultisample,
          1,
          NSOpenGLPFASampleBuffers,
          1,
          NSOpenGLPFASamples,
          4,
          NSOpenGLPFAColorSize,
          32,
          NSOpenGLPFADepthSize,
          24,
          NSOpenGLPFAStencilSize,
          stencilSize,
          NSOpenGLPFAOpenGLProfile,
          NSOpenGLProfileVersionLegacy,
          0,
      };
      pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    } else {
      IGL_DEBUG_ABORT("Unsupported OpenGL version: %u.%u\n",
                      config_.backendVersion.majorVersion,
                      config_.backendVersion.minorVersion);
    }
    auto openGLView = [[GLView alloc] initWithFrame:frame_ pixelFormat:pixelFormat];
    igl::Result result;
    auto context = igl::opengl::macos::Context::createContext(openGLView.openGLContext, &result);
    IGL_DEBUG_ASSERT(result.isOk());
    shellPlatform_ = std::make_shared<igl::shell::PlatformMac>(
        opengl::macos::HWDevice().createWithContext(std::move(context), nullptr));

    auto& device = shellPlatform_->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    platformDevice->setNativeDrawableTextureFormat(config_.swapchainColorTextureFormat, nullptr);
    self.view = openGLView;
    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto vulkanView = [[VulkanView alloc] initWithFrame:frame_];

    self.view = vulkanView;

    // vulkanView.wantsLayer =
    // YES; // Back the view with a layer created by the makeBackingLayer method.

    igl::vulkan::VulkanContextConfig vulkanContextConfig;
    vulkanContextConfig.terminateOnValidationError = true;
    vulkanContextConfig.enhancedShaderDebugging = false;
    vulkanContextConfig.enableBufferDeviceAddress = true;

    vulkanContextConfig.swapChainColorSpace = config_.swapchainColorSpace;
    vulkanContextConfig.requestedSwapChainTextureFormat = config_.swapchainColorTextureFormat;

    auto context =
        igl::vulkan::HWDevice::createContext(vulkanContextConfig, (__bridge void*)vulkanView);
    auto devices = igl::vulkan::HWDevice::queryDevices(
        *context, igl::HWDeviceQueryDesc(igl::HWDeviceType::DiscreteGpu), nullptr);
    if (devices.empty()) {
      devices = igl::vulkan::HWDevice::queryDevices(
          *context, igl::HWDeviceQueryDesc(igl::HWDeviceType::IntegratedGpu), nullptr);
    }
    auto device = igl::vulkan::HWDevice::create(
        std::move(context), devices[0], 0, 0, 0, nullptr, nullptr, "IGL Shell", nullptr);

    shellPlatform_ = std::make_shared<igl::shell::PlatformMac>(std::move(device));
    [vulkanView prepareVulkan:shellPlatform_];
    break;
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
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    break;
  }
  }

  session_ = factory_->createRenderSession(shellPlatform_);
  IGL_DEBUG_ASSERT(session_, "createDefaultRenderSession() must return a valid session");
  // Get initial native surface dimensions
  shellParams_.nativeSurfaceDimensions = glm::ivec2(2048, 1536);
  session_->initialize();
}

- (void)viewDidAppear {
  if ([self.view isKindOfClass:[MetalView class]]) {
    MetalView* v = (MetalView*)self.view;
    v.paused = NO;
  } else if ([self.view isKindOfClass:[GLView class]]) {
    GLView* v = (GLView*)self.view;
    [v startTimer];
  }
  [self.view.window makeFirstResponder:self];
}

- (void)viewWillDisappear {
  if ([self.view isKindOfClass:[MetalView class]]) {
    MetalView* v = (MetalView*)self.view;
    v.paused = YES;
  } else if ([self.view isKindOfClass:[GLView class]]) {
    GLView* v = (GLView*)self.view;
    [v stopTimer];
  }
}

- (void)drawInMTKView:(nonnull MTKView*)view {
  @autoreleasepool {
    currentDrawable_ = view.currentDrawable;
    depthStencilTexture_ = view.depthStencilTexture;
    [self render];
    currentDrawable_ = nil;
  }
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
  frame_.size = size;
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeDidChange:(CGSize)size {
}

- (std::shared_ptr<igl::ITexture>)createTextureFromNativeDrawable {
  switch (config_.backendVersion.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto& device = shellPlatform_->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    IGL_DEBUG_ASSERT(currentDrawable_ != nil);
    auto texture = platformDevice->createTextureFromNativeDrawable(currentDrawable_, nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    auto& device = shellPlatform_->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDrawable(nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto& device = shellPlatform_->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::vulkan::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDrawable(nullptr);
    return texture;
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

  default: {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return nullptr;
  }
  }
}

- (std::shared_ptr<igl::ITexture>)createTextureFromNativeDepth {
  switch (config_.backendVersion.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto& device = shellPlatform_->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDepth(depthStencilTexture_, nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    auto& device = shellPlatform_->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDepth(nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto& device = static_cast<igl::vulkan::Device&>(shellPlatform_->getDevice());
    auto extents = device.getVulkanContext().getSwapchainExtent();
    auto* platformDevice =
        shellPlatform_->getDevice().getPlatformDevice<igl::vulkan::PlatformDevice>();

    IGL_DEBUG_ASSERT(platformDevice);
    auto texture =
        platformDevice->createTextureFromNativeDepth(extents.width, extents.height, nullptr);
    return texture;
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

  default: {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return nullptr;
  }
  }
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (NSPoint)locationForEvent:(NSEvent*)event {
  NSRect contentRect = self.view.frame;
  NSPoint pos = [event locationInWindow];
  return NSMakePoint(pos.x, contentRect.size.height - pos.y);
}

static uint32_t getModifiers(NSEvent* event) {
  uint32_t modifiers = igl::shell::KeyEventModifierNone;
  const NSUInteger flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

  if (flags & NSEventModifierFlagShift) {
    modifiers |= igl::shell::KeyEventModifierShift;
  }
  if (flags & NSEventModifierFlagCapsLock) {
    modifiers |= igl::shell::KeyEventModifierCapsLock;
  }
  if (flags & NSEventModifierFlagControl) {
    modifiers |= igl::shell::KeyEventModifierControl;
  }
  if (flags & NSEventModifierFlagOption) {
    modifiers |= igl::shell::KeyEventModifierOption;
  }
  if (flags & NSCommandKeyMask) {
    modifiers |= igl::shell::KeyEventModifierCommand;
  }
  if (flags & NSEventModifierFlagNumericPad) {
    modifiers |= igl::shell::KeyEventModifierNumLock;
  }
  return modifiers;
}

- (void)keyUp:(NSEvent*)event {
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::KeyEvent(false, event.keyCode, getModifiers(event)));
}

- (void)keyDown:(NSEvent*)event {
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::KeyEvent(true, event.keyCode, getModifiers(event)));
  std::string characters([event.characters UTF8String]);
  for (const auto& c : characters) {
    shellPlatform_->getInputDispatcher().queueEvent(igl::shell::CharEvent{.character = c});
  }
}

- (void)mouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Left, true, curPoint.x, curPoint.y));
}

- (void)rightMouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Right, true, curPoint.x, curPoint.y));
}

- (void)otherMouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Middle, true, curPoint.x, curPoint.y));
}

- (void)mouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Left, false, curPoint.x, curPoint.y));
}

- (void)rightMouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Right, false, curPoint.x, curPoint.y));
}

- (void)otherMouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Middle, false, curPoint.x, curPoint.y));
}

- (void)mouseMoved:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)mouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)rightMouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)otherMouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  shellPlatform_->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)scrollWheel:(NSEvent*)event {
  shellPlatform_->getInputDispatcher().queueEvent(igl::shell::MouseWheelEvent(
      event.scrollingDeltaX * kMouseSpeed_, event.scrollingDeltaY * kMouseSpeed_));
}

- (CGRect)frame {
  return frame_;
}

- (igl::ColorSpace)colorSpace {
  return config_.swapchainColorSpace;
}

@end
