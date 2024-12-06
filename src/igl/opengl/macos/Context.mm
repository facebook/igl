/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/macos/Context.h>

#include <AppKit/NSOpenGL.h>
#include <memory>
#include <utility>

namespace igl::opengl::macos {

namespace {
NSOpenGLContext* createOpenGLContext(igl::opengl::RenderingAPI api) {
  auto format = Context::preferredPixelFormat();

  if (api == igl::opengl::RenderingAPI::GLES3) {
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
    auto pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    IGL_DEBUG_ASSERT(pixelFormat, "Requested attributes not supported");
    if (pixelFormat) {
      format = pixelFormat;
    }
  } else if (api == igl::opengl::RenderingAPI::GL) {
    // Copied from preferredPixelFormat, with NSOpenGLProfileVersion4_1Core added
    static NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFAWindow,
        NSOpenGLPFAAccelerated,
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
        NSOpenGLPFAOpenGLProfile,
        NSOpenGLProfileVersion4_1Core,
    };
    auto pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    IGL_DEBUG_ASSERT(pixelFormat, "Requested attributes not supported");
    if (pixelFormat) {
      format = pixelFormat;
    }
  }
  return [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
}
} // namespace

///--------------------------------------
/// MARK: - Context

std::unique_ptr<IContext> Context::createShareContext(Result* outResult) {
  return createShareContext(*this, outResult);
}

std::unique_ptr<Context> Context::createContext(igl::opengl::RenderingAPI api, Result* outResult) {
  return createContext(createOpenGLContext(api), {}, outResult);
}

std::unique_ptr<Context> Context::createContext(NSOpenGLContext* context, Result* outResult) {
  return createContext(context, {}, outResult);
}

std::unique_ptr<Context> Context::createShareContext(Context& existingContext, Result* outResult) {
  auto existingNSContext = existingContext.getNSContext();
  auto newGLContext = [[NSOpenGLContext alloc] initWithFormat:existingNSContext.pixelFormat
                                                 shareContext:existingNSContext];

  IGL_DEBUG_ASSERT(existingContext.sharegroup_, "Sharegroup must exist");

  igl::Result result;
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

  igl::Result result;
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
  IGL_DEBUG_ASSERT(format, "Requested attributes not supported");
  return format;
}

NSOpenGLContext* Context::getNSContext() {
  return context_;
}

CVOpenGLTextureCacheRef Context::createTextureCache() {
  CVOpenGLTextureCacheRef textureCache = nullptr;
  const CVReturn result = CVOpenGLTextureCacheCreate(kCFAllocatorDefault,
                                                     nullptr,
                                                     context_.CGLContextObj,
                                                     context_.pixelFormat.CGLPixelFormatObj,
                                                     nullptr,
                                                     &textureCache);
  if (result != kCVReturnSuccess) {
    IGL_DEBUG_ABORT("CVOpenGLTextureCacheCreate failed to create texture cache");
  }
  return textureCache;
}

} // namespace igl::opengl::macos
