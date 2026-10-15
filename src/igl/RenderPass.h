/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <type_traits>
#include <vector>
#include <igl/Common.h> // IWYU pragma: keep

namespace igl {

class IFramebuffer;
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

  static_assert(std::is_trivially_copyable_v<AttachmentDesc>);

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
  /// Optional per-render-pass GPU timing. Null queries pointer means disabled.
  TimestampQueryDesc timestampQuery;

  /**
   * @brief Describes one subpass of a render pass.
   *
   * On tiled GPUs, data written by a subpass stays in tile memory and later subpasses read it at
   * the same pixel through input attachments. Bit i of each mask refers to colorAttachments[i].
   * A fragment shader reads input attachment i as:
   *  - Vulkan: layout(input_attachment_index = i, set = 2, binding = IGL_STORAGE_IMAGES_MAX + i)
   *            uniform subpassInput
   *  - Metal: a fragment input with the [[color(i)]] attribute; writes are not masked, so the
   *           fragment function outputs only the attachments in colorAttachmentMask
   *  - OpenGL: layout(location = i) inout with GL_EXT_shader_framebuffer_fetch; the backend masks
   *            writes outside colorAttachmentMask
   *
   * In a render pass on a FramebufferMode::Stereo framebuffer (DeviceFeatures::MultiviewSubpasses),
   * every subpass renders both views, and an input attachment reads the layer of the current view
   * with the same fragment shader declaration as above.
   */
  struct SubpassDesc {
    uint8_t colorAttachmentMask = 0; ///< Color attachments written by this subpass
    uint8_t inputAttachmentMask = 0; ///< Color attachments read by this subpass
    bool operator==(const SubpassDesc& other) const = default;
    bool operator!=(const SubpassDesc& other) const = default;
  };
  // NOLINTNEXTLINE(readability-identifier-naming)
  static_assert(IGL_COLOR_ATTACHMENTS_MAX <= 8, "SubpassDesc masks are 8 bits wide");

  /**
   * @brief The subpasses of this render pass; IRenderCommandEncoder::nextSubpass() advances
   * between them. Empty means a single subpass that writes every color attachment. Otherwise
   * requires DeviceFeatures::Subpasses and:
   *  - 2 to IGL_SUBPASSES_MAX entries
   *  - a Mono framebuffer, or a Stereo framebuffer when DeviceFeatures::MultiviewSubpasses is
   *    available. In a Stereo render pass every subpass renders both views and an input attachment
   *    reads the layer of the current view, so the color and depth attachments need at least 2
   *    layers from the attached layer
   *  - single-sampled attachments, no resolve attachments, and color attachments contiguous from
   *    index 0
   *  - on OpenGL, color attachments that are textures: for a framebuffer without them, such as
   *    the window system's (CurrentFramebuffer), createRenderCommandEncoder() returns
   *    Result::Code::Unsupported
   *  - every color attachment is written by some subpass
   *  - every input attachment is written by an earlier subpass, is not written by the subpass that
   *    reads it, and has TextureDesc::TextureUsageBits::InputAttachment
   *  - pipelines used in a subpass declare every color attachment format
   *  - shaders write only the attachments in colorAttachmentMask and read only those in
   *    inputAttachmentMask
   */
  std::vector<SubpassDesc> subpasses;

  /// Validates the framebuffer's ResourceStorage::Memoryless attachments and `subpasses` against
  /// the framebuffer. A Memoryless attachment must load with LoadAction::Clear or
  /// LoadAction::DontCare and store with StoreAction::DontCare or StoreAction::MsaaResolve;
  /// MsaaResolve needs a multisampled attachment and a separate resolve texture. No attachment can
  /// resolve into a Memoryless texture. A framebuffer color attachment without a `colorAttachments`
  /// entry is checked as a default AttachmentDesc, which stores with StoreAction::Store. Stencil
  /// resolve targets are not checked. A Stereo framebuffer with `subpasses` needs color and depth
  /// attachments with at least 2 layers from the attached layer, which is the `layer` of the
  /// attachment's AttachmentDesc (Result::Code::ArgumentInvalid otherwise), and a Multiview
  /// framebuffer cannot have `subpasses` (Result::Code::Unsupported). Does not check
  /// DeviceFeatures; createRenderCommandEncoder() returns Result::Code::Unsupported when the device
  /// lacks DeviceFeatures::Subpasses, or DeviceFeatures::MultiviewSubpasses for a Stereo
  /// framebuffer. For a pass without `subpasses`, backends run this check in debug builds only and
  /// only log a failure.
  [[nodiscard]] Result validate(const IFramebuffer& framebuffer) const noexcept;
};

} // namespace igl
