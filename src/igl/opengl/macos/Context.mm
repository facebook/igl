/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/macos/Context.h>

#include <AppKit/NSOpenGL.h>
#include <algorithm>
#include <memory>
#include <utility>

// NOLINTBEGIN(clang-diagnostic-deprecated-declarations)
namespace igl::opengl::macos {

namespace {
NSOpenGLContext* createOpenGLContext(BackendVersion backendVersion) {
  IGL_DEBUG_ASSERT(backendVersion.flavor == BackendFlavor::OpenGL);
  IGL_DEBUG_ASSERT((backendVersion.majorVersion == 3 && backendVersion.minorVersion == 2) ||
                   (backendVersion.majorVersion == 4 && backendVersion.minorVersion == 1));
  NSOpenGLPixelFormat* format = nil;

  if (backendVersion.majorVersion == 3 && backendVersion.minorVersion == 2) {
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
        NSOpenGLPFAOpenGLProfile,
        NSOpenGLProfileVersion3_2Core,
        0,
    };
    format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
  } else if (backendVersion.majorVersion == 4 && backendVersion.minorVersion == 1) {
    // Copied from preferredPixelFormat(), with NSOpenGLProfileVersion4_1Core added
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
        NSOpenGLPFAOpenGLProfile,
        NSOpenGLProfileVersion4_1Core,
        0,
    };
    format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
  }

  if (format == nil) {
    // Last resort only: preferredPixelFormat() demands an accelerated, window-capable
    // renderer, which a host with no window server session (headless CI Mac) does not
    // have. Requesting it up-front makes context creation fail on such hosts even when
    // the requested core-profile attributes above are satisfiable. The context handed
    // back here is not a core profile one, so callers can see behavior that does not
    // match the version they asked for.
    IGL_LOG_ERROR("Requested attributes not supported for OpenGL %d.%d; falling back\n",
                  static_cast<int>(backendVersion.majorVersion),
                  static_cast<int>(backendVersion.minorVersion));
    format = Context::preferredPixelFormat();
  }
  IGL_DEBUG_ASSERT(format, "Requested attributes not supported");

  return [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
}
} // namespace

///--------------------------------------
/// MARK: - Context

std::unique_ptr<IContext> Context::createShareContext(Result* outResult) {
  return createShareContext(*this, outResult);
}

std::unique_ptr<Context> Context::createContext(BackendVersion backendVersion, Result* outResult) {
  return createContext(createOpenGLContext(backendVersion), {}, outResult);
}

std::unique_ptr<Context> Context::createContext(NSOpenGLContext* context, Result* outResult) {
  return createContext(context, {}, outResult);
}

std::unique_ptr<Context> Context::createShareContext(Context& existingContext, Result* outResult) {
  auto existingNSContext = existingContext.getNSContext();
  auto newGLContext = [[NSOpenGLContext alloc] initWithFormat:existingNSContext.pixelFormat
                                                 shareContext:existingNSContext];

  IGL_DEBUG_ASSERT(existingContext.sharegroup_, "Sharegroup must exist");

  Result result;
  auto context = std::unique_ptr<Context>(new Context(newGLContext, existingContext.sharegroup_));
  context->initialize(&result);

  // If we are successful, add the new context to our sharegroup.
  if (result.isOk()) {
    context->sharegroup_->push_back(newGLContext);
  } else {
    context = nullptr;
  }

  Result::setResult(outResult, result);
  return context;
}

std::unique_ptr<Context> Context::createContext(
    NSOpenGLContext* context,
    std::shared_ptr<std::vector<NSOpenGLContext*>> shareContexts,
    Result* outResult) {
  if (context == nullptr) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "NSOpenGLContext was null");
    return {};
  }

  Result result;
  auto newContext = std::unique_ptr<Context>(new Context(context, std::move(shareContexts)));
  newContext->initialize(&result);

  // If we are successful, add the new context to our sharegroup.
  if (result.isOk()) {
    newContext->sharegroup_->push_back(context);
  } else {
    newContext = nullptr;
  }

  Result::setResult(outResult, result);
  return newContext;
}

