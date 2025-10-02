/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "GLTFLoader.h"
#include "../graphics/Material.h"
#include <igl/IGL.h>
#include <stb/stb_image.h>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define CGLTF_IMPLEMENTATION
#include <meshoptimizer/extern/cgltf.h>

namespace engine {

std::unique_ptr<GLTFModel> GLTFLoader::loadFromFile(igl::IDevice& device,
                                                     const std::string& path) {
  cgltf_options options = {};
  cgltf_data* data = nullptr;

  cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
  if (result != cgltf_result_success) {
    return nullptr;
  }

  result = cgltf_load_buffers(&options, data, path.c_str());
  if (result != cgltf_result_success) {
    cgltf_free(data);
    return nullptr;
  }

  result = cgltf_validate(data);
  if (result != cgltf_result_success) {
    cgltf_free(data);
    return nullptr;
  }

  auto model = std::make_unique<GLTFModel>();

  // Get base path for loading textures
  std::filesystem::path basePath = std::filesystem::path(path).parent_path();

  // Load all textures
  for (size_t i = 0; i < data->textures_count; ++i) {
    auto texture = loadTexture(device, &data->textures[i], data, basePath.string());
    model->textures.push_back(texture);
  }

  // Load all materials
  for (size_t i = 0; i < data->materials_count; ++i) {
    auto mat = std::make_shared<Material>();
    auto& gltfMat = data->materials[i];

    // Base color factor
    if (gltfMat.has_pbr_metallic_roughness) {
      auto& pbr = gltfMat.pbr_metallic_roughness;
      mat->setBaseColor(glm::vec4(
          pbr.base_color_factor[0],
          pbr.base_color_factor[1],
          pbr.base_color_factor[2],
          pbr.base_color_factor[3]));
      mat->setMetallic(pbr.metallic_factor);
      mat->setRoughness(pbr.roughness_factor);

      // Base color texture
      if (pbr.base_color_texture.texture) {
        size_t texIndex = pbr.base_color_texture.texture - data->textures;
        if (texIndex < model->textures.size()) {
          mat->setTexture("baseColor", model->textures[texIndex]);
        }
      }
    }

    model->materials.push_back(mat);
  }

  // Load all meshes
  for (size_t i = 0; i < data->meshes_count; ++i) {
    auto mesh = createMeshFromGLTF(device, &data->meshes[i], data);
    if (mesh) {
      // Assign material if available
      cgltf_mesh* gltfMesh = &data->meshes[i];
      if (gltfMesh->primitives_count > 0 && gltfMesh->primitives[0].material) {
        size_t matIndex = gltfMesh->primitives[0].material - data->materials;
        if (matIndex < model->materials.size()) {
          mesh->setMaterial(model->materials[matIndex]);
        }
      }
      model->meshes.push_back(mesh);
    }
  }

  // Load all nodes and build scene graph
  model->nodes.resize(data->nodes_count);
  for (size_t i = 0; i < data->nodes_count; ++i) {
    model->nodes[i] = std::make_shared<GLTFNode>();
    cgltf_node* gltfNode = &data->nodes[i];

    // Set node name
    if (gltfNode->name) {
      model->nodes[i]->name = gltfNode->name;
    }

    // Build transform matrix from TRS
    glm::mat4 transform(1.0f);

    if (gltfNode->has_matrix) {
      // Use matrix directly
      transform = glm::make_mat4(gltfNode->matrix);
    } else {
      // Build from TRS components
      glm::vec3 translation(0.0f);
      glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
      glm::vec3 scale(1.0f);

      if (gltfNode->has_translation) {
        translation = glm::vec3(gltfNode->translation[0], gltfNode->translation[1], gltfNode->translation[2]);
      }
      if (gltfNode->has_rotation) {
        rotation = glm::quat(gltfNode->rotation[3], gltfNode->rotation[0],
                             gltfNode->rotation[1], gltfNode->rotation[2]);
      }
      if (gltfNode->has_scale) {
        scale = glm::vec3(gltfNode->scale[0], gltfNode->scale[1], gltfNode->scale[2]);
      }

      // Compose TRS matrix
      transform = glm::translate(glm::mat4(1.0f), translation) *
                  glm::mat4_cast(rotation) *
                  glm::scale(glm::mat4(1.0f), scale);
    }

    model->nodes[i]->transform = transform;

    // Attach mesh if present
    if (gltfNode->mesh) {
      size_t meshIndex = gltfNode->mesh - data->meshes;
      if (meshIndex < model->meshes.size()) {
        model->nodes[i]->mesh = model->meshes[meshIndex];
      }
    }
  }

  // Build node hierarchy (parent-child relationships)
  for (size_t i = 0; i < data->nodes_count; ++i) {
    cgltf_node* gltfNode = &data->nodes[i];
    for (size_t j = 0; j < gltfNode->children_count; ++j) {
      size_t childIndex = gltfNode->children[j] - data->nodes;
      if (childIndex < model->nodes.size()) {
        model->nodes[i]->children.push_back(model->nodes[childIndex]);
      }
    }
  }

  // Find root nodes (nodes not referenced as children)
  std::vector<bool> isChild(data->nodes_count, false);
  for (size_t i = 0; i < data->nodes_count; ++i) {
    cgltf_node* gltfNode = &data->nodes[i];
    for (size_t j = 0; j < gltfNode->children_count; ++j) {
      size_t childIndex = gltfNode->children[j] - data->nodes;
      isChild[childIndex] = true;
    }
  }

  for (size_t i = 0; i < data->nodes_count; ++i) {
    if (!isChild[i]) {
      model->rootNodes.push_back(model->nodes[i]);
    }
  }

  // If there's a default scene, use its nodes as root nodes
  if (data->scene && data->scene->nodes_count > 0) {
    model->rootNodes.clear();
    for (size_t i = 0; i < data->scene->nodes_count; ++i) {
      size_t nodeIndex = data->scene->nodes[i] - data->nodes;
      if (nodeIndex < model->nodes.size()) {
        model->rootNodes.push_back(model->nodes[nodeIndex]);
      }
    }
  }

  cgltf_free(data);
  return model;
}

std::shared_ptr<Mesh> GLTFLoader::createMeshFromGLTF(igl::IDevice& device, void* gltfMeshPtr,
                                                      void* gltfDataPtr) {
  cgltf_mesh* gltfMesh = static_cast<cgltf_mesh*>(gltfMeshPtr);
  cgltf_data* gltfData = static_cast<cgltf_data*>(gltfDataPtr);

  if (gltfMesh->primitives_count == 0) {
    return nullptr;
  }

  auto mesh = std::make_shared<Mesh>();
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  // Process first primitive only for now
  cgltf_primitive* primitive = &gltfMesh->primitives[0];

  // Find position, normal, and texcoord accessors
  cgltf_accessor* positionAccessor = nullptr;
  cgltf_accessor* normalAccessor = nullptr;
  cgltf_accessor* texcoordAccessor = nullptr;

  for (size_t i = 0; i < primitive->attributes_count; ++i) {
    cgltf_attribute* attr = &primitive->attributes[i];
    if (attr->type == cgltf_attribute_type_position) {
      positionAccessor = attr->data;
    } else if (attr->type == cgltf_attribute_type_normal) {
      normalAccessor = attr->data;
    } else if (attr->type == cgltf_attribute_type_texcoord) {
      texcoordAccessor = attr->data;
    }
  }

  if (!positionAccessor) {
    return nullptr;
  }

  size_t vertexCount = positionAccessor->count;
  vertices.resize(vertexCount);

  // Extract positions
  for (size_t i = 0; i < vertexCount; ++i) {
    float pos[3];
    cgltf_accessor_read_float(positionAccessor, i, pos, 3);
    vertices[i].position = glm::vec3(pos[0], pos[1], pos[2]);
    vertices[i].tangent = glm::vec3(0.0f);  // Initialize tangent to zero
  }

  // Extract normals
  if (normalAccessor && normalAccessor->count == vertexCount) {
    for (size_t i = 0; i < vertexCount; ++i) {
      float norm[3];
      cgltf_accessor_read_float(normalAccessor, i, norm, 3);
      vertices[i].normal = glm::vec3(norm[0], norm[1], norm[2]);
    }
  } else {
    // Generate flat normals if not present
    for (size_t i = 0; i < vertexCount; ++i) {
      vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
  }

  // Extract texture coordinates
  if (texcoordAccessor && texcoordAccessor->count == vertexCount) {
    for (size_t i = 0; i < vertexCount; ++i) {
      float uv[2];
      cgltf_accessor_read_float(texcoordAccessor, i, uv, 2);
      vertices[i].texCoord = glm::vec2(uv[0], uv[1]);
    }
  } else {
    for (size_t i = 0; i < vertexCount; ++i) {
      vertices[i].texCoord = glm::vec2(0.0f, 0.0f);
    }
  }

  // Extract indices
  if (primitive->indices) {
    size_t indexCount = primitive->indices->count;
    indices.resize(indexCount);
    for (size_t i = 0; i < indexCount; ++i) {
      indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(primitive->indices, i));
    }
  } else {
    // No indices, create sequential indices
    indices.resize(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
      indices[i] = static_cast<uint32_t>(i);
    }
  }

  mesh->setVertices(vertices);
  mesh->setIndices(indices);
  mesh->calculateBounds();

  // Create IGL buffers
  igl::BufferDesc vbDesc(igl::BufferDesc::BufferTypeBits::Vertex, vertices.data(),
                         vertices.size() * sizeof(Vertex));
  auto vb = device.createBuffer(vbDesc, nullptr);
  mesh->setVertexBuffer(std::shared_ptr<igl::IBuffer>(std::move(vb)));

  igl::BufferDesc ibDesc(igl::BufferDesc::BufferTypeBits::Index, indices.data(),
                         indices.size() * sizeof(uint32_t));
  auto ib = device.createBuffer(ibDesc, nullptr);
  mesh->setIndexBuffer(std::shared_ptr<igl::IBuffer>(std::move(ib)));

  return mesh;
}

std::shared_ptr<igl::ITexture> GLTFLoader::loadTexture(igl::IDevice& device,
                                                        void* gltfTexturePtr,
                                                        void* gltfDataPtr,
                                                        const std::string& basePath) {
  cgltf_texture* gltfTexture = static_cast<cgltf_texture*>(gltfTexturePtr);
  cgltf_data* data = static_cast<cgltf_data*>(gltfDataPtr);

  if (!gltfTexture->image || !gltfTexture->image->uri) {
    return nullptr;
  }

  // Build full path to image
  std::filesystem::path imagePath = std::filesystem::path(basePath) / gltfTexture->image->uri;

  // Load image using stb_image
  int width, height, channels;
  unsigned char* imageData = stbi_load(imagePath.string().c_str(), &width, &height, &channels, 4);
  if (!imageData) {
    IGL_LOG_ERROR("Failed to load texture: %s\n", imagePath.string().c_str());
    return nullptr;
  }

  // Create IGL texture
  igl::TextureDesc texDesc;
  texDesc.type = igl::TextureType::TwoD;
  texDesc.width = width;
  texDesc.height = height;
  texDesc.format = igl::TextureFormat::RGBA_UNorm8;
  texDesc.usage = igl::TextureDesc::TextureUsageBits::Sampled;
  texDesc.debugName = gltfTexture->name ? gltfTexture->name : imagePath.filename().string();

  auto texture = device.createTexture(texDesc, nullptr);
  if (texture) {
    texture->upload(igl::TextureRangeDesc::new2D(0, 0, width, height), imageData);
  }

  stbi_image_free(imageData);
  return texture;
}

} // namespace engine
