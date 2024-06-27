/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>

#include <cstdlib>
#include <igl/Macros.h>

#if defined(IGL_CMAKE_BUILD)
#include <igl/IGLSafeC.h>
#else
#include <secure_lib/secure_string.h>
#endif

#if IGL_PLATFORM_APPLE
#include <unistd.h>
#if IGL_PLATFORM_IOS_SIMULATOR
#include <mach/mach_init.h>
#include <mach/mach_types.h>
#include <mach/vm_map.h>
#endif
#endif

namespace iglu {
ManagedUniformBuffer::ManagedUniformBuffer(igl::IDevice& device,
                                           const ManagedUniformBufferInfo& info) :
  uniformInfo(info) {
  igl::BufferDesc desc;
  desc.length = info.length;

  if (!IGL_VERIFY(desc.length != 0)) {
    result.code = igl::Result::Code::ArgumentInvalid;
    return;
  }

  // Currently, the OpenGL code path always uses individual uniforms so no need to allocate a
  // buffer.
  bool createBuffer = device.getBackendType() != igl::BackendType::OpenGL;
  // Allocate memory
  if (device.getBackendType() == igl::BackendType::Metal) {
#if IGL_PLATFORM_APPLE

    // Metal must be page aligned
    auto pageSize = getpagesize();

    length_ = desc.length;
    const int roundVal = 16;
    // bindBytes requires specific alignment. Algin to 16b is a safe bet.
    length_ = ((length_ + roundVal - 1) / roundVal) * roundVal;
    useBindBytes_ = (length_ < pageSize);
    if (useBindBytes_) {
      data_ = malloc(length_);
      createBuffer = false;
    } else {
      auto pagesRequired = desc.length / pageSize;
      if (desc.length % pageSize != 0) {
        pagesRequired++;
      }
      desc.length = pagesRequired * pageSize;

#if IGL_PLATFORM_IOS_SIMULATOR
      // The simulator will crash if we use memory created with posix_memalign, so we use this
      // per what documentation says here
      // https://developer.apple.com/documentation/metal/gpu_selection_in_macos/selecting_device_objects_for_compute_processing?language=objc#3544751
      vmAllocLength_ = desc.length;
      kern_return_t err = vm_allocate(
          (vm_map_t)mach_task_self(), (vm_address_t*)&data_, vmAllocLength_, VM_FLAGS_ANYWHERE);
      if (err != KERN_SUCCESS) {
        data_ = nullptr;
      }
#else
      auto failure = posix_memalign(&data_, pageSize, desc.length);
      if (failure) {
        data_ = nullptr;
      }
#endif
    }

#endif
  } else {
    data_ = malloc(desc.length);
  }
  if (data_ == nullptr) {
    result.code = igl::Result::Code::RuntimeError;
    return;
  }
  if (createBuffer) {
    desc.data = data_;
    desc.type = igl::BufferDesc::BufferTypeBits::Uniform;
    desc.storage = igl::ResourceStorage::Shared;

    if (device.hasFeature(igl::DeviceFeatures::BufferNoCopy)) {
      desc.type |= igl::BufferDesc::BufferAPIHintBits::NoCopy;
    }
    buffer_ = device.createBuffer(desc, &result);
  }
}

ManagedUniformBuffer::~ManagedUniformBuffer() {
#if IGL_PLATFORM_IOS_SIMULATOR
  if (vmAllocLength_) {
    // if vmAllocLength_ is nonzero it implies we used vm_alloc to allocate the memory
    vm_deallocate((vm_map_t)mach_task_self(), (vm_address_t)data_, vmAllocLength_);
  } else {
#endif
    free(data_);
#if IGL_PLATFORM_IOS_SIMULATOR
  }
#endif
}

void ManagedUniformBuffer::bind(const igl::IDevice& device,
                                const igl::IRenderPipelineState& pipelineState,
                                igl::IRenderCommandEncoder& encoder) {
  if (device.getBackendType() == igl::BackendType::OpenGL) {
#if IGL_BACKEND_OPENGL && !IGL_PLATFORM_MACCATALYST
    for (auto& uniform : uniformInfo.uniforms) {
      // Since the backend is opengl, getIndexByName's igl::ShaderStage parameter is ignored and
      // will work when binding vertex/fragment
      // Might be optimized to use NameHandle
      uniform.location = pipelineState.getIndexByName(igl::genNameHandle(uniform.name),
                                                      igl::ShaderStage::Fragment);

      if (uniform.location >= 0) {
        encoder.bindUniform(uniform, data_);
      } else {
        IGL_LOG_ERROR_ONCE("The uniform %s was not found in shader\n", uniform.name.c_str());
      }
    }
#else
    IGL_ASSERT_MSG(0, "Should not use OpenGL backend on Mac Catalyst, use Metal instead\n");
#endif
  } else {
    if (useBindBytes_) {
      encoder.bindBytes(uniformInfo.index, igl::BindTarget::kAllGraphics, data_, length_);
    } else {
      // Need to ensure the latest data is present in the buffer
      // TODO: Have callers handle this when data has changed.
      void* data = data_;
      if (buffer_->acceptedApiHints() & igl::BufferDesc::BufferAPIHintBits::NoCopy) {
        data = nullptr;
      }
      buffer_->upload(data, {buffer_->getSizeInBytes(), 0});
      encoder.bindBuffer(uniformInfo.index, buffer_, 0);
    }
  }
}

void ManagedUniformBuffer::bind(const igl::IDevice& device, igl::IComputeCommandEncoder& encoder) {
  if (device.getBackendType() == igl::BackendType::OpenGL) {
    IGL_ASSERT_MSG(0, "No ComputeEncoder supported for OpenGL\n");
  } else {
    if (useBindBytes_) {
      encoder.bindBytes(uniformInfo.index, data_, length_);
    } else {
      // Need to ensure the latest data is present in the buffer
      // TODO: Have callers handle this when data has changed.
      void* data = data_;
      if (buffer_->acceptedApiHints() & igl::BufferDesc::BufferAPIHintBits::NoCopy) {
        data = nullptr;
      }
      buffer_->upload(data, {buffer_->getSizeInBytes(), 0});
      encoder.bindBuffer(uniformInfo.index, buffer_, 0);
    }
  }
}

void* ManagedUniformBuffer::getData() {
  return data_;
}

void ManagedUniformBuffer::buildUnifromLUT() {
  uniformLUT_ = std::make_unique<std::unordered_map<std::string, size_t>>();
  for (size_t i = 0; i < uniformInfo.uniforms.size(); ++i) {
    auto& uniform = uniformInfo.uniforms[i];
    uniformLUT_->insert({uniform.name, i});
  }
}

static int findUniformByName(const std::vector<igl::UniformDesc>& uniforms, const char* name) {
  for (size_t i = 0; i < uniforms.size(); ++i) {
    if (strcmp(name, uniforms[i].name.c_str()) == 0) {
      return i;
    }
  }
  return -1;
}

bool ManagedUniformBuffer::updateData(const char* name, const void* data, size_t dataSize) {
  IGL_ASSERT(name);

  int index = -1;
  if (uniformLUT_) {
    auto search = uniformLUT_->find(name);
    index = search != uniformLUT_->end() ? (int)search->second : -1;
  } else {
    index = findUniformByName(uniformInfo.uniforms, name);
  }

  if (index >= 0) {
    auto& uniform = uniformInfo.uniforms[index];
    if (strcmp(name, uniform.name.c_str()) == 0) {
      // If dataSize is smaller than the expected size, we will just update as client requested.
      // This could mean the user knows only a portion of the uniform data needs updating
      // However, if dataSize is larger than or equal to what we expect for this uniform, we will
      // only copy data up to the expected data size for this uniform
      const size_t uniformDataSize = getUniformDataSizeInternal(uniform);
      if (dataSize > uniformDataSize) {
        dataSize = uniformDataSize;
#if IGL_DEBUG
        IGL_LOG_INFO_ONCE(
            "IGLU/ManagedBufferBuffer/updateData: dataSize is larger than expected. This could be "
            "benign. See comments in updateData for more details. \n");
#endif
      }
      char* ptr = reinterpret_cast<char*>(data_);
      checked_memcpy(ptr + uniform.offset, uniformDataSize, data, dataSize);
      return true;
    }
  }
  IGL_ASSERT_MSG(0, "call to updateData: uniform with name %s not found, skipping update\n");
  return false;
}

size_t ManagedUniformBuffer::getUniformDataSize(const char* name) {
  for (auto& uniform : uniformInfo.uniforms) {
    if (strcmp(name, uniform.name.c_str()) == 0) {
      return getUniformDataSizeInternal(uniform);
    }
  }
  return 0;
}

size_t ManagedUniformBuffer::getUniformDataSizeInternal(igl::UniformDesc& uniform) {
  const size_t uniformDataSize = uniform.elementStride != 0
                                     ? uniform.numElements * uniform.elementStride
                                     : uniform.numElements * igl::sizeForUniformType(uniform.type);
  return uniformDataSize;
}

} // namespace iglu
