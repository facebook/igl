/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/IGL.h>
#include <vector>

namespace iglu {
struct ManagedUniformBufferInfo {
  int index = -1;
  size_t length = 0;
  std::vector<igl::UniformDesc> uniforms;
};

class ManagedUniformBuffer {
 public:
  igl::Result result;
  ManagedUniformBufferInfo uniformInfo;
  ManagedUniformBuffer(igl::IDevice& device, const ManagedUniformBufferInfo& info);
  ~ManagedUniformBuffer();
  // This function takes a chunk of data and use it to update the value of uniform 'name'
  bool updateData(const char* name, const void* data, size_t dataSize);
  // This function returns the expected data size for uniform with given name
  // If uniform has type UniformType::Float3, this function will return
  // 3 * sizeof(float) if elementStride is zero and return elementStride otherwise
  // if no uniform with given name exists, the function will return 0
  size_t getUniformDataSize(const char* name);

  void bind(const igl::IDevice& device,
            const igl::IRenderPipelineState& pipelineState,
            igl::IRenderCommandEncoder& encoder);
  void bind(const igl::IDevice& device,
            const igl::IComputePipelineState& pipelineState,
            igl::IComputeCommandEncoder& encoder);

  void* getData();

  void buildUniformLUT();

  int getIndex(const char* name) const;

 private:
  size_t getUniformDataSizeInternal(igl::UniformDesc& uniform);
  void* data_ = nullptr;
  int length_ = 0;
  std::shared_ptr<igl::IBuffer> buffer_ = nullptr;
  std::unique_ptr<std::unordered_map<std::string, size_t>> uniformLUT_ = nullptr;
#if IGL_PLATFORM_IOS_SIMULATOR
  /// If we're in the simulator we need to hold onto length so we can deallocate memory buffer
  /// properly.
  /// If this is non-zero implies that we used vm alloc to allocate the memory instead of malloc
  /// since we don't hold onto the device to be able to use in the destructor
  size_t vmAllocLength_ = 0;
#endif
  bool useBindBytes_ = false;
};
} // namespace iglu
