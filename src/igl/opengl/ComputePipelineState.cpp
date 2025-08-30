/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/NameHandle.h>
#include <igl/opengl/Buffer.h>
#include <igl/opengl/ComputePipelineState.h>
#include <igl/opengl/Texture.h>

namespace igl::opengl {
ComputePipelineState::ComputePipelineState(IContext& context) : WithContext(context) {}
ComputePipelineState::~ComputePipelineState() = default;

Result ComputePipelineState::create(const ComputePipelineDesc& desc) {
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
    if (IGL_DEBUG_VERIFY(loc >= 0)) {
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

  for (const auto& buffer : desc.buffersMap) {
    const auto bufferUnit = buffer.first;
    const auto& bufferName = buffer.second;

    IGL_DEBUG_ASSERT(!bufferName.toString().empty());
    const int loc = reflection_->getIndexByName(bufferName);
    if (IGL_DEBUG_VERIFY(loc >= 0)) {
      if (const auto& ssboDictionary = reflection_->getShaderStorageBufferObjectDictionary();
          ssboDictionary.find(bufferName) != ssboDictionary.end()) {
        const GLint index = getContext().getProgramResourceIndex(
            shaderStages_->getProgramID(), GL_SHADER_STORAGE_BLOCK, bufferName.c_str());
        if (IGL_DEBUG_VERIFY(index != GL_INVALID_INDEX)) {
          bufferUnitMap_[bufferUnit] = loc;
          usingShaderStorageBuffers_ = true;
        } else {
          IGL_LOG_ERROR("Shader storage buffer (%s) not found in shader.\n", bufferName.c_str());
        }
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
  if (shaderStages_) {
    shaderStages_->bind();
  }
}

std::shared_ptr<IComputePipelineState::IComputePipelineReflection>
ComputePipelineState::computePipelineReflection() {
  return reflection_;
}

void ComputePipelineState::unbind() {
  if (shaderStages_) {
    shaderStages_->unbind();
  }
}

Result ComputePipelineState::bindTextureUnit(const size_t unit, Texture* texture) {
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

Result ComputePipelineState::bindBuffer(const size_t unit, Buffer* buffer) {
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
  static_cast<ArrayBuffer&>(*buffer).bindBase(unit, &result);

  return result;
}

int ComputePipelineState::getIndexByName(const NameHandle& name) const {
  return reflection_ ? reflection_->getIndexByName(name) : -1;
}
} // namespace igl::opengl
