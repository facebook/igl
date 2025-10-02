/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "ResourceManager.h"
#include "Mesh.h"
#include "../graphics/Material.h"
#include <shell/shared/platform/Platform.h>

namespace engine {

ResourceManager::ResourceManager(std::shared_ptr<igl::shell::Platform> platform)
    : platform_(std::move(platform)) {}

std::shared_ptr<igl::ITexture> ResourceManager::loadTexture(const std::string& path) {
  // Check cache
  auto it = textureCache_.find(path);
  if (it != textureCache_.end()) {
    return it->second;
  }

  // Load texture using platform
  auto texture = platform_->loadTexture(path.c_str());
  if (texture) {
    textureCache_[path] = texture;
  }
  return texture;
}

std::shared_ptr<Mesh> ResourceManager::loadMesh(const std::string& path) {
  // Check cache
  auto it = meshCache_.find(path);
  if (it != meshCache_.end()) {
    return it->second;
  }

  // TODO: Implement mesh loading via GLTFLoader
  auto mesh = std::make_shared<Mesh>();
  meshCache_[path] = mesh;
  return mesh;
}

std::shared_ptr<Material> ResourceManager::createMaterial() {
  return std::make_shared<Material>();
}

} // namespace engine
