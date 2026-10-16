/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/ComputePipelineState.h>

#include <igl/Macros.h>
#include <igl/NameHandle.h>
#include <igl/opengl/Buffer.h>
#include <igl/opengl/Texture.h>

namespace igl::opengl {
ComputePipelineState::ComputePipelineState(IContext& context) : WithContext(context) {
  samplerUnitMap_.fill(-1);
}
ComputePipelineState::~ComputePipelineState() = default;

Result ComputePipelineState::create(const ComputePipelineDesc& desc) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  Result result;
  if (IGL_DEBUG_VERIFY_NOT(desc.shaderStages == nullptr)) {
    Result::setResult(&result, Result::Code::ArgumentInvalid, "Missing shader stages");
    return result;
  }
  if (!IGL_DEBUG_VERIFY(desc.shaderStages->getType() == ShaderStagesType::Compute)) {
    Result::setResult(&result, Result::Code::ArgumentInvalid, "Shader stages not for compute");
    return result;
  }
  if (!IGL_DEBUG_VERIFY(desc.shaderStages->getComputeModule())) {
    Result::setResult(&result, Result::Code::ArgumentInvalid, "Missing compute shader");
    return result;
  }
  shaderStages_ = std::static_pointer_cast<ShaderStages>(desc.shaderStages);
  reflection_ = std::make_shared<ComputePipelineReflection>(getContext(), *shaderStages_);

  for (const auto& unitSampler : desc.imagesMap) {
    const auto textureUnit = unitSampler.first;
    const auto& imageName = unitSampler.second;

    IGL_DEBUG_ASSERT(!imageName.toString().empty());
    const int loc = reflection_->getIndexByName(imageName);
    if (loc >= 0) {
      GLint unit = 0;
      getContext().getUniformiv(shaderStages_->getProgramID(), loc, &unit);
      if (IGL_DEBUG_VERIFY(unit >= 0)) {
        imageUnitMap_[textureUnit] = unit;
      } else {
        IGL_LOG_ERROR("Image uniform unit (%s) not found in shader.\n", imageName.c_str());
      }
    } else {
      IGL_LOG_ERROR("Image uniform (%s) not found in shader.\n", imageName.c_str());
    }
  }

  for (const auto& unitSampler : desc.samplersMap) {
    const auto textureUnit = unitSampler.first;
    const auto& samplerName = unitSampler.second;

    IGL_DEBUG_ASSERT(!samplerName.toString().empty());
    const int loc = reflection_->getIndexByName(samplerName);
    if (loc >= 0) {
      GLint unit = 0;
      getContext().getUniformiv(shaderStages_->getProgramID(), loc, &unit);
      if (IGL_DEBUG_VERIFY(unit >= 0)) {
        samplerUnitMap_[textureUnit] = unit;
      } else {
        IGL_LOG_ERROR("Sampler uniform unit (%s) not found in shader.\n", samplerName.c_str());
      }
    } else {
      IGL_LOG_ERROR("Sampler uniform (%s) not found in shader.\n", samplerName.c_str());
    }
  }

  for (const auto& buffer : desc.buffersMap) {
    const auto bufferUnit = buffer.first;
    const auto& bufferName = buffer.second;

    IGL_DEBUG_ASSERT(!bufferName.toString().empty());
    const int loc = reflection_->getIndexByName(bufferName);
    if (loc >= 0) {
      if (const auto& ssboDictionary = reflection_->getShaderStorageBufferObjectDictionary();
          ssboDictionary.find(bufferName) != ssboDictionary.end()) {
        bufferUnitMap_[bufferUnit] = loc;
        usingShaderStorageBuffers_ = true;
      } else {
        GLint unit = 0;
        getContext().getUniformiv(shaderStages_->getProgramID(), loc, &unit);
        if (IGL_DEBUG_VERIFY(unit >= 0)) {
          bufferUnitMap_[bufferUnit] = loc;
        } else {
          IGL_LOG_ERROR("Buffer uniform unit (%s) not found in shader.\n", bufferName.c_str());
        }
      }
    } else {
      IGL_LOG_ERROR("Buffer uniform (%s) not found in shader.\n", bufferName.c_str());
    }
  }

  return Result();
}

void ComputePipelineState::bind() {
  IGL_PROFILER_FUNCTION();
  if (shaderStages_) {
    shaderStages_->bind();
  }
}

std::shared_ptr<IComputePipelineState::IComputePipelineReflection>
ComputePipelineState::computePipelineReflection() {
  return reflection_;
}

void ComputePipelineState::unbind() {
  IGL_PROFILER_FUNCTION();
  if (shaderStages_) {
    shaderStages_->unbind();
  }
}

Result ComputePipelineState::bindImageTextureUnit(const size_t unit, Texture* texture) {
  IGL_PROFILER_FUNCTION();
  if (!shaderStages_) {
    return Result{Result::Code::InvalidOperation, "No shader set\n"};
  }

  if (unit >= IGL_TEXTURE_SAMPLERS_MAX) {
    return Result{Result::Code::ArgumentInvalid, "Image unit specified greater than maximum\n"};
  }

  const GLint samplerUnit(imageUnitMap_[unit]);

  if (samplerUnit < 0) {
    return Result{Result::Code::RuntimeError, "Unable to find image location\n"};
  }

  texture->bindImage(samplerUnit);

  return Result();
}

Result ComputePipelineState::bindSamplerTextureUnit(const size_t unit, Texture* texture) {
  IGL_PROFILER_FUNCTION();
  if (!shaderStages_) {
    return Result{Result::Code::InvalidOperation, "No shader set\n"};
  }

  if (unit >= IGL_TEXTURE_SAMPLERS_MAX) {
    return Result{Result::Code::ArgumentInvalid, "Sampler unit specified greater than maximum\n"};
  }

  if (IGL_DEBUG_VERIFY_NOT(texture == nullptr)) {
    return Result{Result::Code::ArgumentInvalid, "Texture is nullptr\n"};
  }

  getContext().activeTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
  texture->bind();

  // A sampler uniform defaults to 0 in GLES, so without this the shader reads GL_TEXTURE0 no
  // matter which unit the texture was bound to.
  const GLint samplerLocation(samplerUnitMap_[unit]);
  if (samplerLocation >= 0) {
    getContext().uniform1i(samplerLocation, static_cast<GLint>(unit));
  } else {
    IGL_LOG_INFO_ONCE(
        "No sampler uniform mapped for compute texture unit %zu; shader will read GL_TEXTURE0\n",
        unit);
  }

  return Result();
}

Result ComputePipelineState::bindBuffer(const size_t unit, Buffer* buffer) {
  IGL_PROFILER_FUNCTION();
  if (!shaderStages_) {
    return Result{Result::Code::InvalidOperation, "No shader set\n"};
  }

  if (unit >= IGL_BUFFER_BINDINGS_MAX) {
    return Result{Result::Code::ArgumentInvalid, "Buffer unit specified greater than maximum\n"};
  }

  const GLint bufferLocation(bufferUnitMap_[unit]);

  if (bufferLocation < 0) {
    return Result{Result::Code::RuntimeError, "Unable to find buffer location\n"};
  }

  Result result;
  static_cast<ArrayBuffer&>(*buffer).bindBase(bufferLocation, &result);

  return result;
}

int ComputePipelineState::getIndexByName(const NameHandle& name) const {
  return reflection_ ? reflection_->getIndexByName(name) : -1;
}
} // namespace igl::opengl
