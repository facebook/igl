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
// @fb-only

#import <AppKit/NSApplication.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSOpenGLView.h>
#import <AppKit/NSView.h>
#import <shell/shared/input/InputDispatcher.h>
#include <shell/shared/platform/Platform.h>
#import <igl/Common.h>
#import <igl/IGL.h>
#if IGL_BACKEND_METAL
#include <igl/metal/ColorSpace.h>
#include <igl/metal/HWDevice.h>
#include <igl/metal/Texture.h>
#include <igl/metal/macos/Device.h>
#endif
#if IGL_BACKEND_OPENGL
#include <igl/opengl/macos/Context.h>
#include <igl/opengl/macos/HWDevice.h>
#include <igl/opengl/macos/PlatformDevice.h>
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
#import "VulkanView.h"

#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#endif
#import <cmath>
#import <simd/simd.h>

using namespace igl;

@interface ViewController () {
  igl::shell::IRenderSessionFactory* _factory;
  igl::shell::RenderSessionConfig _config;
  igl::shell::ShellParams _shellParams;
  CGRect _frame;
  CVDisplayLinkRef _displayLink; // For OpenGL only
  id<CAMetalDrawable> _currentDrawable;
  id<MTLTexture> _depthStencilTexture;
  std::shared_ptr<igl::shell::Platform> _shellPlatform;
  std::unique_ptr<igl::shell::RenderSession> _session;
  float _kMouseSpeed;
}
@end

@implementation ViewController

@synthesize iglView = _iglView;

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

  self->_config = std::move(config);
  self->_factory = &factory;
  self->_shellParams = igl::shell::ShellParams();
  self->_shellParams.viewportSize.x = frame.size.width;
  self->_shellParams.viewportSize.y = frame.size.height;
  self->_frame = frame;
  self->_kMouseSpeed = 0.05f;
  self->_currentDrawable = nil;
  self->_depthStencilTexture = nil;

  return self;
}

- (void)initModule {
}

- (void)teardown {
  if (_session) {
    _session->teardown();
  }
  _session = nullptr;
  _shellPlatform = nullptr;
}

- (void)render {
  if (_session == nullptr) {
    return;
  }

  const NSRect contentRect = self.view.frame;

  _shellParams.viewportSize = glm::vec2(contentRect.size.width, contentRect.size.height);
  _shellParams.viewportScale = self.view.window.backingScaleFactor;
  _session->setShellParams(_shellParams);
  // process user input
  _shellPlatform->getInputDispatcher().processEvents();

  igl::SurfaceTextures surfaceTextures;
  if (_config.backendVersion.flavor != igl::BackendFlavor::Invalid &&
      _shellPlatform->getDevicePtr() != nullptr) {
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
    surfaceTextures = igl::SurfaceTextures{.color = [self createTextureFromNativeDrawable],
                                           .depth = [self createTextureFromNativeDepth]};
    IGL_DEBUG_ASSERT(surfaceTextures.color != nullptr && surfaceTextures.depth != nullptr);
    const auto& dims = surfaceTextures.color->getDimensions();
    _shellParams.nativeSurfaceDimensions = glm::ivec2{dims.width, dims.height};

    // update retina scale
    float pixelsPerPoint = _shellParams.nativeSurfaceDimensions.x / _shellParams.viewportSize.x;
    _session->setPixelsPerPoint(pixelsPerPoint);
// @fb-only
    // @fb-only
    // @fb-only
        // @fb-only
    // @fb-only
// @fb-only
  }

  // Use runUpdate() which automatically handles benchmark timing, reporting, and expiration
  _session->runUpdate(std::move(surfaceTextures));

  if (_session->appParams().exitRequested) {
    [[NSApplication sharedApplication] terminate:nil];
  }
}

