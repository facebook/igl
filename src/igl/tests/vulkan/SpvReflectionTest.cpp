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

} // namespace

TEST(SpvReflectionTest, UniformBufferTest) {
  using namespace vulkan::util;
  SpvModuleInfo spvModuleInfo = getReflectionData(kUniformBufferSpvWords.data(),
                                                  kUniformBufferSpvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.uniformBuffers.size(), 2);
  EXPECT_EQ(spvModuleInfo.uniformBuffers[0].bindingLocation, 0);
  EXPECT_EQ(spvModuleInfo.uniformBuffers[1].bindingLocation, 3);
  EXPECT_EQ(spvModuleInfo.storageBuffers.size(), 0);
}

TEST(SpvReflectionTest, TextureTest) {
  using namespace vulkan::util;
  SpvModuleInfo spvModuleInfo =
      getReflectionData(kTextureSpvWords.data(), kTextureSpvWords.size() * sizeof(uint32_t));

  ASSERT_EQ(spvModuleInfo.uniformBuffers.size(), 0);
  EXPECT_EQ(spvModuleInfo.storageBuffers.size(), 0);
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

  ASSERT_EQ(spvModuleInfo.uniformBuffers.size(), 0);
  EXPECT_EQ(spvModuleInfo.storageBuffers.size(), 0);
  EXPECT_EQ(spvModuleInfo.textures.size(), 2);
  EXPECT_EQ(spvModuleInfo.textures[0].bindingLocation, 1);
  EXPECT_EQ(spvModuleInfo.textures[0].descriptorSet, 0);
  EXPECT_EQ(spvModuleInfo.textures[1].bindingLocation, 2);
  EXPECT_EQ(spvModuleInfo.textures[1].descriptorSet, 1);

  EXPECT_EQ(spvModuleInfo.textures[0].type, TextureType::TwoD);
  EXPECT_EQ(spvModuleInfo.textures[1].type, TextureType::TwoD);
}

} // namespace igl::tests
