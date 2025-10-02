/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Texture.h>
#include <igl/RenderPipelineState.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace engine {

class Material {
 public:
  Material() = default;

  // PBR properties
  void setBaseColor(const glm::vec4& color) {
    baseColor_ = color;
  }

  const glm::vec4& getBaseColor() const {
    return baseColor_;
  }

  void setMetallic(float metallic) {
    metallic_ = metallic;
  }

  float getMetallic() const {
    return metallic_;
  }

  void setRoughness(float roughness) {
    roughness_ = roughness;
  }

  float getRoughness() const {
    return roughness_;
  }

  // Textures
  void setTexture(const std::string& name, std::shared_ptr<igl::ITexture> texture) {
    textures_[name] = std::move(texture);
  }

  std::shared_ptr<igl::ITexture> getTexture(const std::string& name) const {
    auto it = textures_.find(name);
    return it != textures_.end() ? it->second : nullptr;
  }

  const std::unordered_map<std::string, std::shared_ptr<igl::ITexture>>& getTextures() const {
    return textures_;
  }

  // Pipeline state
  void setPipelineState(std::shared_ptr<igl::IRenderPipelineState> pipelineState) {
    pipelineState_ = std::move(pipelineState);
  }

  std::shared_ptr<igl::IRenderPipelineState> getPipelineState() const {
    return pipelineState_;
  }

 private:
  glm::vec4 baseColor_{1.0f, 1.0f, 1.0f, 1.0f};
  float metallic_ = 0.0f;
  float roughness_ = 0.5f;
  std::unordered_map<std::string, std::shared_ptr<igl::ITexture>> textures_;
  std::shared_ptr<igl::IRenderPipelineState> pipelineState_;
};

} // namespace engine