- (void)loadView {
  // We don't care about the device type, just
  // return something that works
  HWDeviceQueryDesc queryDesc(HWDeviceType::Unknown);

  switch (_config.backendVersion.flavor) {
  case igl::BackendFlavor::Invalid: {
    auto headlessView = [[HeadlessView alloc] initWithFrame:_frame];
    self.view = headlessView;

    // @fb-only
        // @fb-only

    // Headless platform does not run on a real device
    _shellPlatform = std::make_shared<igl::shell::PlatformMac>(nullptr);

    [headlessView prepareHeadless];
    break;
  }

#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto hwDevices = metal::HWDevice().queryDevices(queryDesc, nullptr);
    auto device = metal::HWDevice().create(hwDevices[0], nullptr);

    auto d = static_cast<igl::metal::macos::Device*>(device.get())->get();

    auto metalView = [[MetalView alloc] initWithFrame:_frame device:d];
    metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

    metalView.delegate = self;

    metalView.colorPixelFormat =
        metal::Texture::textureFormatToMTLPixelFormat(_config.swapchainColorTextureFormat);
    metalView.colorspace = metal::colorSpaceToCGColorSpace(_config.swapchainColorSpace);

    metalView.framebufferOnly = NO;
    [metalView setViewController:self];
    self.view = metalView;
    _shellPlatform = std::make_shared<igl::shell::PlatformMac>(std::move(device));
    break;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    const bool enableStencilBuffer =
        _config.depthTextureFormat == igl::TextureFormat::S8_UInt_Z24_UNorm ||
        _config.depthTextureFormat == igl::TextureFormat::S_UInt8;
    const NSOpenGLPixelFormatAttribute stencilSize = enableStencilBuffer ? 8 : 0;

    NSOpenGLPixelFormat* pixelFormat;
    if (_config.backendVersion.majorVersion == 4 && _config.backendVersion.minorVersion == 1) {
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
    } else if (_config.backendVersion.majorVersion == 3 &&
               _config.backendVersion.minorVersion == 2) {
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
    } else if (_config.backendVersion.majorVersion == 2 &&
               _config.backendVersion.minorVersion == 1) {
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
                      _config.backendVersion.majorVersion,
                      _config.backendVersion.minorVersion);
    }
    auto openGLView = [[GLView alloc] initWithFrame:_frame pixelFormat:pixelFormat];
    igl::Result result;
    auto context = igl::opengl::macos::Context::createContext(openGLView.openGLContext, &result);
    IGL_DEBUG_ASSERT(result.isOk());
    _shellPlatform = std::make_shared<igl::shell::PlatformMac>(
        opengl::macos::HWDevice().createWithContext(std::move(context), nullptr));

    auto& device = _shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    platformDevice->setNativeDrawableTextureFormat(_config.swapchainColorTextureFormat, nullptr);
    self.view = openGLView;
    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto vulkanView = [[VulkanView alloc] initWithFrame:_frame];

    self.view = vulkanView;

    // vulkanView.wantsLayer =
    // YES; // Back the view with a layer created by the makeBackingLayer method.

    igl::vulkan::VulkanContextConfig vulkanContextConfig;
    vulkanContextConfig.terminateOnValidationError = true;
    vulkanContextConfig.enhancedShaderDebugging = false;

    vulkanContextConfig.swapChainColorSpace = _config.swapchainColorSpace;
    vulkanContextConfig.requestedSwapChainTextureFormat = _config.swapchainColorTextureFormat;

    auto context =
        igl::vulkan::HWDevice::createContext(vulkanContextConfig, (__bridge void*)vulkanView);
    auto devices = igl::vulkan::HWDevice::queryDevices(
        *context, igl::HWDeviceQueryDesc(igl::HWDeviceType::DiscreteGpu), nullptr);
    if (devices.empty()) {
      devices = igl::vulkan::HWDevice::queryDevices(
          *context, igl::HWDeviceQueryDesc(igl::HWDeviceType::IntegratedGpu), nullptr);
    }
    if (devices.empty()) {
      devices = igl::vulkan::HWDevice::queryDevices(
          *context, igl::HWDeviceQueryDesc(igl::HWDeviceType::SoftwareGpu), nullptr);
    }
    auto device = igl::vulkan::HWDevice::create(
        std::move(context), devices[0], 0, 0, 0, nullptr, nullptr, "IGL Shell", nullptr);

    _shellPlatform = std::make_shared<igl::shell::PlatformMac>(std::move(device));
    [vulkanView prepareVulkan:_shellPlatform.get()];
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

  _session = _factory->createRenderSession(_shellPlatform);
  IGL_DEBUG_ASSERT(_session, "createDefaultRenderSession() must return a valid session");
  // Get initial native surface dimensions
  _shellParams.nativeSurfaceDimensions = glm::ivec2(2048, 1536);
  auto args =
      shell::convertArgvToParams(igl::shell::Platform::argc(), igl::shell::Platform::argv());
  shell::parseShellParams(args, _shellParams);

  _session->setShellParams(_shellParams);
  _session->initialize();
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
    _currentDrawable = view.currentDrawable;
    _depthStencilTexture = view.depthStencilTexture;
    [self render];
    _currentDrawable = nil;
  }
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
  _frame.size = size;
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeDidChange:(CGSize)size {
}

