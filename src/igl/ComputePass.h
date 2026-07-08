/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/Common.h> // IWYU pragma: keep

namespace igl {

class ITimestampQueries;

/**
 * @brief ComputePassDesc carries optional per-compute-pass attachments. Currently it scopes the GPU
 * timestamp-query handle and slot used to bracket the compute pass; additional fields can be added
 * here without touching every backend's createComputeCommandEncoder() signature.
 */
struct ComputePassDesc {
  /// Per-compute-pass GPU timestamp query attachment.
  /// When set, the backend measures GPU execution time for this compute pass.
  /// slotIndex is a logical timing slot allocated by GPUTimingCollector.
  /// Metal sets MTLComputePassDescriptor::sampleBufferAttachments[0] with
  /// startOfEncoderSampleIndex = slot*2 and endOfEncoderSampleIndex = slot*2+1; resolved samples
  /// land in the same accessor surface used by render passes so consumers can iterate slots
  /// uniformly across pass types.
  struct TimestampQueryDesc {
    std::shared_ptr<ITimestampQueries> queries;
    uint32_t slotIndex = 0; ///< Logical timing slot from GPUTimingCollector
  };
  /// Optional per-compute-pass GPU timing. Null queries pointer means disabled.
  TimestampQueryDesc timestampQuery;
};

} // namespace igl
