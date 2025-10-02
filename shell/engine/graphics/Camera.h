/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine {

class Camera {
 public:
  enum class ProjectionType { Perspective, Orthographic };

  Camera() = default;

  // Projection
  void setPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) {
    projectionType_ = ProjectionType::Perspective;
    fov_ = fovDegrees;
    aspectRatio_ = aspectRatio;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    updateProjectionMatrix();
  }

  void setOrthographic(float left, float right, float bottom, float top, float nearPlane,
                       float farPlane) {
    projectionType_ = ProjectionType::Orthographic;
    orthoLeft_ = left;
    orthoRight_ = right;
    orthoBottom_ = bottom;
    orthoTop_ = top;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    updateProjectionMatrix();
  }

  // View matrix
  void setPosition(const glm::vec3& position) {
    position_ = position;
    updateViewMatrix();
  }

  const glm::vec3& getPosition() const {
    return position_;
  }

  void setTarget(const glm::vec3& target) {
    target_ = target;
    updateViewMatrix();
  }

  const glm::vec3& getTarget() const {
    return target_;
  }

  void setUp(const glm::vec3& up) {
    up_ = up;
    updateViewMatrix();
  }

  const glm::vec3& getUp() const {
    return up_;
  }

  // FPS camera control
  void setYaw(float yaw) {
    yaw_ = yaw;
    updateFPSVectors();
  }

  float getYaw() const {
    return yaw_;
  }

  void setPitch(float pitch) {
    pitch_ = glm::clamp(pitch, -89.0f, 89.0f);
    updateFPSVectors();
  }

  float getPitch() const {
    return pitch_;
  }

  void rotate(float deltaYaw, float deltaPitch) {
    yaw_ += deltaYaw;
    pitch_ += deltaPitch;
    pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
    updateFPSVectors();
  }

  glm::vec3 getForward() const {
    return glm::normalize(target_ - position_);
  }

  glm::vec3 getRight() const {
    return glm::normalize(glm::cross(getForward(), up_));
  }

  // Matrices
  const glm::mat4& getViewMatrix() const {
    return viewMatrix_;
  }

  const glm::mat4& getProjectionMatrix() const {
    return projectionMatrix_;
  }

  glm::mat4 getViewProjectionMatrix() const {
    return projectionMatrix_ * viewMatrix_;
  }

 private:
  void updateViewMatrix() {
    viewMatrix_ = glm::lookAt(position_, target_, up_);
  }

  void updateProjectionMatrix() {
    if (projectionType_ == ProjectionType::Perspective) {
      projectionMatrix_ = glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
    } else {
      projectionMatrix_ = glm::ortho(orthoLeft_, orthoRight_, orthoBottom_, orthoTop_, nearPlane_,
                                     farPlane_);
    }
  }

  void updateFPSVectors() {
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    direction.y = sin(glm::radians(pitch_));
    direction.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    target_ = position_ + glm::normalize(direction);
    updateViewMatrix();
  }

  ProjectionType projectionType_ = ProjectionType::Perspective;

  // Perspective params
  float fov_ = 60.0f;
  float aspectRatio_ = 16.0f / 9.0f;

  // Orthographic params
  float orthoLeft_ = -10.0f;
  float orthoRight_ = 10.0f;
  float orthoBottom_ = -10.0f;
  float orthoTop_ = 10.0f;

  // Common params
  float nearPlane_ = 0.1f;
  float farPlane_ = 1000.0f;

  // View params
  glm::vec3 position_{0.0f, 0.0f, 5.0f};
  glm::vec3 target_{0.0f, 0.0f, 0.0f};
  glm::vec3 up_{0.0f, 1.0f, 0.0f};

  // FPS camera
  float yaw_ = -90.0f;
  float pitch_ = 0.0f;

  // Matrices
  glm::mat4 viewMatrix_{1.0f};
  glm::mat4 projectionMatrix_{1.0f};
};

} // namespace engine
