/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glslang/Include/glslang_c_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

void glslangGetDefaultResource(glslang_resource_t* resource);
void glslangGetDefaultInput(const char* shaderCode,
                            glslang_stage_t stage,
                            const glslang_resource_t* resource,
                            glslang_input_t* out);

#ifdef __cplusplus
}
#endif
