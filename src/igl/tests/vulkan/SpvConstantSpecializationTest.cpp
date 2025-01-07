/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/util/SpvConstantSpecialization.h>

#include <gtest/gtest.h>

namespace igl::tests {
// GLSL:
//
//   layout(constant_id = 0) const int kConstant0 = 10;
//   layout(constant_id = 1) const int kConstant1 = 11;
//
//   out vec4 fragColor;
//
//   void main() {
//     fragColor = vec4(float(kConstant0), float(kConstant1), 0.0, 1.0);
//   }

// SPIR-V:
//
//                  OpCapability Shader
//                  OpMemoryModel Logical Simple
//                  OpEntryPoint Fragment %main "main" %fragColor
//                  OpName %kConstant0 "kConstant0"
//                  OpName %kConstant1 "kConstant1"
//                  OpName %fragColor "fragColor"
//                  OpName %main "main"
//                  OpDecorate %kConstant0 SpecId 0
//                  OpDecorate %kConstant1 SpecId 1
//           %int = OpTypeInt 32 1
//    %kConstant0 = OpSpecConstant %int 10
//    %kConstant1 = OpSpecConstant %int 11
//         %float = OpTypeFloat 32
//       %v4float = OpTypeVector %float 4
//   %ptr_Output_v4float = OpTypePointer Output %v4float
//     %fragColor = OpVariable %ptr_Output_v4float Output
//          %void = OpTypeVoid
//          %func = OpTypeFunction %void
//         %_0_0f = OpConstant %float 0.0
//         %_1_0f = OpConstant %float 1.0
//          %main = OpFunction %void None %func
//            %_1 = OpLabel
//            %_2 = OpConvertSToF %float %kConstant0
//            %_3 = OpConvertSToF %float %kConstant1
//            %_4 = OpCompositeConstruct %v4float %_2 %_3 %_0_0f %_1_0f
//                  OpStore %fragColor %_4
//                  OpReturn
//                  OpFunctionEnd

namespace {
uint32_t getWord(int32_t val) {
  return *reinterpret_cast<uint32_t*>(&val);
}
} // namespace

TEST(SpvConstantSpecializationTest, intSpecialization) {
  using namespace vulkan::util;
  std::vector<uint32_t> spv = {
      0x07230203, 0x00010300, 0xdeadbeef, 0x00000011, 0x00000000, 0x00020011, 0x00000001,
      0x0003000e, 0x00000000, 0x00000000, 0x0006000f, 0x00000004, 0x00000001, 0x6e69616d,
      0x00000000, 0x00000002, 0x00050005, 0x00000003, 0x6e6f436b, 0x6e617473, 0x00003074,
      0x00050005, 0x00000004, 0x6e6f436b, 0x6e617473, 0x00003174, 0x00050005, 0x00000002,
      0x67617266, 0x6f6c6f43, 0x00000072, 0x00040005, 0x00000001, 0x6e69616d, 0x00000000,
      0x00040047, 0x00000003, 0x00000001, 0x00000000, 0x00040047, 0x00000004, 0x00000001,
      0x00000001, 0x00040015, 0x00000005, 0x00000020, 0x00000001, 0x00040032, 0x00000005,
      0x00000003, 0x0000000a, 0x00040032, 0x00000005, 0x00000004, 0x0000000b, 0x00030016,
      0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020,
      0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000002, 0x00000003,
      0x00020013, 0x00000009, 0x00030021, 0x0000000a, 0x00000009, 0x0004002b, 0x00000006,
      0x0000000b, 0x00000000, 0x0004002b, 0x00000006, 0x0000000c, 0x3f800000, 0x00050036,
      0x00000009, 0x00000001, 0x00000000, 0x0000000a, 0x000200f8, 0x0000000d, 0x0004006f,
      0x00000006, 0x0000000e, 0x00000003, 0x0004006f, 0x00000006, 0x0000000f, 0x00000004,
      0x00070050, 0x00000007, 0x00000010, 0x0000000e, 0x0000000f, 0x0000000b, 0x0000000c,
      0x0003003e, 0x00000002, 0x00000010, 0x000100fd, 0x00010038,

  };

  // Specialize kConstant0 to 0 and kConstant1 to 1
  const std::vector<uint32_t> values = {getWord(0), getWord(1)};

  EXPECT_EQ(spv[50], getWord(10)); // 0x0000000a above
  EXPECT_EQ(spv[54], getWord(11)); // 0x0000000b

  specializeConstants(spv.data(), spv.size() * sizeof(uint32_t), values);

  EXPECT_EQ(spv[50], getWord(0));
  EXPECT_EQ(spv[54], getWord(1));
}

} // namespace igl::tests
