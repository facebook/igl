/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <utility>
#include <vector>

namespace igl {

enum ShaderStage : uint8_t {
  Stage_Vertex = 0,
  Stage_Fragment,
  Stage_Compute,
};

constexpr uint32_t kNumShaderStages = Stage_Compute + 1;

/**
 * @brief Descriptor used to construct a shader module.
 * @see igl::Device::createShaderModule
 */
struct ShaderModuleDesc {
  ShaderStage stage = Stage_Fragment;
  const char* data = nullptr;
  size_t dataSize = 0; // if `dataSize` is non-zero, interpret `data` as binary shader data
  std::string entryPoint = "main";
  std::string debugName;

  ShaderModuleDesc() = default;
  ShaderModuleDesc(const char* source, igl::ShaderStage stage, std::string debugName) :
    stage(stage), data(source), debugName(std::move(debugName)) {}
  ShaderModuleDesc(const void* data,
                   size_t dataLength,
                   igl::ShaderStage stage,
                   std::string debugName) :
    stage(stage),
    data(static_cast<const char*>(data)),
    dataSize(dataLength),
    debugName(std::move(debugName)) {
    IGL_ASSERT(dataSize);
  }
};

/**
 * @brief Represents an individual shader, such as a vertex shader or fragment shader.
 */
class IShaderModule {
 protected:
  explicit IShaderModule(ShaderModuleDesc desc) : desc_(std::move(desc)) {}
  virtual ~IShaderModule() = default;

 public:
  /** @brief Returns metadata about the shader module */
  const ShaderModuleDesc& desc() const noexcept {
    return desc_;
  }

 private:
  const ShaderModuleDesc desc_;
};

struct ShaderStages final {
  ShaderStages() = default;
  ShaderStages(std::shared_ptr<IShaderModule> vertexModule,
               std::shared_ptr<IShaderModule> fragmentModule) {
    modules_[Stage_Vertex] = std::move(vertexModule);
    modules_[Stage_Fragment] = std::move(fragmentModule);
  }

  explicit ShaderStages(std::shared_ptr<IShaderModule> computeModule) {
    modules_[Stage_Compute] = std::move(computeModule);
  }

  const IShaderModule* getModule(ShaderStage stage) const {
    IGL_ASSERT(stage < kNumShaderStages);
    return modules_[stage].get();
  }

  std::shared_ptr<IShaderModule> modules_[kNumShaderStages] = {};
};

} // namespace igl
