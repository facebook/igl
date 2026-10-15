/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/TextureTarget.h>

#include <igl/Macros.h>

namespace igl::opengl {

TextureTarget::~TextureTarget() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  if (renderBufferID_ != 0) {
    getContext().deleteRenderbuffers(1, &renderBufferID_);
  }
  releaseImplicitRenderBuffer();
}

TextureType TextureTarget::getType() const {
  return TextureType::TwoD;
}

TextureDesc::TextureUsage TextureTarget::getUsage() const {
  return usage_;
}

bool TextureTarget::canPresent() const noexcept {
  return canPresent_;
}

// create a 2D texture given the specified dimensions and format
Result TextureTarget::create(const TextureDesc& desc, bool hasStorageAlready) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  Result result = Super::create(desc, hasStorageAlready);
  if (result.isOk()) {
    if (desc.usage & TextureDesc::TextureUsageBits::Attachment) {
      usage_ = desc.usage;
      result = createRenderBuffer(desc, hasStorageAlready);
    } else {
      result = Result(Result::Code::Unsupported, "invalid usage!");
    }
  }
  return result;
}

// create a render buffer for render target usages
Result TextureTarget::createRenderBuffer(const TextureDesc& desc, bool hasStorageAlready) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (desc.type != TextureType::TwoD) {
    // Renderbuffers only support 2D textures
    return Result{Result::Code::Unsupported, "Texture type must be TwoD."};
  }
  if (desc.numMipLevels > 1) {
    return Result{Result::Code::Unsupported, "numMipLevels must be 1."};
  }

  if (!toRenderBufferFormatGL(desc.usage, glInternalFormat_)) {
    // can't create a texture with the given format
    return Result{Result::Code::ArgumentInvalid, "Invalid texture format"};
  }

  debugName_ = desc.debugName;

  // create the GL render buffer
  getContext().genRenderbuffers(1, &renderBufferID_);

  if (!hasStorageAlready) {
    getContext().bindRenderbuffer(GL_RENDERBUFFER, renderBufferID_);

    needsStorage_ = desc.storage == ResourceStorage::Memoryless;
    if (!needsStorage_) {
      specifyStorage();
    }
    labelRenderBuffer(renderBufferID_);

    getContext().bindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  return Result();
}

void TextureTarget::specifyStorage() const {
  if (getSamples() > 1) {
    getContext().renderbufferStorageMultisample(GL_RENDERBUFFER,
                                                static_cast<GLsizei>(getSamples()),
                                                glInternalFormat_,
                                                getWidth(),
                                                getHeight());
  } else {
    getContext().renderbufferStorage(GL_RENDERBUFFER, glInternalFormat_, getWidth(), getHeight());
  }
}

void TextureTarget::labelRenderBuffer(GLuint renderBufferId) const {
  if (!debugName_.empty() &&
      getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugLabel)) {
    getContext().objectLabel(GL_RENDERBUFFER,
                             renderBufferId,
                             static_cast<GLsizei>(debugName_.size()),
                             debugName_.c_str());
  }
}

void TextureTarget::createImplicitRenderBuffer(uint32_t samples) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  getContext().genRenderbuffers(1, &implicitRenderBufferID_);
  getContext().bindRenderbuffer(GL_RENDERBUFFER, implicitRenderBufferID_);
  getContext().renderbufferStorageMultisampleImplicit(
      GL_RENDERBUFFER, static_cast<GLsizei>(samples), glInternalFormat_, getWidth(), getHeight());
  labelRenderBuffer(implicitRenderBufferID_);
  getContext().bindRenderbuffer(GL_RENDERBUFFER, 0);
}

void TextureTarget::releaseImplicitRenderBuffer() {
  if (implicitRenderBufferID_ != 0) {
    getContext().deleteRenderbuffers(1, &implicitRenderBufferID_);
    implicitRenderBufferID_ = 0;
  }
}

