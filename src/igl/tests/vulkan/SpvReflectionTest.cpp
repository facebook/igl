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

const std::vector<uint32_t> kUniformBufferSpvWords = {
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

/**
               OpCapability Shader
               OpMemoryModel Logical Simple
               OpEntryPoint CombinedVF %main "main" %builtinPosition %_1
               OpName %s0 "s0"
               OpName %s1 "s1"
               OpName %s2 "s2"
               OpName %s3 "s3"
               OpName %f "f"
               OpName %main "main"
               OpDecorate %builtinPosition BuiltIn Position
               OpDecorate %_1 Location 0
               OpDecorate %s1 DescriptorSet 0
               OpDecorate %s1 Binding 1
               OpDecorate %s3 DescriptorSet 0
               OpDecorate %s3 Binding 3
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%ptr_Output_v4float = OpTypePointer Output %v4float
%builtinPosition = OpVariable %ptr_Output_v4float Output
         %_1 = OpVariable %ptr_Output_v4float Output
   %image_2D = OpTypeImage %float 2D 0 0 0 1 Unknown
%sampled_image_2D = OpTypeSampledImage %image_2D
%ptr_UniformConstant_sampled_image_2D = OpTypePointer UniformConstant %sampled_image_2D
         %s0 = OpVariable %ptr_UniformConstant_sampled_image_2D UniformConstant
         %s1 = OpVariable %ptr_UniformConstant_sampled_image_2D UniformConstant
         %s2 = OpVariable %ptr_UniformConstant_sampled_image_2D UniformConstant
         %s3 = OpVariable %ptr_UniformConstant_sampled_image_2D UniformConstant
       %void = OpTypeVoid
       %func = OpTypeFunction %void
      %_0_0f = OpConstant %float 0.0
   %v4floatc = OpConstantComposite %v4float %_0_0f %_0_0f %_0_0f %_0_0f
    %v2float = OpTypeVector %float 2
   %v2floatc = OpConstantComposite %v2float %_0_0f %_0_0f
      %_0_1f = OpConstant %float 0.1
  %v2floatc2 = OpConstantComposite %v2float %_0_1f %_0_1f
      %_0_2f = OpConstant %float 0.2
  %v2floatc3 = OpConstantComposite %v2float %_0_2f %_0_2f
%_0_30000001f = OpConstant %float 0.30000001
  %v2floatc4 = OpConstantComposite %v2float %_0_30000001f %_0_30000001f
%ptr_Function_v4float = OpTypePointer Function %v4float
       %main = OpFunction %void None %func
         %_2 = OpLabel
          %f = OpVariable %ptr_Function_v4float Function
               OpStore %builtinPosition %v4floatc
         %_3 = OpLoad %sampled_image_2D %s0
         %_4 = OpImageSampleImplicitLod %v4float %_3 %v2floatc
         %_5 = OpLoad %sampled_image_2D %s1
         %_6 = OpImageSampleImplicitLod %v4float %_5 %v2floatc2
         %_7 = OpFAdd %v4float %_4 %_6
         %_8 = OpLoad %sampled_image_2D %s2
         %_9 = OpImageSampleImplicitLod %v4float %_8 %v2floatc3
        %_10 = OpFAdd %v4float %_7 %_9
        %_11 = OpLoad %sampled_image_2D %s3
        %_12 = OpImageSampleImplicitLod %v4float %_11 %v2floatc4
        %_13 = OpFAdd %v4float %_10 %_12
               OpStore %f %_13
        %_14 = OpLoad %v4float %f
               OpStore %_1 %_14
               OpReturn
               OpFunctionEnd
*/

const std::vector<uint32_t> kTextureSpvWords = {
    0x07230203, 0x00010000, 0xdeadbeef, 0x00000036, 0x00000000, 0x00020011, 0x00000001, 0x0003000e,
    0x00000000, 0x00000000, 0x0006000f, 0x00000000, 0x00000029, 0x6e69616d, 0x00000000, 0x00000002,
    0x0006000f, 0x00000004, 0x0000002a, 0x6e69616d, 0x00000000, 0x00000003, 0x00030010, 0x0000002a,
    0x00000008, 0x00030005, 0x00000004, 0x00003073, 0x00030005, 0x00000005, 0x00003173, 0x00030005,
    0x00000006, 0x00003273, 0x00030005, 0x00000007, 0x00003373, 0x00040005, 0x00000029, 0x6e69616d,
    0x00000000, 0x00040005, 0x0000002a, 0x6e69616d, 0x00000000, 0x00030005, 0x00000027, 0x00000066,
    0x00040047, 0x00000002, 0x0000000b, 0x00000000, 0x00040047, 0x00000003, 0x0000001e, 0x00000000,
    0x00040047, 0x00000005, 0x00000022, 0x00000000, 0x00040047, 0x00000005, 0x00000021, 0x00000001,
    0x00040047, 0x00000007, 0x00000022, 0x00000000, 0x00040047, 0x00000007, 0x00000021, 0x00000003,
    0x00030016, 0x00000009, 0x00000020, 0x0004002b, 0x00000009, 0x00000011, 0x00000000, 0x0004002b,
    0x00000009, 0x00000015, 0x3dcccccd, 0x0004002b, 0x00000009, 0x00000017, 0x3e4ccccd, 0x0004002b,
    0x00000009, 0x00000019, 0x3e99999a, 0x00040017, 0x0000000a, 0x00000009, 0x00000004, 0x00040020,
    0x0000000b, 0x00000003, 0x0000000a, 0x00090019, 0x0000000c, 0x00000009, 0x00000001, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x0000000d, 0x0000000c, 0x00040020,
    0x0000000e, 0x00000000, 0x0000000d, 0x00020013, 0x0000000f, 0x00030021, 0x00000010, 0x0000000f,
    0x00040017, 0x00000013, 0x00000009, 0x00000002, 0x00040017, 0x00000035, 0x00000009, 0x00000003,
    0x0007002c, 0x0000000a, 0x00000012, 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x0005002c,
    0x00000013, 0x00000014, 0x00000011, 0x00000011, 0x0005002c, 0x00000013, 0x00000016, 0x00000015,
    0x00000015, 0x0005002c, 0x00000013, 0x00000018, 0x00000017, 0x00000017, 0x0005002c, 0x00000013,
    0x0000001a, 0x00000019, 0x00000019, 0x0004003b, 0x0000000b, 0x00000002, 0x00000003, 0x0004003b,
    0x0000000b, 0x00000003, 0x00000003, 0x0004003b, 0x0000000e, 0x00000004, 0x00000000, 0x0004003b,
    0x0000000e, 0x00000005, 0x00000000, 0x0004003b, 0x0000000e, 0x00000006, 0x00000000, 0x0004003b,
    0x0000000e, 0x00000007, 0x00000000, 0x00050036, 0x0000000f, 0x00000029, 0x00000000, 0x00000010,
    0x000200f8, 0x0000002b, 0x0003003e, 0x00000002, 0x00000012, 0x000100fd, 0x00010038, 0x00050036,
    0x0000000f, 0x0000002a, 0x00000000, 0x00000010, 0x000200f8, 0x0000002c, 0x0004003d, 0x0000000d,
    0x0000002e, 0x00000004, 0x00050057, 0x0000000a, 0x0000001e, 0x0000002e, 0x00000014, 0x0004003d,
    0x0000000d, 0x00000030, 0x00000005, 0x00050057, 0x0000000a, 0x00000020, 0x00000030, 0x00000016,
    0x00050081, 0x0000000a, 0x00000021, 0x0000001e, 0x00000020, 0x0004003d, 0x0000000d, 0x00000032,
    0x00000006, 0x00050057, 0x0000000a, 0x00000023, 0x00000032, 0x00000018, 0x00050081, 0x0000000a,
    0x00000024, 0x00000021, 0x00000023, 0x0004003d, 0x0000000d, 0x00000034, 0x00000007, 0x00050057,
    0x0000000a, 0x00000026, 0x00000034, 0x0000001a, 0x00050081, 0x0000000a, 0x00000027, 0x00000024,
    0x00000026, 0x0003003e, 0x00000003, 0x00000027, 0x000100fd, 0x00010038,
};

/*
               OpCapability Shader
               OpMemoryModel Logical Simple
               OpEntryPoint CombinedVF %main "main" %uv %fragColor
               OpName %tex0 "tex0"
               OpName %tex1 "tex1"
               OpName %uv "uv"
               OpName %fragColor "fragColor"
               OpName %main "main"
               OpDecorate %tex0 Binding 1
               OpDecorate %tex0 DescriptorSet 0
               OpDecorate %tex1 Binding 2
               OpDecorate %tex1 DescriptorSet 1
      %float = OpTypeFloat 32
   %image_2D = OpTypeImage %float 2D 0 0 0 1 Unknown
%sampled_image_2D = OpTypeSampledImage %image_2D
%ptr_UniformConstant_sampled_image_2D = OpTypePointer UniformConstant %sampled_image_2D
       %tex0 = OpVariable %ptr_UniformConstant_sampled_image_2D UniformConstant
       %tex1 = OpVariable %ptr_UniformConstant_sampled_image_2D UniformConstant
    %v2float = OpTypeVector %float 2
%ptr_Input_v2float = OpTypePointer Input %v2float
         %uv = OpVariable %ptr_Input_v2float Input
    %v4float = OpTypeVector %float 4
%ptr_Output_v4float = OpTypePointer Output %v4float
  %fragColor = OpVariable %ptr_Output_v4float Output
       %void = OpTypeVoid
       %func = OpTypeFunction %void
       %main = OpFunction %void None %func
         %_1 = OpLabel
         %_2 = OpLoad %sampled_image_2D %tex0
         %_3 = OpLoad %v2float %uv
         %_4 = OpImageSampleImplicitLod %v4float %_2 %_3
         %_5 = OpLoad %sampled_image_2D %tex1
         %_6 = OpLoad %v2float %uv
         %_7 = OpImageSampleImplicitLod %v4float %_5 %_6
         %_8 = OpFAdd %v4float %_4 %_7
               OpStore %fragColor %_8
               OpReturn
               OpFunctionEnd
*/

const std::vector<uint32_t> kTextureWithDescriptorSetSpvWords = {
    0x07230203, 0x00010000, 0xdeadbeef, 0x00000025, 0x00000000, 0x00020011, 0x00000001, 0x0003000e,
    0x00000000, 0x00000000, 0x0007000f, 0x00000000, 0x00000018, 0x6e69616d, 0x00000000, 0x00000002,
    0x0000001e, 0x0007000f, 0x00000004, 0x00000019, 0x6e69616d, 0x00000000, 0x00000003, 0x0000001f,
    0x00030010, 0x00000019, 0x00000008, 0x00040005, 0x00000004, 0x30786574, 0x00000000, 0x00040005,
    0x00000005, 0x31786574, 0x00000000, 0x00030005, 0x00000002, 0x00007675, 0x00050005, 0x00000003,
    0x67617266, 0x6f6c6f43, 0x00000072, 0x00040005, 0x00000018, 0x6e69616d, 0x00000000, 0x00040005,
    0x00000019, 0x6e69616d, 0x00000000, 0x00040047, 0x00000004, 0x00000022, 0x00000000, 0x00040047,
    0x00000004, 0x00000021, 0x00000001, 0x00040047, 0x00000005, 0x00000022, 0x00000001, 0x00040047,
    0x00000005, 0x00000021, 0x00000002, 0x00040047, 0x00000002, 0x0000001e, 0x00000000, 0x00040047,
    0x00000003, 0x0000001e, 0x00000000, 0x00040047, 0x0000001e, 0x0000001e, 0x00000000, 0x00040047,
    0x0000001f, 0x0000001e, 0x00000000, 0x00030016, 0x00000006, 0x00000020, 0x00090019, 0x00000007,
    0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b,
    0x00000008, 0x00000007, 0x00040020, 0x00000009, 0x00000000, 0x00000008, 0x00040017, 0x0000000a,
    0x00000006, 0x00000002, 0x00040020, 0x0000000b, 0x00000001, 0x0000000a, 0x00040017, 0x0000000c,
    0x00000006, 0x00000004, 0x00040020, 0x0000000d, 0x00000003, 0x0000000c, 0x00020013, 0x0000000e,
    0x00030021, 0x0000000f, 0x0000000e, 0x00040020, 0x00000023, 0x00000003, 0x0000000a, 0x00040017,
    0x00000024, 0x00000006, 0x00000003, 0x0004003b, 0x00000009, 0x00000004, 0x00000000, 0x0004003b,
    0x00000009, 0x00000005, 0x00000000, 0x0004003b, 0x0000000b, 0x00000002, 0x00000001, 0x0004003b,
    0x0000000d, 0x00000003, 0x00000003, 0x0004003b, 0x00000023, 0x0000001e, 0x00000003, 0x0004003b,
    0x0000000b, 0x0000001f, 0x00000001, 0x00050036, 0x0000000e, 0x00000018, 0x00000000, 0x0000000f,
    0x000200f8, 0x0000001a, 0x0004003d, 0x0000000a, 0x00000012, 0x00000002, 0x0003003e, 0x0000001e,
    0x00000012, 0x000100fd, 0x00010038, 0x00050036, 0x0000000e, 0x00000019, 0x00000000, 0x0000000f,
    0x000200f8, 0x0000001b, 0x0004003d, 0x00000008, 0x0000001d, 0x00000004, 0x0004003d, 0x0000000a,
    0x00000020, 0x0000001f, 0x00050057, 0x0000000c, 0x00000013, 0x0000001d, 0x00000020, 0x0004003d,
    0x00000008, 0x00000022, 0x00000005, 0x00050057, 0x0000000c, 0x00000016, 0x00000022, 0x00000020,
    0x00050081, 0x0000000c, 0x00000017, 0x00000013, 0x00000016, 0x0003003e, 0x00000003, 0x00000017,
    0x000100fd, 0x00010038,
};

/*
               OpCapability Shader
               OpCapability ImageQuery
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %mtl %vtx %out_FragColor
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 460
               OpName %main "main"
               OpName %textureBindless2D_u1_vf2_ "textureBindless2D(u1;vf2;"
               OpName %textureid "textureid"
               OpName %uv "uv"
               OpName %PCF3_vf3_ "PCF3(vf3;"
               OpName %uvw "uvw"
               OpName %shadow_vf4_ "shadow(vf4;"
               OpName %s "s"
               OpName %size "size"
               OpName %texShadow "texShadow"
               OpName %shadow "shadow"
               OpName %v "v"
               OpName %u "u"
               OpName %depthBias "depthBias"
               OpName %shadowSample "shadowSample"
               OpName %param "param"
               OpName %alpha "alpha"
               OpName %Material "Material"
               OpMemberName %Material 0 "ambient"
               OpMemberName %Material 1 "diffuse"
               OpMemberName %Material 2 "texAmbient"
               OpMemberName %Material 3 "texDiffuse"
               OpMemberName %Material 4 "texAlpha"
               OpMemberName %Material 5 "padding"
               OpName %mtl "mtl"
               OpName %PerVertex "PerVertex"
               OpMemberName %PerVertex 0 "normal"
               OpMemberName %PerVertex 1 "uv"
               OpMemberName %PerVertex 2 "shadowCoords"
               OpName %vtx "vtx"
               OpName %param_0 "param"
               OpName %param_1 "param"
               OpName %Ka "Ka"
               OpName %param_2 "param"
               OpName %param_3 "param"
               OpName %Kd "Kd"
               OpName %param_4 "param"
               OpName %param_5 "param"
               OpName %drawNormals "drawNormals"
               OpName %UniformsPerFrame "UniformsPerFrame"
               OpMemberName %UniformsPerFrame 0 "proj"
               OpMemberName %UniformsPerFrame 1 "view"
               OpMemberName %UniformsPerFrame 2 "light"
               OpMemberName %UniformsPerFrame 3 "bDrawNormals"
               OpMemberName %UniformsPerFrame 4 "bDebugLines"
               OpMemberName %UniformsPerFrame 5 "padding"
               OpName %PerFrame "PerFrame"
               OpMemberName %PerFrame 0 "perFrame"
               OpName %_ ""
               OpName %n "n"
               OpName %NdotL1 "NdotL1"
               OpName %NdotL2 "NdotL2"
               OpName %NdotL "NdotL"
               OpName %diffuse "diffuse"
               OpName %texSkyboxIrradiance "texSkyboxIrradiance"
               OpName %out_FragColor "out_FragColor"
               OpName %param_6 "param"
               OpDecorate %texShadow DescriptorSet 0
               OpDecorate %texShadow Binding 0
               OpDecorate %mtl Flat
               OpDecorate %mtl Location 5
               OpDecorate %vtx Location 0
               OpMemberDecorate %UniformsPerFrame 0 ColMajor
               OpMemberDecorate %UniformsPerFrame 0 Offset 0
               OpMemberDecorate %UniformsPerFrame 0 MatrixStride 16
               OpMemberDecorate %UniformsPerFrame 1 ColMajor
               OpMemberDecorate %UniformsPerFrame 1 Offset 64
               OpMemberDecorate %UniformsPerFrame 1 MatrixStride 16
               OpMemberDecorate %UniformsPerFrame 2 ColMajor
               OpMemberDecorate %UniformsPerFrame 2 Offset 128
               OpMemberDecorate %UniformsPerFrame 2 MatrixStride 16
               OpMemberDecorate %UniformsPerFrame 3 Offset 192
               OpMemberDecorate %UniformsPerFrame 4 Offset 196
               OpMemberDecorate %UniformsPerFrame 5 Offset 200
               OpMemberDecorate %PerFrame 0 Offset 0
               OpDecorate %PerFrame Block
               OpDecorate %_ DescriptorSet 1
               OpDecorate %_ Binding 0
               OpDecorate %texSkyboxIrradiance DescriptorSet 0
               OpDecorate %texSkyboxIrradiance Binding 4
               OpDecorate %out_FragColor Location 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
%_ptr_Function_uint = OpTypePointer Function %uint
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
%_ptr_Function_v2float = OpTypePointer Function %v2float
    %v4float = OpTypeVector %float 4
         %12 = OpTypeFunction %v4float %_ptr_Function_uint %_ptr_Function_v2float
    %v3float = OpTypeVector %float 3
%_ptr_Function_v3float = OpTypePointer Function %v3float
         %19 = OpTypeFunction %float %_ptr_Function_v3float
%_ptr_Function_v4float = OpTypePointer Function %v4float
         %24 = OpTypeFunction %float %_ptr_Function_v4float
    %float_1 = OpConstant %float 1
         %29 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
%_ptr_Function_float = OpTypePointer Function %float
         %34 = OpTypeImage %float 2D 1 0 0 1 Unknown
         %35 = OpTypeSampledImage %34
%_ptr_UniformConstant_35 = OpTypePointer UniformConstant %35
  %texShadow = OpVariable %_ptr_UniformConstant_35 UniformConstant
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %v2int = OpTypeVector %int 2
     %uint_0 = OpConstant %uint 0
    %float_0 = OpConstant %float 0
%_ptr_Function_int = OpTypePointer Function %int
     %int_n1 = OpConstant %int -1
      %int_1 = OpConstant %int 1
       %bool = OpTypeBool
    %float_9 = OpConstant %float 9
     %uint_3 = OpConstant %uint 3
     %uint_2 = OpConstant %uint 2
   %float_n1 = OpConstant %float -1
%float_n4_99999987en05 = OpConstant %float -4.99999987e-05
     %uint_1 = OpConstant %uint 1
%float_0_300000012 = OpConstant %float 0.300000012
   %Material = OpTypeStruct %v4float %v4float %int %int %int %int
%_ptr_Input_Material = OpTypePointer Input %Material
        %mtl = OpVariable %_ptr_Input_Material Input
      %int_4 = OpConstant %int 4
%_ptr_Input_int = OpTypePointer Input %int
  %PerVertex = OpTypeStruct %v3float %v2float %v4float
%_ptr_Input_PerVertex = OpTypePointer Input %PerVertex
        %vtx = OpVariable %_ptr_Input_PerVertex Input
%_ptr_Input_v2float = OpTypePointer Input %v2float
  %float_0_5 = OpConstant %float 0.5
%_ptr_Input_v4float = OpTypePointer Input %v4float
      %int_2 = OpConstant %int 2
      %int_3 = OpConstant %int 3
%_ptr_Function_bool = OpTypePointer Function %bool
%mat4v4float = OpTypeMatrix %v4float 4
%UniformsPerFrame = OpTypeStruct %mat4v4float %mat4v4float %mat4v4float %int %int %v2float
   %PerFrame = OpTypeStruct %UniformsPerFrame
%_ptr_Uniform_PerFrame = OpTypePointer Uniform %PerFrame
          %_ = OpVariable %_ptr_Uniform_PerFrame Uniform
%_ptr_Uniform_int = OpTypePointer Uniform %int
%_ptr_Input_v3float = OpTypePointer Input %v3float
%float_n0_577350259 = OpConstant %float -0.577350259
%float_0_577350259 = OpConstant %float 0.577350259
        %221 = OpConstantComposite %v3float %float_n0_577350259 %float_0_577350259
%float_0_577350259 %226 = OpConstantComposite %v3float %float_n0_577350259 %float_0_577350259
%float_n0_577350259 %235 = OpTypeImage %float Cube 0 0 0 1 Unknown %236 = OpTypeSampledImage %235
%_ptr_UniformConstant_236 = OpTypePointer UniformConstant %236
%texSkyboxIrradiance = OpVariable %_ptr_UniformConstant_236 UniformConstant
%float_0_959999979 = OpConstant %float 0.959999979
        %245 = OpConstantComposite %v4float %float_0_959999979 %float_0_959999979 %float_0_959999979
%float_0_959999979
%_ptr_Output_v4float = OpTypePointer Output %v4float
%out_FragColor = OpVariable %_ptr_Output_v4float Output
        %254 = OpConstantComposite %v3float %float_1 %float_1 %float_1
       %main = OpFunction %void None %3
          %5 = OpLabel
      %alpha = OpVariable %_ptr_Function_v4float Function
    %param_0 = OpVariable %_ptr_Function_uint Function
    %param_1 = OpVariable %_ptr_Function_v2float Function
         %Ka = OpVariable %_ptr_Function_v4float Function
    %param_2 = OpVariable %_ptr_Function_uint Function
    %param_3 = OpVariable %_ptr_Function_v2float Function
         %Kd = OpVariable %_ptr_Function_v4float Function
    %param_4 = OpVariable %_ptr_Function_uint Function
    %param_5 = OpVariable %_ptr_Function_v2float Function
%drawNormals = OpVariable %_ptr_Function_bool Function
          %n = OpVariable %_ptr_Function_v3float Function
     %NdotL1 = OpVariable %_ptr_Function_float Function
     %NdotL2 = OpVariable %_ptr_Function_float Function
      %NdotL = OpVariable %_ptr_Function_float Function
    %diffuse = OpVariable %_ptr_Function_v4float Function
        %250 = OpVariable %_ptr_Function_v4float Function
    %param_6 = OpVariable %_ptr_Function_v4float Function
        %143 = OpAccessChain %_ptr_Input_int %mtl %int_4
        %144 = OpLoad %int %143
        %145 = OpBitcast %uint %144
               OpStore %param_0 %145
        %152 = OpAccessChain %_ptr_Input_v2float %vtx %int_1
        %153 = OpLoad %v2float %152
               OpStore %param_1 %153
        %154 = OpFunctionCall %v4float %textureBindless2D_u1_vf2_ %param_0 %param_1
               OpStore %alpha %154
        %155 = OpAccessChain %_ptr_Input_int %mtl %int_4
        %156 = OpLoad %int %155
        %157 = OpSGreaterThan %bool %156 %int_0
               OpSelectionMerge %159 None
               OpBranchConditional %157 %158 %159
        %158 = OpLabel
        %160 = OpAccessChain %_ptr_Function_float %alpha %uint_0
        %161 = OpLoad %float %160
        %163 = OpFOrdLessThan %bool %161 %float_0_5
               OpBranch %159
        %159 = OpLabel
        %164 = OpPhi %bool %157 %5 %163 %158
               OpSelectionMerge %166 None
               OpBranchConditional %164 %165 %166
        %165 = OpLabel
               OpKill
        %166 = OpLabel
        %170 = OpAccessChain %_ptr_Input_v4float %mtl %int_0
        %171 = OpLoad %v4float %170
        %173 = OpAccessChain %_ptr_Input_int %mtl %int_2
        %174 = OpLoad %int %173
        %175 = OpBitcast %uint %174
               OpStore %param_2 %175
        %178 = OpAccessChain %_ptr_Input_v2float %vtx %int_1
        %179 = OpLoad %v2float %178
               OpStore %param_3 %179
        %180 = OpFunctionCall %v4float %textureBindless2D_u1_vf2_ %param_2 %param_3
        %181 = OpFMul %v4float %171 %180
               OpStore %Ka %181
        %183 = OpAccessChain %_ptr_Input_v4float %mtl %int_1
        %184 = OpLoad %v4float %183
        %186 = OpAccessChain %_ptr_Input_int %mtl %int_3
        %187 = OpLoad %int %186
        %188 = OpBitcast %uint %187
               OpStore %param_4 %188
        %191 = OpAccessChain %_ptr_Input_v2float %vtx %int_1
        %192 = OpLoad %v2float %191
               OpStore %param_5 %192
        %193 = OpFunctionCall %v4float %textureBindless2D_u1_vf2_ %param_4 %param_5
        %194 = OpFMul %v4float %184 %193
               OpStore %Kd %194
        %203 = OpAccessChain %_ptr_Uniform_int %_ %int_0 %int_3
        %204 = OpLoad %int %203
        %205 = OpSGreaterThan %bool %204 %int_0
               OpStore %drawNormals %205
        %206 = OpAccessChain %_ptr_Function_float %Kd %uint_3
        %207 = OpLoad %float %206
        %208 = OpFOrdLessThan %bool %207 %float_0_5
               OpSelectionMerge %210 None
               OpBranchConditional %208 %209 %210
        %209 = OpLabel
               OpKill
        %210 = OpLabel
        %214 = OpAccessChain %_ptr_Input_v3float %vtx %int_0
        %215 = OpLoad %v3float %214
        %216 = OpExtInst %v3float %1 Normalize %215
               OpStore %n %216
        %218 = OpLoad %v3float %n
        %222 = OpDot %float %218 %221
        %223 = OpExtInst %float %1 FClamp %222 %float_0 %float_1
               OpStore %NdotL1 %223
        %225 = OpLoad %v3float %n
        %227 = OpDot %float %225 %226
        %228 = OpExtInst %float %1 FClamp %227 %float_0 %float_1
               OpStore %NdotL2 %228
        %230 = OpLoad %float %NdotL1
        %231 = OpLoad %float %NdotL2
        %232 = OpFAdd %float %230 %231
        %233 = OpFMul %float %float_0_5 %232
               OpStore %NdotL %233
        %239 = OpLoad %236 %texSkyboxIrradiance
        %240 = OpLoad %v3float %n
        %241 = OpImageSampleImplicitLod %v4float %239 %240
        %242 = OpLoad %v4float %Kd
        %243 = OpFMul %v4float %241 %242
        %246 = OpFMul %v4float %243 %245
               OpStore %diffuse %246
        %249 = OpLoad %bool %drawNormals
               OpSelectionMerge %252 None
               OpBranchConditional %249 %251 %261
        %251 = OpLabel
        %253 = OpLoad %v3float %n
        %255 = OpFAdd %v3float %253 %254
        %256 = OpVectorTimesScalar %v3float %255 %float_0_5
        %257 = OpCompositeExtract %float %256 0
        %258 = OpCompositeExtract %float %256 1
        %259 = OpCompositeExtract %float %256 2
        %260 = OpCompositeConstruct %v4float %257 %258 %259 %float_1
               OpStore %250 %260
               OpBranch %252
        %261 = OpLabel
        %262 = OpLoad %v4float %Ka
        %263 = OpLoad %v4float %diffuse
        %265 = OpAccessChain %_ptr_Input_v4float %vtx %int_2
        %266 = OpLoad %v4float %265
               OpStore %param_6 %266
        %267 = OpFunctionCall %float %shadow_vf4_ %param_6
        %268 = OpVectorTimesScalar %v4float %263 %267
        %269 = OpFAdd %v4float %262 %268
               OpStore %250 %269
               OpBranch %252
        %252 = OpLabel
        %270 = OpLoad %v4float %250
               OpStore %out_FragColor %270
               OpReturn
               OpFunctionEnd
%textureBindless2D_u1_vf2_ = OpFunction %v4float None %12
  %textureid = OpFunctionParameter %_ptr_Function_uint
         %uv = OpFunctionParameter %_ptr_Function_v2float
         %16 = OpLabel
               OpReturnValue %29
               OpFunctionEnd
  %PCF3_vf3_ = OpFunction %float None %19
        %uvw = OpFunctionParameter %_ptr_Function_v3float
         %22 = OpLabel
       %size = OpVariable %_ptr_Function_float Function
     %shadow = OpVariable %_ptr_Function_float Function
          %v = OpVariable %_ptr_Function_int Function
          %u = OpVariable %_ptr_Function_int Function
         %38 = OpLoad %35 %texShadow
         %41 = OpImage %34 %38
         %43 = OpImageQuerySizeLod %v2int %41 %int_0
         %45 = OpCompositeExtract %int %43 0
         %46 = OpConvertSToF %float %45
         %47 = OpFDiv %float %float_1 %46
               OpStore %size %47
               OpStore %shadow %float_0
               OpStore %v %int_n1
               OpBranch %53
         %53 = OpLabel
               OpLoopMerge %55 %56 None
               OpBranch %57
         %57 = OpLabel
         %58 = OpLoad %int %v
         %61 = OpSLessThanEqual %bool %58 %int_1
               OpBranchConditional %61 %54 %55
         %54 = OpLabel
               OpStore %u %int_n1
               OpBranch %63
         %63 = OpLabel
               OpLoopMerge %65 %66 None
               OpBranch %67
         %67 = OpLabel
         %68 = OpLoad %int %u
         %69 = OpSLessThanEqual %bool %68 %int_1
               OpBranchConditional %69 %64 %65
         %64 = OpLabel
         %70 = OpLoad %35 %texShadow
         %71 = OpLoad %v3float %uvw
         %72 = OpLoad %float %size
         %73 = OpLoad %int %u
         %74 = OpConvertSToF %float %73
         %75 = OpLoad %int %v
         %76 = OpConvertSToF %float %75
         %77 = OpCompositeConstruct %v3float %74 %76 %float_0
         %78 = OpVectorTimesScalar %v3float %77 %72
         %79 = OpFAdd %v3float %71 %78
         %80 = OpCompositeExtract %float %79 2
         %81 = OpImageSampleDrefImplicitLod %float %70 %79 %80
         %82 = OpLoad %float %shadow
         %83 = OpFAdd %float %82 %81
               OpStore %shadow %83
               OpBranch %66
         %66 = OpLabel
         %84 = OpLoad %int %u
         %85 = OpIAdd %int %84 %int_1
               OpStore %u %85
               OpBranch %63
         %65 = OpLabel
               OpBranch %56
         %56 = OpLabel
         %86 = OpLoad %int %v
         %87 = OpIAdd %int %86 %int_1
               OpStore %v %87
               OpBranch %53
         %55 = OpLabel
         %88 = OpLoad %float %shadow
         %90 = OpFDiv %float %88 %float_9
               OpReturnValue %90
               OpFunctionEnd
%shadow_vf4_ = OpFunction %float None %24
          %s = OpFunctionParameter %_ptr_Function_v4float
         %27 = OpLabel
  %depthBias = OpVariable %_ptr_Function_float Function
%shadowSample = OpVariable %_ptr_Function_float Function
      %param = OpVariable %_ptr_Function_v3float Function
         %93 = OpLoad %v4float %s
         %95 = OpAccessChain %_ptr_Function_float %s %uint_3
         %96 = OpLoad %float %95
         %97 = OpCompositeConstruct %v4float %96 %96 %96 %96
         %98 = OpFDiv %v4float %93 %97
               OpStore %s %98
        %100 = OpAccessChain %_ptr_Function_float %s %uint_2
        %101 = OpLoad %float %100
        %103 = OpFOrdGreaterThan %bool %101 %float_n1
               OpSelectionMerge %105 None
               OpBranchConditional %103 %104 %105
        %104 = OpLabel
        %106 = OpAccessChain %_ptr_Function_float %s %uint_2
        %107 = OpLoad %float %106
        %108 = OpFOrdLessThan %bool %107 %float_1
               OpBranch %105
        %105 = OpLabel
        %109 = OpPhi %bool %103 %27 %108 %104
               OpSelectionMerge %111 None
               OpBranchConditional %109 %110 %111
        %110 = OpLabel
               OpStore %depthBias %float_n4_99999987en05
        %115 = OpAccessChain %_ptr_Function_float %s %uint_1
        %116 = OpLoad %float %115
        %117 = OpFSub %float %float_1 %116
        %118 = OpAccessChain %_ptr_Function_float %s %uint_1
               OpStore %118 %117
        %120 = OpAccessChain %_ptr_Function_float %s %uint_0
        %121 = OpLoad %float %120
        %122 = OpAccessChain %_ptr_Function_float %s %uint_1
        %123 = OpLoad %float %122
        %124 = OpAccessChain %_ptr_Function_float %s %uint_2
        %125 = OpLoad %float %124
        %126 = OpLoad %float %depthBias
        %127 = OpFAdd %float %125 %126
        %128 = OpCompositeConstruct %v3float %121 %123 %127
               OpStore %param %128
        %130 = OpFunctionCall %float %PCF3_vf3_ %param
               OpStore %shadowSample %130
        %132 = OpLoad %float %shadowSample
        %133 = OpExtInst %float %1 FMix %float_0_300000012 %float_1 %132
               OpReturnValue %133
        %111 = OpLabel
               OpReturnValue %float_1
               OpFunctionEnd
*/

const std::vector<uint32_t> kTinyMeshFragmentShader = {
    0x07230203, 0x00010000, 0xdeadbeef, 0x00000106, 0x00000000, 0x00020011, 0x00000001, 0x00020011,
    0x00000032, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e,
    0x00000000, 0x00000001, 0x0008000f, 0x00000004, 0x00000002, 0x6e69616d, 0x00000000, 0x00000003,
    0x00000004, 0x00000005, 0x00030010, 0x00000002, 0x00000007, 0x00030003, 0x00000002, 0x000001cc,
    0x00040005, 0x00000002, 0x6e69616d, 0x00000000, 0x00090005, 0x00000006, 0x74786574, 0x42657275,
    0x6c646e69, 0x32737365, 0x31752844, 0x3266763b, 0x0000003b, 0x00050005, 0x00000007, 0x74786574,
    0x69657275, 0x00000064, 0x00030005, 0x00000008, 0x00007675, 0x00050005, 0x00000009, 0x33464350,
    0x33667628, 0x0000003b, 0x00030005, 0x0000000a, 0x00777675, 0x00050005, 0x0000000b, 0x64616873,
    0x7628776f, 0x003b3466, 0x00030005, 0x0000000c, 0x00000073, 0x00040005, 0x0000000d, 0x657a6973,
    0x00000000, 0x00050005, 0x0000000e, 0x53786574, 0x6f646168, 0x00000077, 0x00040005, 0x0000000f,
    0x64616873, 0x0000776f, 0x00030005, 0x00000010, 0x00000076, 0x00030005, 0x00000011, 0x00000075,
    0x00050005, 0x00000012, 0x74706564, 0x61694268, 0x00000073, 0x00060005, 0x00000013, 0x64616873,
    0x6153776f, 0x656c706d, 0x00000000, 0x00040005, 0x00000014, 0x61726170, 0x0000006d, 0x00040005,
    0x00000015, 0x68706c61, 0x00000061, 0x00050005, 0x00000016, 0x6574614d, 0x6c616972, 0x00000000,
    0x00050006, 0x00000016, 0x00000000, 0x69626d61, 0x00746e65, 0x00050006, 0x00000016, 0x00000001,
    0x66666964, 0x00657375, 0x00060006, 0x00000016, 0x00000002, 0x41786574, 0x6569626d, 0x0000746e,
    0x00060006, 0x00000016, 0x00000003, 0x44786574, 0x75666669, 0x00006573, 0x00060006, 0x00000016,
    0x00000004, 0x41786574, 0x6168706c, 0x00000000, 0x00050006, 0x00000016, 0x00000005, 0x64646170,
    0x00676e69, 0x00030005, 0x00000003, 0x006c746d, 0x00050005, 0x00000017, 0x56726550, 0x65747265,
    0x00000078, 0x00050006, 0x00000017, 0x00000000, 0x6d726f6e, 0x00006c61, 0x00040006, 0x00000017,
    0x00000001, 0x00007675, 0x00070006, 0x00000017, 0x00000002, 0x64616873, 0x6f43776f, 0x7364726f,
    0x00000000, 0x00030005, 0x00000004, 0x00787476, 0x00040005, 0x00000018, 0x61726170, 0x0000006d,
    0x00040005, 0x00000019, 0x61726170, 0x0000006d, 0x00030005, 0x0000001a, 0x0000614b, 0x00040005,
    0x0000001b, 0x61726170, 0x0000006d, 0x00040005, 0x0000001c, 0x61726170, 0x0000006d, 0x00030005,
    0x0000001d, 0x0000644b, 0x00040005, 0x0000001e, 0x61726170, 0x0000006d, 0x00040005, 0x0000001f,
    0x61726170, 0x0000006d, 0x00050005, 0x00000020, 0x77617264, 0x6d726f4e, 0x00736c61, 0x00070005,
    0x00000021, 0x66696e55, 0x736d726f, 0x46726550, 0x656d6172, 0x00000000, 0x00050006, 0x00000021,
    0x00000000, 0x6a6f7270, 0x00000000, 0x00050006, 0x00000021, 0x00000001, 0x77656976, 0x00000000,
    0x00050006, 0x00000021, 0x00000002, 0x6867696c, 0x00000074, 0x00070006, 0x00000021, 0x00000003,
    0x61724462, 0x726f4e77, 0x736c616d, 0x00000000, 0x00060006, 0x00000021, 0x00000004, 0x62654462,
    0x694c6775, 0x0073656e, 0x00050006, 0x00000021, 0x00000005, 0x64646170, 0x00676e69, 0x00050005,
    0x00000022, 0x46726550, 0x656d6172, 0x00000000, 0x00060006, 0x00000022, 0x00000000, 0x46726570,
    0x656d6172, 0x00000000, 0x00030005, 0x00000023, 0x00000000, 0x00030005, 0x00000024, 0x0000006e,
    0x00040005, 0x00000025, 0x746f644e, 0x0000314c, 0x00040005, 0x00000026, 0x746f644e, 0x0000324c,
    0x00040005, 0x00000027, 0x746f644e, 0x0000004c, 0x00040005, 0x00000028, 0x66666964, 0x00657375,
    0x00070005, 0x00000029, 0x53786574, 0x6f62796b, 0x72724978, 0x61696461, 0x0065636e, 0x00060005,
    0x00000005, 0x5f74756f, 0x67617246, 0x6f6c6f43, 0x00000072, 0x00040005, 0x0000002a, 0x61726170,
    0x0000006d, 0x00040047, 0x0000000e, 0x00000022, 0x00000000, 0x00040047, 0x0000000e, 0x00000021,
    0x00000000, 0x00030047, 0x00000003, 0x0000000e, 0x00040047, 0x00000003, 0x0000001e, 0x00000005,
    0x00040047, 0x00000004, 0x0000001e, 0x00000000, 0x00040048, 0x00000021, 0x00000000, 0x00000005,
    0x00050048, 0x00000021, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x00000021, 0x00000000,
    0x00000007, 0x00000010, 0x00040048, 0x00000021, 0x00000001, 0x00000005, 0x00050048, 0x00000021,
    0x00000001, 0x00000023, 0x00000040, 0x00050048, 0x00000021, 0x00000001, 0x00000007, 0x00000010,
    0x00040048, 0x00000021, 0x00000002, 0x00000005, 0x00050048, 0x00000021, 0x00000002, 0x00000023,
    0x00000080, 0x00050048, 0x00000021, 0x00000002, 0x00000007, 0x00000010, 0x00050048, 0x00000021,
    0x00000003, 0x00000023, 0x000000c0, 0x00050048, 0x00000021, 0x00000004, 0x00000023, 0x000000c4,
    0x00050048, 0x00000021, 0x00000005, 0x00000023, 0x000000c8, 0x00050048, 0x00000022, 0x00000000,
    0x00000023, 0x00000000, 0x00030047, 0x00000022, 0x00000002, 0x00040047, 0x00000023, 0x00000022,
    0x00000001, 0x00040047, 0x00000023, 0x00000021, 0x00000000, 0x00040047, 0x00000029, 0x00000022,
    0x00000000, 0x00040047, 0x00000029, 0x00000021, 0x00000004, 0x00040047, 0x00000005, 0x0000001e,
    0x00000000, 0x00020013, 0x0000002b, 0x00030021, 0x0000002c, 0x0000002b, 0x00040015, 0x0000002d,
    0x00000020, 0x00000000, 0x00040020, 0x0000002e, 0x00000007, 0x0000002d, 0x00030016, 0x0000002f,
    0x00000020, 0x00040017, 0x00000030, 0x0000002f, 0x00000002, 0x00040020, 0x00000031, 0x00000007,
    0x00000030, 0x00040017, 0x00000032, 0x0000002f, 0x00000004, 0x00050021, 0x00000033, 0x00000032,
    0x0000002e, 0x00000031, 0x00040017, 0x00000034, 0x0000002f, 0x00000003, 0x00040020, 0x00000035,
    0x00000007, 0x00000034, 0x00040021, 0x00000036, 0x0000002f, 0x00000035, 0x00040020, 0x00000037,
    0x00000007, 0x00000032, 0x00040021, 0x00000038, 0x0000002f, 0x00000037, 0x0003002b, 0x0000002f,
    0x00000039, 0x0007002c, 0x00000032, 0x0000003a, 0x00000039, 0x00000039, 0x00000039, 0x00000039,
    0x00040020, 0x0000003b, 0x00000007, 0x0000002f, 0x00090019, 0x0000003c, 0x0000002f, 0x00000001,
    0x00000001, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x0000003d, 0x0000003c,
    0x00040020, 0x0000003e, 0x00000000, 0x0000003d, 0x0004003b, 0x0000003e, 0x0000000e, 0x00000000,
    0x00040015, 0x0000003f, 0x00000020, 0x00000001, 0x0004002b, 0x0000003f, 0x00000040, 0x00000000,
    0x00040017, 0x00000041, 0x0000003f, 0x00000002, 0x0004002b, 0x0000002d, 0x00000042, 0x00000000,
    0x0003002b, 0x0000002f, 0x00000043, 0x00040020, 0x00000044, 0x00000007, 0x0000003f, 0x0004002b,
    0x0000003f, 0x00000045, 0xffffffff, 0x0004002b, 0x0000003f, 0x00000046, 0x00000001, 0x00020014,
    0x00000047, 0x0003002b, 0x0000002f, 0x00000048, 0x0004002b, 0x0000002d, 0x00000049, 0x00000003,
    0x0004002b, 0x0000002d, 0x0000004a, 0x00000002, 0x0003002b, 0x0000002f, 0x0000004b, 0x0004002b,
    0x0000002f, 0x0000004c, 0xb851b717, 0x0004002b, 0x0000002d, 0x0000004d, 0x00000001, 0x0004002b,
    0x0000002f, 0x0000004e, 0x3e99999a, 0x0008001e, 0x00000016, 0x00000032, 0x00000032, 0x0000003f,
    0x0000003f, 0x0000003f, 0x0000003f, 0x00040020, 0x0000004f, 0x00000001, 0x00000016, 0x0004003b,
    0x0000004f, 0x00000003, 0x00000001, 0x0004002b, 0x0000003f, 0x00000050, 0x00000004, 0x00040020,
    0x00000051, 0x00000001, 0x0000003f, 0x0005001e, 0x00000017, 0x00000034, 0x00000030, 0x00000032,
    0x00040020, 0x00000052, 0x00000001, 0x00000017, 0x0004003b, 0x00000052, 0x00000004, 0x00000001,
    0x00040020, 0x00000053, 0x00000001, 0x00000030, 0x0004002b, 0x0000002f, 0x00000054, 0x3f000000,
    0x00040020, 0x00000055, 0x00000001, 0x00000032, 0x0004002b, 0x0000003f, 0x00000056, 0x00000002,
    0x0004002b, 0x0000003f, 0x00000057, 0x00000003, 0x00040020, 0x00000058, 0x00000007, 0x00000047,
    0x00040018, 0x00000059, 0x00000032, 0x00000004, 0x0008001e, 0x00000021, 0x00000059, 0x00000059,
    0x00000059, 0x0000003f, 0x0000003f, 0x00000030, 0x0003001e, 0x00000022, 0x00000021, 0x00040020,
    0x0000005a, 0x00000002, 0x00000022, 0x0004003b, 0x0000005a, 0x00000023, 0x00000002, 0x00040020,
    0x0000005b, 0x00000002, 0x0000003f, 0x00040020, 0x0000005c, 0x00000001, 0x00000034, 0x0004002b,
    0x0000002f, 0x0000005d, 0xbf13cd3a, 0x0004002b, 0x0000002f, 0x0000005e, 0x3f13cd3a, 0x0006002c,
    0x00000034, 0x0000005f, 0x0000005d, 0x0000005e, 0x0000005e, 0x0006002c, 0x00000034, 0x00000060,
    0x0000005d, 0x0000005e, 0x0000005d, 0x00090019, 0x00000061, 0x0000002f, 0x00000003, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x00000062, 0x00000061, 0x00040020,
    0x00000063, 0x00000000, 0x00000062, 0x0004003b, 0x00000063, 0x00000029, 0x00000000, 0x0004002b,
    0x0000002f, 0x00000064, 0x3f75c28f, 0x0007002c, 0x00000032, 0x00000065, 0x00000064, 0x00000064,
    0x00000064, 0x00000064, 0x00040020, 0x00000066, 0x00000003, 0x00000032, 0x0004003b, 0x00000066,
    0x00000005, 0x00000003, 0x0006002c, 0x00000034, 0x00000067, 0x00000039, 0x00000039, 0x00000039,
    0x00050036, 0x0000002b, 0x00000002, 0x00000000, 0x0000002c, 0x000200f8, 0x00000068, 0x0004003b,
    0x00000037, 0x00000015, 0x00000007, 0x0004003b, 0x0000002e, 0x00000018, 0x00000007, 0x0004003b,
    0x00000031, 0x00000019, 0x00000007, 0x0004003b, 0x00000037, 0x0000001a, 0x00000007, 0x0004003b,
    0x0000002e, 0x0000001b, 0x00000007, 0x0004003b, 0x00000031, 0x0000001c, 0x00000007, 0x0004003b,
    0x00000037, 0x0000001d, 0x00000007, 0x0004003b, 0x0000002e, 0x0000001e, 0x00000007, 0x0004003b,
    0x00000031, 0x0000001f, 0x00000007, 0x0004003b, 0x00000058, 0x00000020, 0x00000007, 0x0004003b,
    0x00000035, 0x00000024, 0x00000007, 0x0004003b, 0x0000003b, 0x00000025, 0x00000007, 0x0004003b,
    0x0000003b, 0x00000026, 0x00000007, 0x0004003b, 0x0000003b, 0x00000027, 0x00000007, 0x0004003b,
    0x00000037, 0x00000028, 0x00000007, 0x0004003b, 0x00000037, 0x00000069, 0x00000007, 0x0004003b,
    0x00000037, 0x0000002a, 0x00000007, 0x00050041, 0x00000051, 0x0000006a, 0x00000003, 0x00000050,
    0x0004003d, 0x0000003f, 0x0000006b, 0x0000006a, 0x0004007c, 0x0000002d, 0x0000006c, 0x0000006b,
    0x0003003e, 0x00000018, 0x0000006c, 0x00050041, 0x00000053, 0x0000006d, 0x00000004, 0x00000046,
    0x0004003d, 0x00000030, 0x0000006e, 0x0000006d, 0x0003003e, 0x00000019, 0x0000006e, 0x00060039,
    0x00000032, 0x0000006f, 0x00000006, 0x00000018, 0x00000019, 0x0003003e, 0x00000015, 0x0000006f,
    0x00050041, 0x00000051, 0x00000070, 0x00000003, 0x00000050, 0x0004003d, 0x0000003f, 0x00000071,
    0x00000070, 0x000500ad, 0x00000047, 0x00000072, 0x00000071, 0x00000040, 0x000300f7, 0x00000073,
    0x00000000, 0x000400fa, 0x00000072, 0x00000074, 0x00000073, 0x000200f8, 0x00000074, 0x00050041,
    0x0000003b, 0x00000075, 0x00000015, 0x00000042, 0x0004003d, 0x0000002f, 0x00000076, 0x00000075,
    0x000500b8, 0x00000047, 0x00000077, 0x00000076, 0x00000054, 0x000200f9, 0x00000073, 0x000200f8,
    0x00000073, 0x000700f5, 0x00000047, 0x00000078, 0x00000072, 0x00000068, 0x00000077, 0x00000074,
    0x000300f7, 0x00000079, 0x00000000, 0x000400fa, 0x00000078, 0x0000007a, 0x00000079, 0x000200f8,
    0x0000007a, 0x000100fc, 0x000200f8, 0x00000079, 0x00050041, 0x00000055, 0x0000007b, 0x00000003,
    0x00000040, 0x0004003d, 0x00000032, 0x0000007c, 0x0000007b, 0x00050041, 0x00000051, 0x0000007d,
    0x00000003, 0x00000056, 0x0004003d, 0x0000003f, 0x0000007e, 0x0000007d, 0x0004007c, 0x0000002d,
    0x0000007f, 0x0000007e, 0x0003003e, 0x0000001b, 0x0000007f, 0x00050041, 0x00000053, 0x00000080,
    0x00000004, 0x00000046, 0x0004003d, 0x00000030, 0x00000081, 0x00000080, 0x0003003e, 0x0000001c,
    0x00000081, 0x00060039, 0x00000032, 0x00000082, 0x00000006, 0x0000001b, 0x0000001c, 0x00050085,
    0x00000032, 0x00000083, 0x0000007c, 0x00000082, 0x0003003e, 0x0000001a, 0x00000083, 0x00050041,
    0x00000055, 0x00000084, 0x00000003, 0x00000046, 0x0004003d, 0x00000032, 0x00000085, 0x00000084,
    0x00050041, 0x00000051, 0x00000086, 0x00000003, 0x00000057, 0x0004003d, 0x0000003f, 0x00000087,
    0x00000086, 0x0004007c, 0x0000002d, 0x00000088, 0x00000087, 0x0003003e, 0x0000001e, 0x00000088,
    0x00050041, 0x00000053, 0x00000089, 0x00000004, 0x00000046, 0x0004003d, 0x00000030, 0x0000008a,
    0x00000089, 0x0003003e, 0x0000001f, 0x0000008a, 0x00060039, 0x00000032, 0x0000008b, 0x00000006,
    0x0000001e, 0x0000001f, 0x00050085, 0x00000032, 0x0000008c, 0x00000085, 0x0000008b, 0x0003003e,
    0x0000001d, 0x0000008c, 0x00060041, 0x0000005b, 0x0000008d, 0x00000023, 0x00000040, 0x00000057,
    0x0004003d, 0x0000003f, 0x0000008e, 0x0000008d, 0x000500ad, 0x00000047, 0x0000008f, 0x0000008e,
    0x00000040, 0x0003003e, 0x00000020, 0x0000008f, 0x00050041, 0x0000003b, 0x00000090, 0x0000001d,
    0x00000049, 0x0004003d, 0x0000002f, 0x00000091, 0x00000090, 0x000500b8, 0x00000047, 0x00000092,
    0x00000091, 0x00000054, 0x000300f7, 0x00000093, 0x00000000, 0x000400fa, 0x00000092, 0x00000094,
    0x00000093, 0x000200f8, 0x00000094, 0x000100fc, 0x000200f8, 0x00000093, 0x00050041, 0x0000005c,
    0x00000095, 0x00000004, 0x00000040, 0x0004003d, 0x00000034, 0x00000096, 0x00000095, 0x0006000c,
    0x00000034, 0x00000097, 0x00000001, 0x00000045, 0x00000096, 0x0003003e, 0x00000024, 0x00000097,
    0x0004003d, 0x00000034, 0x00000098, 0x00000024, 0x00050094, 0x0000002f, 0x00000099, 0x00000098,
    0x0000005f, 0x0008000c, 0x0000002f, 0x0000009a, 0x00000001, 0x0000002b, 0x00000099, 0x00000043,
    0x00000039, 0x0003003e, 0x00000025, 0x0000009a, 0x0004003d, 0x00000034, 0x0000009b, 0x00000024,
    0x00050094, 0x0000002f, 0x0000009c, 0x0000009b, 0x00000060, 0x0008000c, 0x0000002f, 0x0000009d,
    0x00000001, 0x0000002b, 0x0000009c, 0x00000043, 0x00000039, 0x0003003e, 0x00000026, 0x0000009d,
    0x0004003d, 0x0000002f, 0x0000009e, 0x00000025, 0x0004003d, 0x0000002f, 0x0000009f, 0x00000026,
    0x00050081, 0x0000002f, 0x000000a0, 0x0000009e, 0x0000009f, 0x00050085, 0x0000002f, 0x000000a1,
    0x00000054, 0x000000a0, 0x0003003e, 0x00000027, 0x000000a1, 0x0004003d, 0x00000062, 0x000000a2,
    0x00000029, 0x0004003d, 0x00000034, 0x000000a3, 0x00000024, 0x00050057, 0x00000032, 0x000000a4,
    0x000000a2, 0x000000a3, 0x0004003d, 0x00000032, 0x000000a5, 0x0000001d, 0x00050085, 0x00000032,
    0x000000a6, 0x000000a4, 0x000000a5, 0x00050085, 0x00000032, 0x000000a7, 0x000000a6, 0x00000065,
    0x0003003e, 0x00000028, 0x000000a7, 0x0004003d, 0x00000047, 0x000000a8, 0x00000020, 0x000300f7,
    0x000000a9, 0x00000000, 0x000400fa, 0x000000a8, 0x000000aa, 0x000000ab, 0x000200f8, 0x000000aa,
    0x0004003d, 0x00000034, 0x000000ac, 0x00000024, 0x00050081, 0x00000034, 0x000000ad, 0x000000ac,
    0x00000067, 0x0005008e, 0x00000034, 0x000000ae, 0x000000ad, 0x00000054, 0x00050051, 0x0000002f,
    0x000000af, 0x000000ae, 0x00000000, 0x00050051, 0x0000002f, 0x000000b0, 0x000000ae, 0x00000001,
    0x00050051, 0x0000002f, 0x000000b1, 0x000000ae, 0x00000002, 0x00070050, 0x00000032, 0x000000b2,
    0x000000af, 0x000000b0, 0x000000b1, 0x00000039, 0x0003003e, 0x00000069, 0x000000b2, 0x000200f9,
    0x000000a9, 0x000200f8, 0x000000ab, 0x0004003d, 0x00000032, 0x000000b3, 0x0000001a, 0x0004003d,
    0x00000032, 0x000000b4, 0x00000028, 0x00050041, 0x00000055, 0x000000b5, 0x00000004, 0x00000056,
    0x0004003d, 0x00000032, 0x000000b6, 0x000000b5, 0x0003003e, 0x0000002a, 0x000000b6, 0x00050039,
    0x0000002f, 0x000000b7, 0x0000000b, 0x0000002a, 0x0005008e, 0x00000032, 0x000000b8, 0x000000b4,
    0x000000b7, 0x00050081, 0x00000032, 0x000000b9, 0x000000b3, 0x000000b8, 0x0003003e, 0x00000069,
    0x000000b9, 0x000200f9, 0x000000a9, 0x000200f8, 0x000000a9, 0x0004003d, 0x00000032, 0x000000ba,
    0x00000069, 0x0003003e, 0x00000005, 0x000000ba, 0x000100fd, 0x00010038, 0x00050036, 0x00000032,
    0x00000006, 0x00000000, 0x00000033, 0x00030037, 0x0000002e, 0x00000007, 0x00030037, 0x00000031,
    0x00000008, 0x000200f8, 0x000000bb, 0x000200fe, 0x0000003a, 0x00010038, 0x00050036, 0x0000002f,
    0x00000009, 0x00000000, 0x00000036, 0x00030037, 0x00000035, 0x0000000a, 0x000200f8, 0x000000bc,
    0x0004003b, 0x0000003b, 0x0000000d, 0x00000007, 0x0004003b, 0x0000003b, 0x0000000f, 0x00000007,
    0x0004003b, 0x00000044, 0x00000010, 0x00000007, 0x0004003b, 0x00000044, 0x00000011, 0x00000007,
    0x0004003d, 0x0000003d, 0x000000bd, 0x0000000e, 0x00040064, 0x0000003c, 0x000000be, 0x000000bd,
    0x00050067, 0x00000041, 0x000000bf, 0x000000be, 0x00000040, 0x00050051, 0x0000003f, 0x000000c0,
    0x000000bf, 0x00000000, 0x0004006f, 0x0000002f, 0x000000c1, 0x000000c0, 0x00050088, 0x0000002f,
    0x000000c2, 0x00000039, 0x000000c1, 0x0003003e, 0x0000000d, 0x000000c2, 0x0003003e, 0x0000000f,
    0x00000043, 0x0003003e, 0x00000010, 0x00000045, 0x000200f9, 0x000000c3, 0x000200f8, 0x000000c3,
    0x000400f6, 0x000000c4, 0x000000c5, 0x00000000, 0x000200f9, 0x000000c6, 0x000200f8, 0x000000c6,
    0x0004003d, 0x0000003f, 0x000000c7, 0x00000010, 0x000500b3, 0x00000047, 0x000000c8, 0x000000c7,
    0x00000046, 0x000400fa, 0x000000c8, 0x000000c9, 0x000000c4, 0x000200f8, 0x000000c9, 0x0003003e,
    0x00000011, 0x00000045, 0x000200f9, 0x000000ca, 0x000200f8, 0x000000ca, 0x000400f6, 0x000000cb,
    0x000000cc, 0x00000000, 0x000200f9, 0x000000cd, 0x000200f8, 0x000000cd, 0x0004003d, 0x0000003f,
    0x000000ce, 0x00000011, 0x000500b3, 0x00000047, 0x000000cf, 0x000000ce, 0x00000046, 0x000400fa,
    0x000000cf, 0x000000d0, 0x000000cb, 0x000200f8, 0x000000d0, 0x0004003d, 0x0000003d, 0x000000d1,
    0x0000000e, 0x0004003d, 0x00000034, 0x000000d2, 0x0000000a, 0x0004003d, 0x0000002f, 0x000000d3,
    0x0000000d, 0x0004003d, 0x0000003f, 0x000000d4, 0x00000011, 0x0004006f, 0x0000002f, 0x000000d5,
    0x000000d4, 0x0004003d, 0x0000003f, 0x000000d6, 0x00000010, 0x0004006f, 0x0000002f, 0x000000d7,
    0x000000d6, 0x00060050, 0x00000034, 0x000000d8, 0x000000d5, 0x000000d7, 0x00000043, 0x0005008e,
    0x00000034, 0x000000d9, 0x000000d8, 0x000000d3, 0x00050081, 0x00000034, 0x000000da, 0x000000d2,
    0x000000d9, 0x00050051, 0x0000002f, 0x000000db, 0x000000da, 0x00000002, 0x00060059, 0x0000002f,
    0x000000dc, 0x000000d1, 0x000000da, 0x000000db, 0x0004003d, 0x0000002f, 0x000000dd, 0x0000000f,
    0x00050081, 0x0000002f, 0x000000de, 0x000000dd, 0x000000dc, 0x0003003e, 0x0000000f, 0x000000de,
    0x000200f9, 0x000000cc, 0x000200f8, 0x000000cc, 0x0004003d, 0x0000003f, 0x000000df, 0x00000011,
    0x00050080, 0x0000003f, 0x000000e0, 0x000000df, 0x00000046, 0x0003003e, 0x00000011, 0x000000e0,
    0x000200f9, 0x000000ca, 0x000200f8, 0x000000cb, 0x000200f9, 0x000000c5, 0x000200f8, 0x000000c5,
    0x0004003d, 0x0000003f, 0x000000e1, 0x00000010, 0x00050080, 0x0000003f, 0x000000e2, 0x000000e1,
    0x00000046, 0x0003003e, 0x00000010, 0x000000e2, 0x000200f9, 0x000000c3, 0x000200f8, 0x000000c4,
    0x0004003d, 0x0000002f, 0x000000e3, 0x0000000f, 0x00050088, 0x0000002f, 0x000000e4, 0x000000e3,
    0x00000048, 0x000200fe, 0x000000e4, 0x00010038, 0x00050036, 0x0000002f, 0x0000000b, 0x00000000,
    0x00000038, 0x00030037, 0x00000037, 0x0000000c, 0x000200f8, 0x000000e5, 0x0004003b, 0x0000003b,
    0x00000012, 0x00000007, 0x0004003b, 0x0000003b, 0x00000013, 0x00000007, 0x0004003b, 0x00000035,
    0x00000014, 0x00000007, 0x0004003d, 0x00000032, 0x000000e6, 0x0000000c, 0x00050041, 0x0000003b,
    0x000000e7, 0x0000000c, 0x00000049, 0x0004003d, 0x0000002f, 0x000000e8, 0x000000e7, 0x00070050,
    0x00000032, 0x000000e9, 0x000000e8, 0x000000e8, 0x000000e8, 0x000000e8, 0x00050088, 0x00000032,
    0x000000ea, 0x000000e6, 0x000000e9, 0x0003003e, 0x0000000c, 0x000000ea, 0x00050041, 0x0000003b,
    0x000000eb, 0x0000000c, 0x0000004a, 0x0004003d, 0x0000002f, 0x000000ec, 0x000000eb, 0x000500ba,
    0x00000047, 0x000000ed, 0x000000ec, 0x0000004b, 0x000300f7, 0x000000ee, 0x00000000, 0x000400fa,
    0x000000ed, 0x000000ef, 0x000000ee, 0x000200f8, 0x000000ef, 0x00050041, 0x0000003b, 0x000000f0,
    0x0000000c, 0x0000004a, 0x0004003d, 0x0000002f, 0x000000f1, 0x000000f0, 0x000500b8, 0x00000047,
    0x000000f2, 0x000000f1, 0x00000039, 0x000200f9, 0x000000ee, 0x000200f8, 0x000000ee, 0x000700f5,
    0x00000047, 0x000000f3, 0x000000ed, 0x000000e5, 0x000000f2, 0x000000ef, 0x000300f7, 0x000000f4,
    0x00000000, 0x000400fa, 0x000000f3, 0x000000f5, 0x000000f4, 0x000200f8, 0x000000f5, 0x0003003e,
    0x00000012, 0x0000004c, 0x00050041, 0x0000003b, 0x000000f6, 0x0000000c, 0x0000004d, 0x0004003d,
    0x0000002f, 0x000000f7, 0x000000f6, 0x00050083, 0x0000002f, 0x000000f8, 0x00000039, 0x000000f7,
    0x00050041, 0x0000003b, 0x000000f9, 0x0000000c, 0x0000004d, 0x0003003e, 0x000000f9, 0x000000f8,
    0x00050041, 0x0000003b, 0x000000fa, 0x0000000c, 0x00000042, 0x0004003d, 0x0000002f, 0x000000fb,
    0x000000fa, 0x00050041, 0x0000003b, 0x000000fc, 0x0000000c, 0x0000004d, 0x0004003d, 0x0000002f,
    0x000000fd, 0x000000fc, 0x00050041, 0x0000003b, 0x000000fe, 0x0000000c, 0x0000004a, 0x0004003d,
    0x0000002f, 0x000000ff, 0x000000fe, 0x0004003d, 0x0000002f, 0x00000100, 0x00000012, 0x00050081,
    0x0000002f, 0x00000101, 0x000000ff, 0x00000100, 0x00060050, 0x00000034, 0x00000102, 0x000000fb,
    0x000000fd, 0x00000101, 0x0003003e, 0x00000014, 0x00000102, 0x00050039, 0x0000002f, 0x00000103,
    0x00000009, 0x00000014, 0x0003003e, 0x00000013, 0x00000103, 0x0004003d, 0x0000002f, 0x00000104,
    0x00000013, 0x0008000c, 0x0000002f, 0x00000105, 0x00000001, 0x0000002e, 0x0000004e, 0x00000039,
    0x00000104, 0x000200fe, 0x00000105, 0x000200f8, 0x000000f4, 0x000200fe, 0x00000039, 0x00010038};

} // namespace

