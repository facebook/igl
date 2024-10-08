/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/TextureTarget.h>

#include <igl/opengl/Errors.h>

namespace igl::opengl {

TextureTarget::~TextureTarget() {
  if (renderBufferID_ != 0) {
    getContext().deleteRenderbuffers(1, &renderBufferID_);
  }
}

TextureType TextureTarget::getType() const {
  return TextureType::TwoD;
}

TextureDesc::TextureUsage TextureTarget::getUsage() const {
  return TextureDesc::TextureUsageBits::Attachment;
}

// create a 2D texture given the specified dimensions and format
Result TextureTarget::create(const TextureDesc& desc, bool hasStorageAlready) {
  Result result = Super::create(desc, hasStorageAlready);
  if (result.isOk()) {
    if (desc.usage & TextureDesc::TextureUsageBits::Attachment) {
      result = createRenderBuffer(desc, hasStorageAlready);
    } else {
      result = Result(Result::Code::Unsupported, "invalid usage!");
    }
  }
  return result;
}

// create a render buffer for render target usages
Result TextureTarget::createRenderBuffer(const TextureDesc& desc, bool hasStorageAlready) {
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

  // create the GL render buffer
  getContext().genRenderbuffers(1, &renderBufferID_);

  if (!hasStorageAlready) {
    getContext().bindRenderbuffer(GL_RENDERBUFFER, renderBufferID_);

    if (desc.numSamples > 1) {
      getContext().renderbufferStorageMultisample(
          GL_RENDERBUFFER, desc.numSamples, glInternalFormat_, getWidth(), getHeight());
    } else {
      getContext().renderbufferStorage(
          GL_RENDERBUFFER, glInternalFormat_, (GLsizei)getWidth(), (GLsizei)getHeight());
    }
    if (!desc.debugName.empty() &&
        getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugLabel)) {
      getContext().objectLabel(
          GL_RENDERBUFFER, renderBufferID_, desc.debugName.size(), desc.debugName.c_str());
    }

    getContext().bindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  return Result();
}

void TextureTarget::bind() {
  getContext().bindRenderbuffer(GL_RENDERBUFFER, renderBufferID_);
}

void TextureTarget::unbind() {
  getContext().bindRenderbuffer(GL_RENDERBUFFER, 0);
}

void TextureTarget::bindImage(size_t /*unit*/) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void TextureTarget::attachAsColor(uint32_t index, const AttachmentParams& params) {
  if (IGL_DEBUG_VERIFY(renderBufferID_)) {
    attach(GL_COLOR_ATTACHMENT0 + index, params, renderBufferID_);
  }
}

void TextureTarget::attach(GLenum attachment,
                           const AttachmentParams& params,
                           GLuint renderBufferId) {
  IGL_DEBUG_ASSERT(params.stereo == false);
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
  AttachmentParams params{};
  params.read = read;
  attach(GL_COLOR_ATTACHMENT0 + index, params, 0);
}

void TextureTarget::attachAsDepth(const AttachmentParams& params) {
  if (IGL_DEBUG_VERIFY(renderBufferID_)) {
    attach(GL_DEPTH_ATTACHMENT, params, renderBufferID_);
  }
}

void TextureTarget::detachAsDepth(bool read) {
  AttachmentParams params{};
  params.read = read;
  attach(GL_DEPTH_ATTACHMENT, params, 0);
}

void TextureTarget::attachAsStencil(const AttachmentParams& params) {
  if (IGL_DEBUG_VERIFY(renderBufferID_)) {
    attach(GL_STENCIL_ATTACHMENT, params, renderBufferID_);
  }
}

void TextureTarget::detachAsStencil(bool read) {
  AttachmentParams params{};
  params.read = read;
  attach(GL_STENCIL_ATTACHMENT, params, 0);
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
