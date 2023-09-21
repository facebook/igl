/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/util/SpvReflection.h>

#include <gtest/gtest.h>

namespace igl::tests {
namespace {
/**
               OpCapability Shader
               OpMemoryModel Logical Simple
               OpEntryPoint CombinedVF %main "main" %builtinPosition %_1
               OpMemberName %UBO0 0 "x0"
               OpName %UBO0 "UBO0"
               OpName %ubo0 "ubo0"
               OpName %x1 "x1"
               OpName %x2 "x2"
               OpMemberName %UBO3 0 "x3"
               OpName %UBO3 "UBO3"
               OpName %ubo3 "ubo3"
               OpName %f "f"
               OpName %main "main"
               OpDecorate %builtinPosition BuiltIn Position
               OpDecorate %_1 Location 0
               OpDecorate %UBO0 Block
               OpMemberDecorate %UBO0 0 Offset 0
               OpDecorate %ubo0 DescriptorSet 0
               OpDecorate %ubo0 Binding 0
               OpDecorate %UBO3 Block
               OpMemberDecorate %UBO3 0 Offset 0
               OpDecorate %ubo3 DescriptorSet 0
               OpDecorate %ubo3 Binding 3
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_Output_v4float = OpTypePointer Output %v4float
%builtinPosition = OpVariable %ptr_Output_v4float Output
         %_1 = OpVariable %ptr_Output_v4float Output
       %UBO0 = OpTypeStruct %float
%ptr_Uniform_UBO0 = OpTypePointer Uniform %UBO0
       %ubo0 = OpVariable %ptr_Uniform_UBO0 Uniform
        %int = OpTypeInt 32 1
       %_16i = OpConstant %int 16
%arr_float__16i = OpTypeArray %float %_16i
%ptr_UniformConstant_arr_float__16i = OpTypePointer UniformConstant %arr_float__16i
         %x1 = OpVariable %ptr_UniformConstant_arr_float__16i UniformConstant
%ptr_UniformConstant_float = OpTypePointer UniformConstant %float
         %x2 = OpVariable %ptr_UniformConstant_float UniformConstant
       %UBO3 = OpTypeStruct %float
%ptr_Uniform_UBO3 = OpTypePointer Uniform %UBO3
       %ubo3 = OpVariable %ptr_Uniform_UBO3 Uniform
       %void = OpTypeVoid
       %func = OpTypeFunction %void
%ptr_Uniform_float = OpTypePointer Uniform %float
        %_0i = OpConstant %int 0
%ptr_Function_v4float = OpTypePointer Function %v4float
       %main = OpFunction %void None %func
         %_2 = OpLabel
          %f = OpVariable %ptr_Function_v4float Function
         %_3 = OpAccessChain %ptr_Uniform_float %ubo0 %_0i
         %_4 = OpLoad %float %_3
         %_5 = OpLoad %arr_float__16i %x1
         %_6 = OpCompositeExtract %float %_5 1
         %_7 = OpLoad %float %x2
         %_8 = OpAccessChain %ptr_Uniform_float %ubo3 %_0i
         %_9 = OpLoad %float %_8
        %_10 = OpCompositeConstruct %v4float %_4 %_6 %_7 %_9
               OpStore %f %_10
        %_11 = OpLoad %v4float %f
               OpStore %builtinPosition %_11
        %_12 = OpLoad %v4float %f
               OpStore %_1 %_12
               OpReturn
               OpFunctionEnd
*/

const std::vector<uint32_t> kSpvWords = {
    0x07230203, 0x00010000, 0xdeadbeef, 0x0000003e, 0x00000000, 0x00020011, 0x00000001, 0x0003000e,
    0x00000000, 0x00000000, 0x0006000f, 0x00000000, 0x00000025, 0x6e69616d, 0x00000000, 0x00000002,
    0x0006000f, 0x00000004, 0x00000026, 0x6e69616d, 0x00000000, 0x00000003, 0x00030010, 0x00000026,
    0x00000008, 0x00040005, 0x00000004, 0x304f4255, 0x00000000, 0x00040006, 0x00000004, 0x00000000,
    0x00003078, 0x00040005, 0x00000008, 0x334f4255, 0x00000000, 0x00040006, 0x00000008, 0x00000000,
    0x00003378, 0x00040005, 0x00000005, 0x306f6275, 0x00000000, 0x00030005, 0x00000006, 0x00003178,
    0x00030005, 0x00000007, 0x00003278, 0x00040005, 0x00000009, 0x336f6275, 0x00000000, 0x00040005,
    0x00000025, 0x6e69616d, 0x00000000, 0x00030005, 0x00000037, 0x00000066, 0x00040005, 0x00000026,
    0x6e69616d, 0x00000000, 0x00030005, 0x00000038, 0x00000066, 0x00030047, 0x00000004, 0x00000002,
    0x00050048, 0x00000004, 0x00000000, 0x00000023, 0x00000000, 0x00030047, 0x00000008, 0x00000002,
    0x00050048, 0x00000008, 0x00000000, 0x00000023, 0x00000000, 0x00040047, 0x00000002, 0x0000000b,
    0x00000000, 0x00040047, 0x00000003, 0x0000001e, 0x00000000, 0x00040047, 0x00000005, 0x00000022,
    0x00000000, 0x00040047, 0x00000005, 0x00000021, 0x00000000, 0x00040047, 0x00000009, 0x00000022,
    0x00000000, 0x00040047, 0x00000009, 0x00000021, 0x00000003, 0x00030016, 0x0000000b, 0x00000020,
    0x00040015, 0x0000000f, 0x00000020, 0x00000001, 0x0004002b, 0x0000000f, 0x00000010, 0x00000010,
    0x0004002b, 0x0000000f, 0x00000018, 0x00000000, 0x00040017, 0x0000000c, 0x0000000b, 0x00000004,
    0x00040020, 0x0000000d, 0x00000003, 0x0000000c, 0x0003001e, 0x00000004, 0x0000000b, 0x00040020,
    0x0000000e, 0x00000002, 0x00000004, 0x0004001c, 0x00000011, 0x0000000b, 0x00000010, 0x00040020,
    0x00000012, 0x00000000, 0x00000011, 0x00040020, 0x00000013, 0x00000000, 0x0000000b, 0x0003001e,
    0x00000008, 0x0000000b, 0x00040020, 0x00000014, 0x00000002, 0x00000008, 0x00020013, 0x00000015,
    0x00030021, 0x00000016, 0x00000015, 0x00040020, 0x00000017, 0x00000002, 0x0000000b, 0x00040017,
    0x00000039, 0x0000000b, 0x00000002, 0x00040017, 0x0000003a, 0x0000000b, 0x00000003, 0x00040017,
    0x0000003b, 0x0000000f, 0x00000002, 0x00040017, 0x0000003c, 0x0000000f, 0x00000003, 0x00040017,
    0x0000003d, 0x0000000f, 0x00000004, 0x0004003b, 0x0000000d, 0x00000002, 0x00000003, 0x0004003b,
    0x0000000d, 0x00000003, 0x00000003, 0x0004003b, 0x0000000e, 0x00000005, 0x00000002, 0x0004003b,
    0x00000012, 0x00000006, 0x00000000, 0x0004003b, 0x00000013, 0x00000007, 0x00000000, 0x0004003b,
    0x00000014, 0x00000009, 0x00000002, 0x00050036, 0x00000015, 0x00000025, 0x00000000, 0x00000016,
    0x000200f8, 0x00000027, 0x00050041, 0x00000017, 0x00000029, 0x00000005, 0x00000018, 0x0004003d,
    0x0000000b, 0x0000002b, 0x00000029, 0x0004003d, 0x00000011, 0x0000002d, 0x00000006, 0x00050051,
    0x0000000b, 0x0000002f, 0x0000002d, 0x00000001, 0x0004003d, 0x0000000b, 0x00000031, 0x00000007,
    0x00050041, 0x00000017, 0x00000033, 0x00000009, 0x00000018, 0x0004003d, 0x0000000b, 0x00000035,
    0x00000033, 0x00070050, 0x0000000c, 0x00000037, 0x0000002b, 0x0000002f, 0x00000031, 0x00000035,
    0x0003003e, 0x00000002, 0x00000037, 0x000100fd, 0x00010038, 0x00050036, 0x00000015, 0x00000026,
    0x00000000, 0x00000016, 0x000200f8, 0x00000028, 0x00050041, 0x00000017, 0x0000002a, 0x00000005,
    0x00000018, 0x0004003d, 0x0000000b, 0x0000002c, 0x0000002a, 0x0004003d, 0x00000011, 0x0000002e,
    0x00000006, 0x00050051, 0x0000000b, 0x00000030, 0x0000002e, 0x00000001, 0x0004003d, 0x0000000b,
    0x00000032, 0x00000007, 0x00050041, 0x00000017, 0x00000034, 0x00000009, 0x00000018, 0x0004003d,
    0x0000000b, 0x00000036, 0x00000034, 0x00070050, 0x0000000c, 0x00000038, 0x0000002c, 0x00000030,
    0x00000032, 0x00000036, 0x0003003e, 0x00000003, 0x00000038, 0x000100fd, 0x00010038,
};
} // namespace

TEST(SpvReflectionTest, UniformBufferTest) {
  using namespace vulkan::util;
  SpvModuleInfo spvModuleInfo = getReflectionData(kSpvWords.data(), kSpvWords.size());

  ASSERT_EQ(spvModuleInfo.uniformBufferBindingLocations.size(), 2);
  EXPECT_EQ(spvModuleInfo.uniformBufferBindingLocations[0], 0);
  EXPECT_EQ(spvModuleInfo.uniformBufferBindingLocations[1], 3);
  EXPECT_EQ(spvModuleInfo.storageBufferBindingLocations.size(), 0);
}

} // namespace igl::tests
