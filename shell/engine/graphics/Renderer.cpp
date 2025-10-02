/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Renderer.h"
#include "../scene/Scene.h"
#include "../scene/GameObject.h"
#include "../resources/Mesh.h"
#include "Camera.h"
#include "Material.h"
#include <shell/shared/platform/Platform.h>

namespace engine {

Renderer::Renderer(std::shared_ptr<igl::shell::Platform> platform)
    : platform_(std::move(platform)) {}

void Renderer::renderScene(Scene& scene) {
  if (!framebuffer_ || !commandQueue_) {
    return;
  }

  Camera* camera = scene.getMainCamera();
  if (!camera) {
    return;
  }

  // Create command buffer
  igl::CommandBufferDesc cbDesc;
  auto commandBuffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);

  // Render each game object
  for (const auto& gameObject : scene.getGameObjects()) {
    auto mesh = gameObject->getMesh();
    auto material = gameObject->getMaterial();

    if (mesh && material && material->getPipelineState()) {
      // TODO: Set up render pass encoder and draw calls
      // This will be implemented when we add proper shader support
    }
  }

  // Submit command buffer
  commandQueue_->submit(*commandBuffer);
}

} // namespace engine
