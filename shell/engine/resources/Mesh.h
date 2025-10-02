/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Texture.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace engine {

class Material;  // Forward declaration

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texCoord;
  glm::vec3 tangent;

  Vertex() = default;
  Vertex(const glm::vec3& pos, const glm::vec3& norm = glm::vec3(0.0f),
         const glm::vec2& uv = glm::vec2(0.0f), const glm::vec3& tan = glm::vec3(0.0f))
      : position(pos), normal(norm), texCoord(uv), tangent(tan) {}
};

struct SubMesh {
  uint32_t indexOffset = 0;
  uint32_t indexCount = 0;
  uint32_t materialIndex = 0;
};

class Mesh {
 public:
  Mesh() = default;

  // Vertex data
  void setVertices(const std::vector<Vertex>& vertices) {
    vertices_ = vertices;
  }

  const std::vector<Vertex>& getVertices() const {
    return vertices_;
  }

  // Index data
  void setIndices(const std::vector<uint32_t>& indices) {
    indices_ = indices;
  }

  const std::vector<uint32_t>& getIndices() const {
    return indices_;
  }

  // SubMeshes (for multi-material support)
  void addSubMesh(const SubMesh& subMesh) {
    subMeshes_.push_back(subMesh);
  }

  const std::vector<SubMesh>& getSubMeshes() const {
    return subMeshes_;
  }

  // IGL Buffers
  void setVertexBuffer(std::shared_ptr<igl::IBuffer> buffer) {
    vertexBuffer_ = std::move(buffer);
  }

  std::shared_ptr<igl::IBuffer> getVertexBuffer() const {
    return vertexBuffer_;
  }

  void setIndexBuffer(std::shared_ptr<igl::IBuffer> buffer) {
    indexBuffer_ = std::move(buffer);
  }

  std::shared_ptr<igl::IBuffer> getIndexBuffer() const {
    return indexBuffer_;
  }

  // Bounding box (for culling)
  void calculateBounds() {
    if (vertices_.empty()) {
      boundsMin_ = glm::vec3(0.0f);
      boundsMax_ = glm::vec3(0.0f);
      return;
    }

    boundsMin_ = vertices_[0].position;
    boundsMax_ = vertices_[0].position;

    for (const auto& vertex : vertices_) {
      boundsMin_ = glm::min(boundsMin_, vertex.position);
      boundsMax_ = glm::max(boundsMax_, vertex.position);
    }
  }

  const glm::vec3& getBoundsMin() const {
    return boundsMin_;
  }

  const glm::vec3& getBoundsMax() const {
    return boundsMax_;
  }

  // Material
  void setMaterial(std::shared_ptr<Material> material) {
    material_ = material;
  }

  std::shared_ptr<Material> getMaterial() const {
    return material_;
  }

 private:
  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;
  std::vector<SubMesh> subMeshes_;
  std::shared_ptr<igl::IBuffer> vertexBuffer_;
  std::shared_ptr<igl::IBuffer> indexBuffer_;
  glm::vec3 boundsMin_{0.0f};
  glm::vec3 boundsMax_{0.0f};
  std::shared_ptr<Material> material_;
};

} // namespace engine
