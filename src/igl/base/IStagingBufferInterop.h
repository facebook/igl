/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace igl::base {

/// @brief Information about a staging buffer region for external copy command generation
/// @note This struct is only valid until removeRegion() is called with it.
///       After removeRegion(), the info becomes invalid and must not be used.
struct StagingBufferRegionInfo {
  /// @brief Region handle for tracking
  uint64_t handle = 0;

  /// @brief Total size of the underlying staging buffer
  size_t size = 0;
};

/// @brief Information about the destination for an upload operation
struct UploadDestinationInfo {
  /// @brief Native handle to the destination resource (buffer or texture)
  void* nativeHandle = nullptr;

  /// @brief Byte offset within the destination resource
  size_t offset = 0;

  /// @brief Size of the upload region in bytes
  size_t size = 0;

  /// @brief Pointer to native struct with additional data for image uploads
  ///        E.g. texture destination may require mip level, face index, etc.
  ///        Must be set to nullptr for buffer uploads.
  void* imageData = nullptr;
};

/// @brief Base staging buffer interface for interoperability
class IStagingBufferInterop {
 public:
  /// @brief Allocate a staging buffer region of the specified size
  /// @param size The size in bytes of the region to allocate
  /// @return Info about the allocated region. Valid until removeRegion() is called.
  [[nodiscard]] virtual StagingBufferRegionInfo allocateRegion(size_t size) = 0;

  /// @brief Upload data to a buffer or texture via a staging buffer region
  /// @param region The staging buffer region info returned by allocateRegion()
  /// @param destInfo The destination info for the upload operation
  /// @param data Pointer to source data
  virtual void upload(const StagingBufferRegionInfo& region,
                      const UploadDestinationInfo& destInfo,
                      const void* data) = 0;

  /// @brief Release the staging buffer region
  /// @param info The staging buffer info returned by allocateRegion().
  ///             After this call, the info becomes invalid and must not be used.
  virtual void removeRegion(const StagingBufferRegionInfo& info) = 0;

 protected:
  // Protected to prevent destruction through this interface
  virtual ~IStagingBufferInterop() = default;
};

} // namespace igl::base
