/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace igl::tests {

// A collection of SPIR-V modules useful for testing. Disassembly can be see in the .cpp file for
// clarity.

const std::vector<uint32_t>& getUniformBufferSpvWords();
const std::vector<uint32_t>& getTextureSpvWords();
const std::vector<uint32_t>& getTextureWithDescriptorSetSpvWords();
const std::vector<uint32_t>& getTinyMeshFragmentShaderSpvWords();

} // namespace igl::tests