- (std::shared_ptr<igl::ITexture>)createTextureFromNativeDrawable {
  switch (_config.backendVersion.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto& device = _shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    IGL_DEBUG_ASSERT(_currentDrawable != nil);
    auto texture = platformDevice->createTextureFromNativeDrawable(_currentDrawable, nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    auto& device = _shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDrawable(nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto& device = _shellPlatform->getDevice();
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
  switch (_config.backendVersion.flavor) {
#if IGL_BACKEND_METAL
  case igl::BackendFlavor::Metal: {
    auto& device = _shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::metal::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDepth(_depthStencilTexture, nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL: {
    auto& device = _shellPlatform->getDevice();
    auto* platformDevice = device.getPlatformDevice<igl::opengl::macos::PlatformDevice>();
    IGL_DEBUG_ASSERT(platformDevice);
    auto texture = platformDevice->createTextureFromNativeDepth(nullptr);
    return texture;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto& device = static_cast<igl::vulkan::Device&>(_shellPlatform->getDevice());
    auto extents = device.getVulkanContext().getSwapchainExtent();
    auto* platformDevice =
        _shellPlatform->getDevice().getPlatformDevice<igl::vulkan::PlatformDevice>();

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
  uint32_t modifiers = igl::shell::kKeyEventModifierNone;
  const NSUInteger flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

  if (flags & NSEventModifierFlagShift) {
    modifiers |= igl::shell::kKeyEventModifierShift;
  }
  if (flags & NSEventModifierFlagCapsLock) {
    modifiers |= igl::shell::kKeyEventModifierCapsLock;
  }
  if (flags & NSEventModifierFlagControl) {
    modifiers |= igl::shell::kKeyEventModifierControl;
  }
  if (flags & NSEventModifierFlagOption) {
    modifiers |= igl::shell::kKeyEventModifierOption;
  }
  if (flags & NSEventModifierFlagCommand) {
    modifiers |= igl::shell::kKeyEventModifierCommand;
  }
  if (flags & NSEventModifierFlagNumericPad) {
    modifiers |= igl::shell::kKeyEventModifierNumLock;
  }
  return modifiers;
}

- (void)keyUp:(NSEvent*)event {
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::KeyEvent(false, event.keyCode, getModifiers(event)));
}

- (void)keyDown:(NSEvent*)event {
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::KeyEvent(true, event.keyCode, getModifiers(event)));
  std::string characters([event.characters UTF8String]);
  for (const auto& c : characters) {
    _shellPlatform->getInputDispatcher().queueEvent(igl::shell::CharEvent{.character = c});
  }
}

- (void)mouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Left, true, curPoint.x, curPoint.y));
}

- (void)rightMouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Right, true, curPoint.x, curPoint.y));
}

- (void)otherMouseDown:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Middle, true, curPoint.x, curPoint.y));
}

- (void)mouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Left, false, curPoint.x, curPoint.y));
}

- (void)rightMouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Right, false, curPoint.x, curPoint.y));
}

- (void)otherMouseUp:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseButtonEvent(igl::shell::MouseButton::Middle, false, curPoint.x, curPoint.y));
}

- (void)mouseMoved:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)mouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)rightMouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)otherMouseDragged:(NSEvent*)event {
  NSPoint curPoint = [self locationForEvent:event];
  _shellPlatform->getInputDispatcher().queueEvent(
      igl::shell::MouseMotionEvent(curPoint.x, curPoint.y, event.deltaX, event.deltaY));
}

- (void)scrollWheel:(NSEvent*)event {
  _shellPlatform->getInputDispatcher().queueEvent(igl::shell::MouseWheelEvent(
      event.scrollingDeltaX * _kMouseSpeed, event.scrollingDeltaY * _kMouseSpeed));
}

- (CGRect)frame {
  return _frame;
}

- (igl::ColorSpace)colorSpace {
  return _config.swapchainColorSpace;
}

@end
