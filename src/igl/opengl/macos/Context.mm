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
    IGL_ASSERT_MSG(pixelFormat, "Requested attributes not supported");
    if (pixelFormat) {
      format = pixelFormat;
    }
  }
  return [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
}
} // namespace

///--------------------------------------
/// MARK: - Context

std::unique_ptr<Context> Context::createContext(igl::opengl::RenderingAPI api, Result* outResult) {
  return createContext(createOpenGLContext(api), {}, outResult);
}

std::unique_ptr<Context> Context::createContext(NSOpenGLContext* context, Result* outResult) {
  return createContext(context, {}, outResult);
}

/*static*/ std::unique_ptr<Context> Context::createShareContext(Context& existingContext,
                                                                Result* outResult) {
  auto existingNSContext = existingContext.getNSContext();
  auto newGLContext = [[NSOpenGLContext alloc] initWithFormat:existingNSContext.pixelFormat
                                                 shareContext:existingNSContext];

  auto sharegroup = existingContext.sharegroup_;
  if (!sharegroup) {
    sharegroup = std::make_shared<std::vector<NSOpenGLContext*>>();
    sharegroup->reserve(2);
    sharegroup->emplace_back(existingNSContext);
  }

  igl::Result result;
  auto context = std::unique_ptr<Context>(new Context(newGLContext, sharegroup));
  context->initialize(&result);

  // If we are successful - add the new context back to our sharegroup.
  if (result.isOk()) {
    sharegroup->emplace_back(newGLContext);
    existingContext.sharegroup_ = std::move(sharegroup);
  }

  Result::setResult(outResult, result);
  return context;
}

std::unique_ptr<Context> Context::createContext(NSOpenGLContext* context,
                                                std::vector<NSOpenGLContext*> shareContexts,
                                                Result* outResult) {
  if (context == nullptr) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "NSOpenGLContext was null");
    return {};
  }

  auto newContext = std::unique_ptr<Context>(new Context(
      context, std::make_shared<std::vector<NSOpenGLContext*>>(std::move(shareContexts))));
  newContext->initialize(outResult);
  return newContext;
}

Context::Context(NSOpenGLContext* context,
                 std::shared_ptr<std::vector<NSOpenGLContext*>> shareContexts) :
  context_(context), sharegroup_(std::move(shareContexts)) {
  IContext::registerContext((__bridge void*)context_, this);
}

Context::~Context() {
  // Clear pool explicitly, since it might have reference back to IContext.
  getAdapterPool().clear();
  // Unregister NSOpenGLContext
  IContext::unregisterContext((__bridge void*)context_);
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
  if (sharegroup_) {
    const auto& sharegroup = *sharegroup_;
    auto it = std::find(sharegroup.begin(), sharegroup.end(), [NSOpenGLContext currentContext]);
    return it != sharegroup.end();
  }
  return false;
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
  IGL_ASSERT_MSG(format, "Requested attributes not supported");
  return format;
}

NSOpenGLContext* Context::getNSContext() {
  return context_;
}

CVOpenGLTextureCacheRef Context::createTextureCache() {
  CVOpenGLTextureCacheRef textureCache = nullptr;
  CVReturn result = CVOpenGLTextureCacheCreate(kCFAllocatorDefault,
                                               nullptr,
                                               context_.CGLContextObj,
                                               context_.pixelFormat.CGLPixelFormatObj,
                                               nullptr,
                                               &textureCache);
  if (result != kCVReturnSuccess) {
    IGL_ASSERT_MSG(false, "CVOpenGLTextureCacheCreate failed to create texture cache");
  }
  return textureCache;
}

} // namespace igl::opengl::macos