TEST(SpvReflectionTest, UniformBufferTest) {
  using namespace vulkan::util;
  SpvModuleInfo spvModuleInfo = getReflectionData(kUniformBufferSpvWords.data(),
                                                  kUniformBufferSpvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.buffers.size(), 2);
  EXPECT_EQ(spvModuleInfo.buffers[0].bindingLocation, 0);
  EXPECT_EQ(spvModuleInfo.buffers[1].bindingLocation, 3);
  EXPECT_EQ(spvModuleInfo.buffers[0].isStorage, false);
  EXPECT_EQ(spvModuleInfo.buffers[1].isStorage, false);
}

TEST(SpvReflectionTest, TextureTest) {
  using namespace vulkan::util;
  SpvModuleInfo spvModuleInfo =
      getReflectionData(kTextureSpvWords.data(), kTextureSpvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.buffers.size(), 0);
  EXPECT_EQ(spvModuleInfo.textures.size(), 4);
  EXPECT_EQ(spvModuleInfo.textures[0].bindingLocation, kNoBindingLocation);
  EXPECT_EQ(spvModuleInfo.textures[0].descriptorSet, kNoDescriptorSet);
  EXPECT_EQ(spvModuleInfo.textures[1].bindingLocation, 1);
  EXPECT_EQ(spvModuleInfo.textures[1].descriptorSet, 0);
  EXPECT_EQ(spvModuleInfo.textures[2].bindingLocation, kNoBindingLocation);
  EXPECT_EQ(spvModuleInfo.textures[2].descriptorSet, kNoBindingLocation);
  EXPECT_EQ(spvModuleInfo.textures[3].bindingLocation, 3);
  EXPECT_EQ(spvModuleInfo.textures[3].descriptorSet, 0);

  EXPECT_EQ(spvModuleInfo.textures[0].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[1].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[2].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[3].type, TextureType::TwoD);
}

