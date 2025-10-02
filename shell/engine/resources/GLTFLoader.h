/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Mesh.h"
#include <igl/IGL.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace igl {
class IDevice;
}

namespace engine {

class Material;

// Scene graph node representing glTF node with transform and mesh
struct GLTFNode {
  std::string name;
  glm::mat4 transform;  // Local transform (TRS matrix)
  std::shared_ptr<Mesh> mesh;  // Optional mesh attached to this node
  std::vector<std::shared_ptr<GLTFNode>> children;

  GLTFNode() : transform(1.0f) {}  // Identity transform by default

  // Get world transform by multiplying parent transforms
  glm::mat4 getWorldTransform(const glm::mat4& parentTransform = glm::mat4(1.0f)) const {
    return parentTransform * transform;
  }
};

struct GLTFModel {
  std::vector<std::shared_ptr<Mesh>> meshes;
  std::vector<std::shared_ptr<igl::ITexture>> textures;
  std::vector<std::shared_ptr<Material>> materials;
  std::vector<std::shared_ptr<GLTFNode>> nodes;  // All nodes (flat list)
  std::vector<std::shared_ptr<GLTFNode>> rootNodes;  // Top-level nodes in the scene
};

class GLTFLoader {
 public:
  static std::unique_ptr<GLTFModel> loadFromFile(igl::IDevice& device, const std::string& path);

 private:
  static std::shared_ptr<Mesh> createMeshFromGLTF(igl::IDevice& device, void* gltfMeshPtr,
                                                  void* gltfDataPtr);
  static std::shared_ptr<igl::ITexture> loadTexture(igl::IDevice& device, void* gltfTexturePtr,
                                                     void* gltfDataPtr, const std::string& basePath);
};

} // namespace engine
