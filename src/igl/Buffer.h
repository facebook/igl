/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

enum class IndexFormat : uint8_t {
  UInt16,
  UInt32,
};

enum class PrimitiveType : uint8_t {
  Point,
  Line,
  LineStrip,
  Triangle,
  TriangleStrip,
};

/**
 * @brief A buffer descriptor used to create a new IBuffer, e.g.
 * by IDevice->createBuffer()
 */
struct BufferDesc {
  enum BufferTypeBits : uint8_t {
    Index = 1 << 0,
    Vertex = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    Indirect = 1 << 4,
  };

  using BufferType = uint8_t;

  /** @brief Data to upload at the time of creation. Can be nullptr. */
  const void* data = nullptr;

  /** @brief Total internal store to allocate */
  size_t length = 0;

  /**
   * @brief Storage mode.
   * @See igl::ResourceStorage
   */
  ResourceStorage storage = ResourceStorage::Shared;

  /**
   * @brief GLES only. Target binding point for this IBuffer
   * @See  igl::BufferType
   */
  BufferType type; // GLES only

  /** @brief Identifier used for debugging */
  const char* debugName = "";

  BufferDesc(BufferType type = 0,
             const void* data = nullptr,
             size_t length = 0,
             ResourceStorage storageIn = ResourceStorage::Shared,
             const char* debugName = "") :
    data(data), length(length), storage(storageIn), type(type), debugName(debugName) {}
};

class IBuffer {
 public:
  virtual ~IBuffer() = default;

  virtual Result upload(const void* data, size_t size, size_t offset = 0) = 0;

  virtual uint8_t* getMappedPtr() const = 0;
  virtual uint64_t gpuAddress(size_t offset = 0) const = 0;

 protected:
  IBuffer() = default;
};

} // namespace igl
