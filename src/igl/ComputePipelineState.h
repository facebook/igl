/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/Shader.h>
#include <igl/Texture.h>
#include <map>
#include <unordered_map>

namespace igl {
/*
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
    return shaderStages == other.shaderStages && debugName == other.debugName;
  }

  /*
   * @brief The compute kernel the pipeline calls.
   */
  std::shared_ptr<ShaderStages> shaderStages;

  std::string debugName;
};

/*
 * @brief Object that contains a compiled compute pipeline.
 * These objects can be created by calling methods on an IDevice object.
 */
class IComputePipelineState {
 public:
  virtual ~IComputePipelineState() = default;
};

} // namespace igl
