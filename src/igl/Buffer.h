/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <igl/Common.h>
#include <igl/ITrackedResource.h>
#include <string>
#include <vector>

namespace igl {

// class forward declaration
class ICommandBuffer;

enum class IndexFormat : uint8_t {
  UInt16,
  UInt32,
};

struct BufferRange {
  size_t size;
  uintptr_t offset;
  BufferRange(size_t size = 0L, uintptr_t offset = 0L) : size(size), offset(offset) {}
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
    // @fb-only
  };

  using BufferType = uint8_t;

  enum BufferAPIHintBits : uint8_t {
    Atomic = 1 << 0,
    UniformBlock = 1 << 1, // Enforces UBO for OpenGL
    Query = 1 << 2,
    // @fb-only
    Ring = 1 << 4, // Metal/Vulkan: Ring buffers with memory for each swapchain image
    NoCopy = 1 << 5, // Metal: The buffer should re-use previously allocated memory.
  };

  using BufferAPIHint = uint8_t;
  /** @brief Data to upload at the time of creation. Can be nullptr. */
  const void* IGL_NULLABLE data;

  /** @brief Total internal store to allocate */
  size_t length;

  /**
   * @brief Storage mode.
   * @See igl::ResourceStorage
   */
  ResourceStorage storage;
  /**
   * @brief Backend API hint flags.
   *
   */
  BufferAPIHint hint = 0;
  /**
   * @brief A bit mask of BufferTypeBits. All usage types must be specified.
   * @See  igl::BufferType
   */
  BufferType type = 0;

  /** @brief Identifier used for debugging */
  std::string debugName;

  BufferDesc(BufferType type = 0,
             const void* IGL_NULLABLE data = nullptr,
             size_t length = 0,
             ResourceStorage storageIn = ResourceStorage::Invalid,
             BufferAPIHint hint = 0,
             std::string debugName = std::string()) :
    data(data),
    length(length),
    storage(storageIn),
    hint(hint),
    type(type),
    debugName(std::move(debugName)) {
    if (storage == ResourceStorage::Invalid) {
#if IGL_PLATFORM_MACOS
      storage = ResourceStorage::Managed;
#else
      storage = ResourceStorage::Shared;
#endif
    }
    // We fallback to the old UniformBuffer for OpenGL by default
  }
};

class IBuffer : public ITrackedResource<IBuffer> {
 public:
  ~IBuffer() override = default;

  /**
   * @brief Upload data into a range in IBuffer.
   *
   * @param data data to be uploaded
   * @param range offset (in IBuffer) and size
   * @return Result::Code::ArgumentOutOfRange if offset + size is > IBuffer size
   * @return Result::Code::Ok On success
   * @remark data is allowed to be a nullptr if acceptedApiHints() includes
   * BufferAPIHintBits::NoCopy. In this case, a nullptr means that the specified range has been
   * updated. In all other situations, data MUST NOT be a nullptr.
   */
  virtual Result upload(const void* IGL_NULLABLE data, const BufferRange& range) = 0;

  /**
   * @brief Map a portion of the contents of a GPU Buffer into memory. Not efficient on OpenGL.
   * unmap() must be called before the buffer is used again in any GPU operations.
   *
   * @param range offset (in IBuffer) and size
   * @param outResult result of the operation,  Result::Code::Ok On success
   * @return a pointer to the data mapped into memory.
   */
  virtual void* IGL_NULLABLE map(const BufferRange& range, Result* IGL_NULLABLE outResult) = 0;

  /**
   * @brief Unmap a GPU Buffer from memory. Should be called after map().
   *
   */
  virtual void unmap() = 0;

  /**
   * @brief Returns the API hints that were requested in the buffer's descriptor.
   * @remark It is NOT guaranteed that all of these hints were accepted and used. Use
   * acceptedApiHints to get those.
   */
  [[nodiscard]] virtual BufferDesc::BufferAPIHint requestedApiHints() const noexcept = 0;

  /**
   * @brief Returns the API hints that were accepted and used in the buffer's creation.
   */
  [[nodiscard]] virtual BufferDesc::BufferAPIHint acceptedApiHints() const noexcept = 0;

  /**
   * @brief Returns the storage mode for the buffer.
   */
  [[nodiscard]] virtual ResourceStorage storage() const noexcept = 0;

  /**
   * @brief Returns current size of IBuffer
   *
   * @return Current allocated size
   */
  [[nodiscard]] virtual size_t getSizeInBytes() const = 0;

  /**
   * @brief Returns a buffer id suitable for bindless rendering (buffer_device_address on Vulkan and
   * gpuResourceID on Metal)
   *
   * @return uint64_t
   */
  [[nodiscard]] virtual uint64_t gpuAddress(size_t offset = 0) const = 0;

  /**
   * @brief Returns the underlying buffer type which is a mask of igl::BufferDesc::BufferTypeBits
   *
   * @return igl::BufferDesc::BufferType
   */
  [[nodiscard]] virtual BufferDesc::BufferType getBufferType() const = 0;

 protected:
  IBuffer() = default;
};

} // namespace igl
