/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/DepthStencilState.h>

namespace igl::opengl {

DepthStencilState::DepthStencilState(IContext& context) : WithContext(context) {}

Result DepthStencilState::create(const DepthStencilStateDesc& desc) {
  desc_ = desc;

  return Result();
}

GLenum DepthStencilState::convertCompareFunction(CompareFunction value) {
  switch (value) {
  case CompareFunction::Never:
    return GL_NEVER;
  case CompareFunction::Less:
    return GL_LESS;
  case CompareFunction::Equal:
    return GL_EQUAL;
  case CompareFunction::LessEqual:
    return GL_LEQUAL;
  case CompareFunction::Greater:
    return GL_GREATER;
  case CompareFunction::NotEqual:
    return GL_NOTEQUAL;
  case CompareFunction::GreaterEqual:
    return GL_GEQUAL;
  case CompareFunction::AlwaysPass:
    return GL_ALWAYS;
  }
  IGL_UNREACHABLE_RETURN(GL_ALWAYS)
}

GLenum DepthStencilState::convertStencilOperation(StencilOperation value) {
  switch (value) {
  case StencilOperation::Keep:
    return GL_KEEP;
  case StencilOperation::Zero:
    return GL_ZERO;
  case StencilOperation::Replace:
    return GL_REPLACE;
  case StencilOperation::IncrementClamp:
    return GL_INCR;
  case StencilOperation::DecrementClamp:
    return GL_DECR;
  case StencilOperation::Invert:
    return GL_INVERT;
  case StencilOperation::IncrementWrap:
    return GL_INCR_WRAP;
  case StencilOperation::DecrementWrap:
    return GL_DECR_WRAP;
  }
  IGL_UNREACHABLE_RETURN(GL_ZERO)
}

void DepthStencilState::bind(uint32_t frontStencilReferenceValue,
                             uint32_t backStencilReferenceValue) {
  getContext().depthMask(static_cast<GLboolean>(desc_.isDepthWriteEnabled));

  // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDepthFunc.xhtml
  // In order to unconditionally write to the depth buffer, the depth test should
  // be enabled and set to GL_ALWAYS
  if (desc_.isDepthWriteEnabled || desc_.compareFunction != CompareFunction::AlwaysPass) {
    getContext().enable(GL_DEPTH_TEST);
  } else {
    getContext().disable(GL_DEPTH_TEST);
  }
  getContext().depthFunc(convertCompareFunction(desc_.compareFunction));

  if (desc_.frontFaceStencil != StencilStateDesc() || desc_.backFaceStencil != StencilStateDesc()) {
    getContext().enable(GL_STENCIL_TEST);

    const GLenum frontCompareFunc =
        convertCompareFunction(desc_.frontFaceStencil.stencilCompareFunction);
    const GLenum backCompareFunc =
        convertCompareFunction(desc_.backFaceStencil.stencilCompareFunction);
    getContext().stencilFuncSeparate(
        GL_FRONT, frontCompareFunc, frontStencilReferenceValue, desc_.frontFaceStencil.readMask);
    getContext().stencilFuncSeparate(
        GL_BACK, backCompareFunc, backStencilReferenceValue, desc_.backFaceStencil.readMask);

    GLenum sfail = convertStencilOperation(desc_.backFaceStencil.stencilFailureOperation);
    GLenum dpfail = convertStencilOperation(desc_.backFaceStencil.depthFailureOperation);
    GLenum dppass = convertStencilOperation(desc_.backFaceStencil.depthStencilPassOperation);
    getContext().stencilOpSeparate(GL_BACK, sfail, dpfail, dppass);

    sfail = convertStencilOperation(desc_.frontFaceStencil.stencilFailureOperation);
    dpfail = convertStencilOperation(desc_.frontFaceStencil.depthFailureOperation);
    dppass = convertStencilOperation(desc_.frontFaceStencil.depthStencilPassOperation);
    getContext().stencilOpSeparate(GL_FRONT, sfail, dpfail, dppass);

    getContext().stencilMaskSeparate(GL_BACK, desc_.backFaceStencil.writeMask);
    getContext().stencilMaskSeparate(GL_FRONT, desc_.frontFaceStencil.writeMask);
  } else {
    getContext().disable(GL_STENCIL_TEST);
  }
}

void DepthStencilState::unbind() {}

} // namespace igl::opengl
