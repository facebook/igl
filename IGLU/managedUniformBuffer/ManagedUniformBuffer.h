/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <vector>
#include <igl/IGL.h>

namespace iglu {
struct ManagedUniformBufferInfo {
  int index = -1;
  size_t length = 0;
  std::vector<igl::UniformDesc> uniforms;
  // Name of the GLSL uniform block these uniforms belong to, when they are backed by an interface
  // block (`uniform <blockName> { ... }`). On the OpenGL backend this lets bind() upload + bind the
  // block as a UBO when the linked program keeps it as a native block (GLSL ES 3.x), whose members
  // are not addressable via glGetUniformLocation(). Empty for plain (non-block) uniforms, in which
  // case bind() resolves each uniform individually as before.
  std::string blockName;
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

  // return the type of the uniform
  // return igl::UniformType::Invalid if name invalid
  igl::UniformType getUniformType(const char* name) const;

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
  // OpenGL only. When `uniformInfo.blockName` names a native uniform block in the linked program
  // (blockBindingPoint >= 0), uploads the packed block data to buffer_ and returns true so the
  // caller binds it as a UBO at blockBindingPoint. Returns false when there is no native block
  // (SPIRV-Cross flattened it to plain uniforms), in which case the caller binds per-uniform.
  bool bindOpenGLUniformBlock(int blockBindingPoint);
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
