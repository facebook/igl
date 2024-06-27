/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/NameHandle.h>
#include <igl/RenderPipelineReflection.h>
#include <igl/Shader.h>
#include <igl/Texture.h>
#include <map>
#include <unordered_map>

namespace igl {
/**
 * @brief Object for customizing the compilation of a new compute pipeline state object
 * To create a IComputePipelineState object, you create a pipeline descriptor, configure
 * its properties, pass it to one of the creation methods on a IDevice object.
 *
 * The most important property to set is the computeFunction property,
 * which specifies which function to call.
 */
struct ComputePipelineDesc {
 public:
  bool operator==(const ComputePipelineDesc& other) const {
    return shaderStages == other.shaderStages && imagesMap == other.imagesMap &&
           buffersMap == other.buffersMap && debugName == other.debugName;
  }

  /*
   * OpenGL only
   */
  std::unordered_map<size_t, igl::NameHandle> imagesMap;
  std::unordered_map<size_t, igl::NameHandle> buffersMap;

  /*
   * @brief The compute kernel the pipeline calls.
   */
  std::shared_ptr<IShaderStages> shaderStages;

  std::string debugName;
};

/**
 * @brief Object that contains a compiled compute pipeline.
 * These objects can be created by calling methods on an IDevice object.
 */
class IComputePipelineState {
 public:
  using IComputePipelineReflection = IRenderPipelineReflection;
  virtual std::shared_ptr<IComputePipelineReflection> computePipelineReflection() = 0;
  virtual ~IComputePipelineState() = default;
  [[nodiscard]] virtual int getIndexByName(const NameHandle& /* name */) const {
    return -1;
  }
};

} // namespace igl

/// Hashing function declarations
///
namespace std {

// ComputePipelineDesc hash
template<>
struct hash<igl::ComputePipelineDesc> {
  size_t operator()(const igl::ComputePipelineDesc& desc) const {
    size_t hash = std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(desc.shaderStages.get()));
    hash ^= std::hash<std::string>()(desc.debugName);
    for (const auto& p : desc.buffersMap) {
      hash ^= std::hash<size_t>()(p.first);
      hash ^= std::hash<std::string>()(p.second.toString());
    }
    for (const auto& p : desc.imagesMap) {
      hash ^= std::hash<size_t>()(p.first);
      hash ^= std::hash<std::string>()(p.second.toString());
    }
    return hash;
  }
};

} // namespace std