TEST(SpvReflectionTest, TextureDescriptorSetTest) {
  using namespace vulkan::util;
  SpvModuleInfo spvModuleInfo =
      getReflectionData(kTextureWithDescriptorSetSpvWords.data(),
                        kTextureWithDescriptorSetSpvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.buffers.size(), 0);
  EXPECT_EQ(spvModuleInfo.textures.size(), 2);
  EXPECT_EQ(spvModuleInfo.textures[0].bindingLocation, 1);
  EXPECT_EQ(spvModuleInfo.textures[0].descriptorSet, 0);
  EXPECT_EQ(spvModuleInfo.textures[1].bindingLocation, 2);
  EXPECT_EQ(spvModuleInfo.textures[1].descriptorSet, 1);

  EXPECT_EQ(spvModuleInfo.textures[0].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[1].type, TextureType::TwoD);
}

TEST(SpvReflectionTest, TinyMeshFragmentShaderTest) {
  using namespace vulkan::util;
  SpvModuleInfo spvModuleInfo = getReflectionData(
      kTinyMeshFragmentShader.data(), kTinyMeshFragmentShader.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.buffers.size(), 1);
  EXPECT_EQ(spvModuleInfo.textures.size(), 2);
  EXPECT_EQ(spvModuleInfo.buffers[0].bindingLocation, 0);
  EXPECT_EQ(spvModuleInfo.buffers[0].descriptorSet, 1);
  EXPECT_EQ(spvModuleInfo.buffers[0].isStorage, false);
  EXPECT_EQ(spvModuleInfo.textures[0].bindingLocation, 0);
  EXPECT_EQ(spvModuleInfo.textures[0].descriptorSet, 0);
  EXPECT_EQ(spvModuleInfo.textures[1].bindingLocation, 4);
  EXPECT_EQ(spvModuleInfo.textures[1].descriptorSet, 0);
}

} // namespace igl::tests
