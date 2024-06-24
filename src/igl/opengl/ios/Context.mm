/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/ios/Context.h>

#import <Foundation/Foundation.h>

#include <OpenGLES/EAGL.h>
#include <QuartzCore/CAEAGLLayer.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/Texture.h>
#import <objc/runtime.h>

namespace igl::opengl::ios {
namespace {
EAGLContext* createEAGLContext(RenderingAPI api, EAGLSharegroup* sharegroup) {
  if (api == RenderingAPI::GLES3) {
    EAGLContext* context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3
                                                 sharegroup:sharegroup];
    if (context == nullptr) {
      return [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2 sharegroup:sharegroup];
    }
    return context;
  } else {
    IGL_ASSERT_MSG(api == RenderingAPI::GLES2,
                   "IGL: unacceptable enum for rendering API for iOS\n");
    return [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2 sharegroup:sharegroup];
  }
}

void* getOrGenerateContextUniqueID(EAGLContext* context) {
  static const void* uniqueIdKey = &uniqueIdKey;
  static uint64_t idCounter = 0;
  NSNumber* key = objc_getAssociatedObject(context, &uniqueIdKey);
  uint64_t contextId;
  if (key == nullptr) {
    // Generate and set id if it doesn't exist
    contextId = idCounter++;
    objc_setAssociatedObject(context, &uniqueIdKey, @(contextId), OBJC_ASSOCIATION_RETAIN);
  } else {
    contextId = key.integerValue;
  }
  return (void*)contextId;
}
} // namespace

Context::Context(RenderingAPI api) : context_(createEAGLContext(api, nil)) {
  if (context_ != nil) {
    IContext::registerContext(getOrGenerateContextUniqueID(context_), this);
  }

  initialize();
}

Context::Context(RenderingAPI api, Result* result) : context_(createEAGLContext(api, nil)) {
  if (context_ != nil) {
    IContext::registerContext(getOrGenerateContextUniqueID(context_), this);
  } else {
    Result::setResult(result, Result::Code::ArgumentInvalid);
  }

  initialize(result);
}

Context::Context(EAGLContext* context) : context_(context) {
  if (context_ != nil) {
    IContext::registerContext(getOrGenerateContextUniqueID(context_), this);
  }
  initialize();
}

Context::Context(EAGLContext* context, Result* result) : context_(context) {
  if (context_ != nil) {
    IContext::registerContext(getOrGenerateContextUniqueID(context_), this);
  } else {
    Result::setResult(result, Result::Code::ArgumentInvalid);
  }
  initialize(result);
}

Context::~Context() {
  // Release CVOpenGLESTextureCacheRef
  if (textureCache_ != nullptr) {
    CVOpenGLESTextureCacheFlush(textureCache_, 0);
    CFRelease(textureCache_);
  }
  willDestroy(context_ == nil ? nullptr : getOrGenerateContextUniqueID(context_));

  // Unregister EAGLContext
  if (context_ != nil) {
    if (context_ == [EAGLContext currentContext]) {
      [EAGLContext setCurrentContext:nil];
    }
  }
}

void Context::present(std::shared_ptr<ITexture> surface) const {
  static_cast<Texture&>(*surface).bind();
  [context_ presentRenderbuffer:GL_RENDERBUFFER];
}

void Context::setCurrent() {
  [EAGLContext setCurrentContext:context_];
  flushDeletionQueue();
}

void Context::clearCurrentContext() const {
  [EAGLContext setCurrentContext:nil];
}

bool Context::isCurrentContext() const {
  return [EAGLContext currentContext] == context_;
}

bool Context::isCurrentSharegroup() const {
  return [EAGLContext currentContext].sharegroup == context_.sharegroup;
}

std::unique_ptr<IContext> Context::createShareContext(Result* outResult) {
  EAGLContext* sharedContext = [[EAGLContext alloc] initWithAPI:context_.API
                                                     sharegroup:context_.sharegroup];
  if (!sharedContext) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create shared context");
    return nullptr;
  }
  Result::setOk(outResult);
  return std::make_unique<Context>(sharedContext);
}

CVOpenGLESTextureCacheRef Context::getTextureCache() {
  if (textureCache_ == nullptr) {
    CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, nullptr, context_, nullptr, &textureCache_);
  }
  return textureCache_;
}

} // namespace igl::opengl::ios
