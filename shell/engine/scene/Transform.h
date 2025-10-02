/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine {

class Transform {
 public:
  Transform() = default;

  // Position
  void setPosition(const glm::vec3& position) {
    position_ = position;
    dirty_ = true;
  }

  const glm::vec3& getPosition() const {
    return position_;
  }

  // Rotation (using quaternions for smooth interpolation)
  void setRotation(const glm::quat& rotation) {
    rotation_ = rotation;
    dirty_ = true;
  }

  const glm::quat& getRotation() const {
    return rotation_;
  }

  void setEulerAngles(const glm::vec3& eulerAngles) {
    rotation_ = glm::quat(glm::radians(eulerAngles));
    dirty_ = true;
  }

  glm::vec3 getEulerAngles() const {
    return glm::degrees(glm::eulerAngles(rotation_));
  }

  // Scale
  void setScale(const glm::vec3& scale) {
    scale_ = scale;
    dirty_ = true;
  }

  const glm::vec3& getScale() const {
    return scale_;
  }

  // Transform matrix
  const glm::mat4& getLocalMatrix() const {
    if (dirty_) {
      updateMatrix();
    }
    return localMatrix_;
  }

  // Direction vectors
  glm::vec3 getForward() const {
    return glm::normalize(rotation_ * glm::vec3(0.0f, 0.0f, -1.0f));
  }

  glm::vec3 getRight() const {
    return glm::normalize(rotation_ * glm::vec3(1.0f, 0.0f, 0.0f));
  }

  glm::vec3 getUp() const {
    return glm::normalize(rotation_ * glm::vec3(0.0f, 1.0f, 0.0f));
  }

  // Transformation operations
  void translate(const glm::vec3& offset) {
    position_ += offset;
    dirty_ = true;
  }

  void rotate(const glm::quat& rotation) {
    rotation_ = rotation * rotation_;
    dirty_ = true;
  }

  void rotate(float angle, const glm::vec3& axis) {
    rotation_ = glm::angleAxis(glm::radians(angle), glm::normalize(axis)) * rotation_;
    dirty_ = true;
  }

 private:
  void updateMatrix() const {
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position_);
    glm::mat4 rotationMatrix = glm::mat4_cast(rotation_);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale_);
    localMatrix_ = translationMatrix * rotationMatrix * scaleMatrix;
    dirty_ = false;
  }

  glm::vec3 position_{0.0f};
  glm::quat rotation_{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion
  glm::vec3 scale_{1.0f};
  mutable glm::mat4 localMatrix_{1.0f};
  mutable bool dirty_ = true;
};

} // namespace engine
