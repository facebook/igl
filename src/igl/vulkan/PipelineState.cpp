/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "PipelineState.h"

#include <igl/vulkan/VulkanPipelineLayout.h>

namespace igl::vulkan {

VkPipelineLayout PipelineState::getVkPipelineLayout() const {
  IGL_ASSERT(pipelineLayout_);

  return pipelineLayout_->getVkPipelineLayout();
}

} // namespace igl::vulkan