Context::Context(NSOpenGLContext* context,
                 std::shared_ptr<std::vector<NSOpenGLContext*>> shareContexts) :
  context_(context), sharegroup_(std::move(shareContexts)) {
  if (!sharegroup_) {
    sharegroup_ = std::make_shared<std::vector<NSOpenGLContext*>>();
  }
  // Note that we're not adding the context to the sharegroup yet. It'll only be done by the
  // callers, after the new context is initialized successfully.

  IContext::registerContext((__bridge void*)context_, this);
}

Context::~Context() {
  willDestroy((__bridge void*)context_);
}

void Context::present(std::shared_ptr<ITexture> /*surface*/) const {
  [context_ flushBuffer];
}

void Context::setCurrent() {
  [context_ makeCurrentContext];
  flushDeletionQueue();
}

void Context::clearCurrentContext() const {
  [NSOpenGLContext clearCurrentContext];
}

bool Context::isCurrentContext() const {
  return [NSOpenGLContext currentContext] == context_;
}

bool Context::isCurrentSharegroup() const {
  IGL_DEBUG_ASSERT(sharegroup_ != nullptr, "Sharegroup must exist");
  auto it = std::find(sharegroup_->begin(), sharegroup_->end(), [NSOpenGLContext currentContext]);
  return it != sharegroup_->end();
}

NSOpenGLPixelFormat* Context::preferredPixelFormat() {
  static NSOpenGLPixelFormatAttribute attributes[] = {
      NSOpenGLPFAWindow,
      NSOpenGLPFAAccelerated,
      // Allow the system to fall back to an offline (e.g. headless / not
      // display-attached) renderer. Without this, an accelerated+window pixel
      // format can intermittently fail to allocate on headless or GPU-contended
      // hosts (CI Macs). The sibling 3.2/4.1 attribute lists in
      // createOpenGLContext() already set this.
      NSOpenGLPFAAllowOfflineRenderers,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAColorSize,
      24,
      NSOpenGLPFAAlphaSize,
      8,
      NSOpenGLPFADepthSize,
      24,
      NSOpenGLPFAStencilSize,
      8,
      0,
  };
  NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
  if (format == nil) {
    // A host with no window server session has no window-capable accelerated renderer
    // at all, so drop both requirements and let the system pick a software renderer
    // rather than handing the caller a nil pixel format. No profile attribute is
    // requested here, so this is a legacy context, same as the list above.
    static NSOpenGLPixelFormatAttribute headlessAttributes[] = {
        NSOpenGLPFAAllowOfflineRenderers,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize,
        24,
        NSOpenGLPFAAlphaSize,
        8,
        NSOpenGLPFADepthSize,
        24,
        NSOpenGLPFAStencilSize,
        8,
        0,
    };
    format = [[NSOpenGLPixelFormat alloc] initWithAttributes:headlessAttributes];
  }
  IGL_DEBUG_ASSERT(format, "Requested attributes not supported");
  return format;
}

NSOpenGLContext* Context::getNSContext() {
  return context_;
}

CVOpenGLTextureCacheRef Context::createTextureCache() {
  CVOpenGLTextureCacheRef textureCache = nullptr;
  CGLContextObj cglContext = context_.CGLContextObj;
  CGLPixelFormatObj cglPixelFormat = context_.pixelFormat.CGLPixelFormatObj;
  if (cglContext == nullptr || cglPixelFormat == nullptr) {
    IGL_DEBUG_ABORT("CGLContextObj or CGLPixelFormatObj is null");
    return nullptr;
  }
  const CVReturn result = CVOpenGLTextureCacheCreate(
      kCFAllocatorDefault, nullptr, cglContext, cglPixelFormat, nullptr, &textureCache);
  if (result != kCVReturnSuccess) {
    IGL_DEBUG_ABORT("CVOpenGLTextureCacheCreate failed to create texture cache");
  }
  return textureCache;
}

} // namespace igl::opengl::macos
// NOLINTEND(clang-diagnostic-deprecated-declarations)
