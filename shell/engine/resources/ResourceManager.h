/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace igl {
class ITexture;
class IDevice;
namespace shell {
class Platform;
}
} // namespace igl

namespace engine {

class Mesh;
class Material;

class ResourceManager {
 public:
  explicit ResourceManager(std::shared_ptr<igl::shell::Platform> platform);

  std::shared_ptr<igl::ITexture> loadTexture(const std::string& path);
  std::shared_ptr<Mesh> loadMesh(const std::string& path);
  std::shared_ptr<Material> createMaterial();

 private:
  std::shared_ptr<igl::shell::Platform> platform_;
  std::unordered_map<std::string, std::shared_ptr<igl::ITexture>> textureCache_;
  std::unordered_map<std::string, std::shared_ptr<Mesh>> meshCache_;
};

} // namespace engine