GLuint TextureTarget::prepareRenderBuffer(const AttachmentParams& params) {
  IGL_PROFILER_FUNCTION();
  if (params.implicitSamples > 1) {
    // A framebuffer that resolves implicitly attaches only textures with its number of samples
    IGL_DEBUG_ASSERT(params.implicitSamples == getSamples());
    if (implicitRenderBufferID_ == 0) {
      createImplicitRenderBuffer(params.implicitSamples);
    }
    return implicitRenderBufferID_;
  }
  // A framebuffer that attaches the multisampled renderbuffer has no use for the implicit one
  releaseImplicitRenderBuffer();
  if (needsStorage_) {
    getContext().bindRenderbuffer(GL_RENDERBUFFER, renderBufferID_);
    specifyStorage();
    getContext().bindRenderbuffer(GL_RENDERBUFFER, 0);
    needsStorage_ = false;
  }
  return renderBufferID_;
}

void TextureTarget::bind() {
  IGL_PROFILER_FUNCTION();
  getContext().bindRenderbuffer(GL_RENDERBUFFER, renderBufferID_);
}

void TextureTarget::unbind() {
  IGL_PROFILER_FUNCTION();
  getContext().bindRenderbuffer(GL_RENDERBUFFER, 0);
}

void TextureTarget::bindImage(size_t /*unit*/) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void TextureTarget::attachAsColor(uint32_t index, const AttachmentParams& params) {
  IGL_PROFILER_FUNCTION();
  if (IGL_DEBUG_VERIFY(renderBufferID_)) {
    attach(GL_COLOR_ATTACHMENT0 + index, params, prepareRenderBuffer(params));
  }
}

void TextureTarget::attach(GLenum attachment,
                           const AttachmentParams& params,
                           GLuint renderBufferId) {
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(!params.stereo);
  IGL_DEBUG_ASSERT(params.face == 0);
  IGL_DEBUG_ASSERT(params.layer == 0);
  IGL_DEBUG_ASSERT(params.mipLevel == 0);

  GLenum framebufferTarget = GL_FRAMEBUFFER;
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    framebufferTarget = params.read ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER;
  }
  if (renderBufferId) {
    getContext().framebufferRenderbuffer(
        framebufferTarget, attachment, GL_RENDERBUFFER, renderBufferId);
  } else {
    // Binding to render buffer ID 0 is undefined in iOS so unbind as texture
    getContext().framebufferTexture2D(framebufferTarget, attachment, GL_TEXTURE_2D, 0, 0);
  }
}

void TextureTarget::detachAsColor(uint32_t index, bool read) {
  IGL_PROFILER_FUNCTION();
  AttachmentParams params{};
  params.read = read;
  attach(GL_COLOR_ATTACHMENT0 + index, params, 0);
  releaseImplicitRenderBuffer();
}

void TextureTarget::attachAsDepth(const AttachmentParams& params) {
  IGL_PROFILER_FUNCTION();
  if (IGL_DEBUG_VERIFY(renderBufferID_)) {
    attach(GL_DEPTH_ATTACHMENT, params, prepareRenderBuffer(params));
  }
}

void TextureTarget::detachAsDepth(bool read) {
  IGL_PROFILER_FUNCTION();
  AttachmentParams params{};
  params.read = read;
  attach(GL_DEPTH_ATTACHMENT, params, 0);
  releaseImplicitRenderBuffer();
}

void TextureTarget::attachAsStencil(const AttachmentParams& params) {
  IGL_PROFILER_FUNCTION();
  if (IGL_DEBUG_VERIFY(renderBufferID_)) {
    attach(GL_STENCIL_ATTACHMENT, params, prepareRenderBuffer(params));
  }
}

void TextureTarget::detachAsStencil(bool read) {
  IGL_PROFILER_FUNCTION();
  AttachmentParams params{};
  params.read = read;
  attach(GL_STENCIL_ATTACHMENT, params, 0);
  releaseImplicitRenderBuffer();
}

bool TextureTarget::toRenderBufferFormatGL(TextureDesc::TextureUsage usage,
                                           GLenum& formatGL) const {
  FormatDescGL formatDescGL;
  if (!toFormatDescGL(getFormat(), usage, formatDescGL)) {
    return false;
  }
  formatGL = formatDescGL.internalFormat;
  return true;
}

} // namespace igl::opengl
