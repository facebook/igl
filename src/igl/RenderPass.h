/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <vector>

namespace igl {

/**
 * @brief LoadAction determines the loading time action of the various components of a
 * RenderPassDesc. This can be DontCare, Load, or Clear.
 *
 * DontCare : No specific operation required.
 * Load : Preserve previous render contents
 * Clear : Clear render contents
 */
enum class LoadAction : uint8_t {
  DontCare,
  Load,
  Clear,
};

/**
 * @brief StoreAction determines the resolution action of the various components of a
 * RenderPassDesc. This can be DontCare, Store, or MsaaResolve.
 *
 * DontCare : No specific operation required.
 * Store : Preserve render contents
 * MsaaResolve : Use MSAA to preserve render contents
 */
enum class StoreAction : uint8_t {
  DontCare,
  Store,
  MsaaResolve,
};

/**
 * @brief RenderPassDesc provides the basis for describing a render pass and provides default
 * operations for depth and stencil.
 *
 * RenderPass by default does not contain any ColorAttachmentDesc operations.
 */
struct RenderPassDesc {
 public:
  /**
   * @brief BaseAttachmentDesc sets default load action, store action, cube face , mip level, and
   * array layer for the attachments of a RenderPassDesc
   */
  struct BaseAttachmentDesc {
    LoadAction loadAction = LoadAction::Clear; // default load action for depth and stencil
    StoreAction storeAction = StoreAction::DontCare; // default store action for depth and stencil
    uint8_t face = 0; // Cube texture face
    uint8_t mipLevel = 0; // Texture mip level
    uint8_t layer = 0; // Texture array layer
  };

  /**
   * @brief ColorAttachmentDesc stores to black by default on a RenderPassDesc but can be set to
   * perform other operations
   */
  struct ColorAttachmentDesc : BaseAttachmentDesc {
    // NOTE: Color has different defaults
    ColorAttachmentDesc() : BaseAttachmentDesc{LoadAction::DontCare, StoreAction::Store} {}
    Color clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
  };

  /**
   * @brief DepthAttachmentDesc uses provided MSAA value by default and clears depth.
   */
  struct DepthAttachmentDesc : BaseAttachmentDesc {
    float clearDepth = 1.0f;
  };

  /**
   * @brief StencilAttachmentDesc sets stencil to 0 by default.
   */
  struct StencilAttachmentDesc : BaseAttachmentDesc {
    uint32_t clearStencil = 0;
  };

  /**
   * @brief colorAttachments properties which is empty by default.
   */
  std::vector<ColorAttachmentDesc> colorAttachments;
  /**
   * @brief depthAttachment property which is clear to 1 and use provided MSAA by default
   */
  DepthAttachmentDesc depthAttachment;
  /**
   * @brief stencilAttachment property which is clear to 0 by default
   */
  StencilAttachmentDesc stencilAttachment;
};

} // namespace igl
