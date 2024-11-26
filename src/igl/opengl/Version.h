/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/DeviceFeatures.h>
#include <string>
#include <utility>

namespace igl::opengl {

enum class GLVersion {
  NotAvailable,
  v1_1,
  v2_0_ES,
  v2_0,
  v2_1,
  v3_0_ES,
  v3_0,
  v3_1_ES,
  v3_1,
  v3_2_ES,
  v3_2,
  v3_3,
  v4_0,
  v4_1,
  v4_2,
  v4_3,
  v4_4,
  v4_5,
  v4_6
};

std::pair<uint32_t, uint32_t> parseVersionString(const char* version);
GLVersion getGLVersion(const char* version, bool constrain = false);

ShaderVersion getShaderVersion(GLVersion version);

// Returns the version tag to provide at the top of the shader
std::string getStringFromShaderVersion(ShaderVersion version);

} // namespace igl::opengl
