/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <vector>
#include <igl/Common.h> // IWYU pragma: keep

namespace igl {

class ITimestampQueries;

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
  struct AttachmentDesc {
    LoadAction loadAction = LoadAction::DontCare; // default load action for color
    StoreAction storeAction = StoreAction::Store; // default store action for color
    uint8_t face = 0; // Cube texture face
    uint8_t mipLevel = 0; // Texture mip level
    uint8_t layer = 0; // Texture array layer
    Color clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    float clearDepth = 1.0f;
    uint32_t clearStencil = 0;
  };

  /**
   * @brief colorAttachments properties which is empty by default.
   */
  std::vector<AttachmentDesc> colorAttachments;
  /**
   * @brief depthAttachment property which is clear to 1 and use provided MSAA by default
   */
  AttachmentDesc depthAttachment = {.loadAction = LoadAction::Clear,
                                    .storeAction = StoreAction::DontCare};
  /**
   * @brief stencilAttachment property which is clear to 0 by default
   */
  AttachmentDesc stencilAttachment = {.loadAction = LoadAction::Clear,
                                      .storeAction = StoreAction::DontCare};

  /// Per-render-pass GPU timestamp query attachment.
  /// When set, the backend measures GPU execution time for this render pass.
  /// slotIndex is a logical timing slot allocated by GPUTimingCollector.
  /// Metal maps it to two sampleBufferAttachments indices (slot*2, slot*2+1).
  /// OpenGL uses it as the GL_TIME_ELAPSED query index.
  struct TimestampQueryDesc {
    std::shared_ptr<ITimestampQueries> queries;
    uint32_t slotIndex = 0; ///< Logical timing slot from GPUTimingCollector
  };
  /// Optional per-render-pass timestamp query. Null queries pointer means disabled.
  TimestampQueryDesc timestampQuery;
};

} // namespace igl
