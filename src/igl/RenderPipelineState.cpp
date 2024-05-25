/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/RenderPipelineState.h>

#include <string>
#include <unordered_map>

using namespace igl;

///
/// RenderPipelineDesc::FramebufferDesc::ColorAttachment
///
bool RenderPipelineDesc::TargetDesc::ColorAttachment::operator!=(
    const ColorAttachment& other) const {
  return !(*this == other);
}

bool RenderPipelineDesc::TargetDesc::ColorAttachment::operator==(
    const ColorAttachment& other) const {
  return (textureFormat == other.textureFormat && colorWriteMask == other.colorWriteMask &&
          blendEnabled == other.blendEnabled && rgbBlendOp == other.rgbBlendOp &&
          alphaBlendOp == other.alphaBlendOp && srcRGBBlendFactor == other.srcRGBBlendFactor &&
          srcAlphaBlendFactor == other.srcAlphaBlendFactor &&
          dstRGBBlendFactor == other.dstRGBBlendFactor &&
          dstAlphaBlendFactor == other.dstAlphaBlendFactor);
}

///
/// RenderPipelineDesc::FramebufferDesc
///
bool RenderPipelineDesc::TargetDesc::operator!=(const TargetDesc& other) const {
  return !(*this == other);
}

bool RenderPipelineDesc::TargetDesc::operator==(const TargetDesc& other) const {
  return (colorAttachments == other.colorAttachments &&
          depthAttachmentFormat == other.depthAttachmentFormat &&
          stencilAttachmentFormat == other.stencilAttachmentFormat);
}

///
/// RenderPipelineDesc
///
bool RenderPipelineDesc::operator!=(const RenderPipelineDesc& other) const {
  return !(*this == other);
}

bool RenderPipelineDesc::operator==(const RenderPipelineDesc& other) const {
  if (topology != other.topology) {
    return false;
  }

  if (vertexInputState != other.vertexInputState) {
    return false;
  }

  if (shaderStages != other.shaderStages) {
    return false;
  }

  if (targetDesc != other.targetDesc) {
    return false;
  }

  if (cullMode != other.cullMode || frontFaceWinding != other.frontFaceWinding ||
      polygonFillMode != other.polygonFillMode) {
    return false;
  }

  if (vertexUnitSamplerMap != other.vertexUnitSamplerMap) {
    return false;
  }
  if (fragmentUnitSamplerMap != other.fragmentUnitSamplerMap) {
    return false;
  }
  if (uniformBlockBindingMap != other.uniformBlockBindingMap) {
    return false;
  }

  if (sampleCount != other.sampleCount) {
    return false;
  }

  if (debugName != other.debugName) {
    return false;
  }

  return true;
}

/// The underlying assumption for this hash is all of the shared pointers in
/// this structure can uniquely identify the object they are pointing to.
/// It is the responsibility of the caller of this function to make sure
/// that is the case.
size_t std::hash<RenderPipelineDesc>::operator()(RenderPipelineDesc const& key) const {
  size_t hash = std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(key.vertexInputState.get()));

  hash ^= std::hash<int>()(EnumToValue(key.topology));
  hash ^= std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(key.shaderStages.get()));
  hash ^= std::hash<RenderPipelineDesc::TargetDesc>()(key.targetDesc);
  hash ^= std::hash<int>()(EnumToValue(key.cullMode));
  hash ^= std::hash<int>()(key.sampleCount);
  hash ^= std::hash<int>()(EnumToValue(key.frontFaceWinding));
  hash ^= std::hash<int>()(EnumToValue(key.polygonFillMode));
  hash ^= std::hash<igl::NameHandle>()(key.debugName);

  for (const auto& i : key.vertexUnitSamplerMap) {
    hash ^= std::hash<size_t>()(i.first);
    hash ^= std::hash<igl::NameHandle>()(i.second);
  }
  for (const auto& i : key.fragmentUnitSamplerMap) {
    hash ^= std::hash<size_t>()(i.first);
    hash ^= std::hash<igl::NameHandle>()(i.second);
  }
  for (const auto& i : key.uniformBlockBindingMap) {
    hash ^= std::hash<size_t>()(i.first);
    hash ^= std::hash<igl::NameHandle>()(i.second.first);
    hash ^= std::hash<igl::NameHandle>()(i.second.second);
  }

  return hash;
}

size_t std::hash<RenderPipelineDesc::TargetDesc>::operator()(
    RenderPipelineDesc::TargetDesc const& key) const {
  size_t hash = std::hash<int>()(EnumToValue(key.depthAttachmentFormat));

  hash ^= std::hash<int>()(EnumToValue(key.stencilAttachmentFormat));
  hash ^= std::hash<size_t>()(key.colorAttachments.size());

  for (const auto& ca : key.colorAttachments) {
    hash ^= std::hash<RenderPipelineDesc::TargetDesc::ColorAttachment>()(ca);
  }

  return hash;
}

size_t std::hash<RenderPipelineDesc::TargetDesc::ColorAttachment>::operator()(
    RenderPipelineDesc::TargetDesc::ColorAttachment const& key) const {
  size_t hash = std::hash<int>()(EnumToValue(key.textureFormat));
  hash ^= std::hash<uint8_t>()(key.colorWriteMask);
  hash ^= std::hash<bool>()(key.blendEnabled);
  hash ^= std::hash<int>()(EnumToValue(key.rgbBlendOp));
  hash ^= std::hash<int>()(EnumToValue(key.alphaBlendOp));
  hash ^= std::hash<int>()(EnumToValue(key.srcRGBBlendFactor));
  hash ^= std::hash<int>()(EnumToValue(key.srcAlphaBlendFactor));
  hash ^= std::hash<int>()(EnumToValue(key.dstRGBBlendFactor));
  hash ^= std::hash<int>()(EnumToValue(key.dstAlphaBlendFactor));
  return hash;
}
