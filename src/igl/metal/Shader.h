/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <memory>
#include <string>
#include <vector>
#include <igl/Shader.h>

namespace igl::metal {

class ShaderModule final : public IShaderModule {
  friend class Device;

 public:
  ShaderModule(ShaderModuleInfo info, id<MTLFunction> value);
  ~ShaderModule() override = default;

  IGL_INLINE id<MTLFunction> get() const {
    return value;
  }

  id<MTLFunction> value;
};

class ShaderLibrary final : public IShaderLibrary {
 public:
  explicit ShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules);
};

/**
 * @brief A Metal dynamic library: precompiled shader functions that pipelines link when they are
 * created, instead of carrying them in their own libraries.
 * @see PlatformDevice::createDynamicLibrary()
 */
class DynamicLibrary final {
 public:
  explicit DynamicLibrary(id<MTLDynamicLibrary> value) API_AVAILABLE(macos(11.0), ios(14.0));

  [[nodiscard]] id<MTLDynamicLibrary> get() const API_AVAILABLE(macos(11.0), ios(14.0));

  /**
   * @brief The name that libraries built against this one record for it, and link it by.
   */
  [[nodiscard]] const std::string& getInstallName() const noexcept;

 private:
  // An id<MTLDynamicLibrary>, held untyped so that this class compiles for deployment targets
  // older than macOS 11 / iOS 14.
  id value_ = nil;
  std::string installName_;
};

class ShaderStages final : public IShaderStages {
 public:
  explicit ShaderStages(ShaderStagesDesc desc);
  ShaderStages(ShaderStagesDesc desc, std::vector<std::shared_ptr<DynamicLibrary>> linkedLibraries);

  /**
   * @brief The dynamic libraries that pipelines created from these stages link. Empty unless the
   * stages came from PlatformDevice::createShaderStages().
   */
  [[nodiscard]] const std::vector<std::shared_ptr<DynamicLibrary>>& getLinkedLibraries()
      const noexcept;

 private:
  std::vector<std::shared_ptr<DynamicLibrary>> linkedLibraries_;
};

} // namespace igl::metal
