/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Device.h>
#include <igl/Shader.h>

#include <algorithm>
#include <utility>

namespace igl {

// Make sure the structure igl::Color is tightly packed so it can be passed into APIs which expect
// float[4] RGBA values
static_assert(sizeof(Color) == 4 * sizeof(float));

TextureDesc IDevice::sanitize(const TextureDesc& desc) const {
  TextureDesc sanitized = desc;
  if (desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.numLayers == 0 ||
      desc.numSamples == 0 || desc.numMipLevels == 0) {
    sanitized.width = std::max(sanitized.width, 1u);
    sanitized.height = std::max(sanitized.height, 1u);
    sanitized.depth = std::max(sanitized.depth, 1u);
    sanitized.numLayers = std::max(sanitized.numLayers, 1u);
    sanitized.numSamples = std::max(sanitized.numSamples, 1u);
    sanitized.numMipLevels = std::max(sanitized.numMipLevels, 1u);
    LLOGW(
        "width (%d), height (%d), depth (%d), numLayers (%d), numSamples (%d) and numMipLevels "
        "(%d) should be at least 1.\n",
        desc.width,
        desc.height,
        desc.depth,
        desc.numLayers,
        desc.numSamples,
        desc.numMipLevels);
  }

  return sanitized;
}

std::unique_ptr<ShaderStages> IDevice::createShaderStages(const char* vs,
                                                           std::string debugNameVS,
                                                           const char* fs,
                                                           std::string debugNameFS,
                                                           Result* outResult) const {
  auto VS = createShaderModule(igl::ShaderModuleDesc(vs, Stage_Vertex, std::move(debugNameVS)),
                               outResult);
  auto FS = createShaderModule(igl::ShaderModuleDesc(fs, Stage_Fragment, std::move(debugNameFS)),
                               outResult);
  return std::make_unique<igl::ShaderStages>(VS, FS);
}

std::unique_ptr<ShaderStages> IDevice::createShaderStages(const char* cs,
                                                          std::string debugName,
                                                          Result* outResult) const {
  return std::make_unique<igl::ShaderStages>(
      createShaderModule(igl::ShaderModuleDesc(cs, Stage_Compute, debugName), outResult));
}


} // namespace igl
