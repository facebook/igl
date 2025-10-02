/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Transform.h"
#include "Component.h"
#include <memory>
#include <vector>
#include <string>

namespace engine {

class Mesh;
class Material;

class GameObject {
 public:
  GameObject() = default;
  explicit GameObject(const std::string& name) : name_(name) {}

  void setName(const std::string& name) {
    name_ = name;
  }

  const std::string& getName() const {
    return name_;
  }

  Transform& getTransform() {
    return transform_;
  }

  const Transform& getTransform() const {
    return transform_;
  }

  // Mesh/Material
  void setMesh(std::shared_ptr<Mesh> mesh) {
    mesh_ = std::move(mesh);
  }

  std::shared_ptr<Mesh> getMesh() const {
    return mesh_;
  }

  void setMaterial(std::shared_ptr<Material> material) {
    material_ = std::move(material);
  }

  std::shared_ptr<Material> getMaterial() const {
    return material_;
  }

  // Components
  template<typename T>
  T* addComponent() {
    auto component = std::make_unique<T>();
    component->setOwner(this);
    T* ptr = component.get();
    components_.push_back(std::move(component));
    return ptr;
  }

  template<typename T>
  T* getComponent() {
    for (auto& component : components_) {
      if (T* ptr = dynamic_cast<T*>(component.get())) {
        return ptr;
      }
    }
    return nullptr;
  }

  void update(float deltaTime) {
    for (auto& component : components_) {
      component->update(deltaTime);
    }
  }

  void fixedUpdate(float fixedDeltaTime) {
    for (auto& component : components_) {
      component->fixedUpdate(fixedDeltaTime);
    }
  }

 private:
  std::string name_;
  Transform transform_;
  std::shared_ptr<Mesh> mesh_;
  std::shared_ptr<Material> material_;
  std::vector<std::unique_ptr<Component>> components_;
};

} // namespace engine
