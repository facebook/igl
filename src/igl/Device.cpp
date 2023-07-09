/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Device.h>
#include <igl/Shader.h>

#include <assert.h>
#include <algorithm>
#include <utility>

namespace igl {

bool _IGLVerify(bool cond, const char* func, const char* file, int line, const char* format, ...) {
  if (!cond) {
    LLOGW("[IGL] Assert failed in '%s' (%s:%d): ", func, file, line);
    va_list ap;
    va_start(ap, format);
    LLOGW(format, ap);
    va_end(ap);
    assert(false);
  }
  return cond;
}

// Make sure the structure igl::Color is tightly packed so it can be passed into APIs which expect
// float[4] RGBA values
static_assert(sizeof(Color) == 4 * sizeof(float));

std::unique_ptr<ShaderStages> IDevice::createShaderStages(const char* vs,
                                                          const char* debugNameVS,
                                                          const char* fs,
                                                          const char* debugNameFS,
                                                          Result* outResult) const {
  auto VS = createShaderModule(igl::ShaderModuleDesc(vs, Stage_Vertex, debugNameVS), outResult);
  auto FS = createShaderModule(igl::ShaderModuleDesc(fs, Stage_Fragment, debugNameFS), outResult);
  return std::make_unique<igl::ShaderStages>(VS, FS);
}

std::unique_ptr<ShaderStages> IDevice::createShaderStages(const char* cs,
                                                          const char* debugName,
                                                          Result* outResult) const {
  return std::make_unique<igl::ShaderStages>(
      createShaderModule(igl::ShaderModuleDesc(cs, Stage_Compute, debugName), outResult));
}

} // namespace igl
