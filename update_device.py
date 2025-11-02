#!/usr/bin/env python3
import re

# Read the file
with open('src/igl/d3d12/Device.cpp', 'r') as f:
    content = f.read()

# Find and replace the root parameter section
old_params = '''  // Parameter 0: Root CBV for b0 (UniformsPerFrame)
  rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[0].Descriptor.ShaderRegister = 0;  // b0
  rootParams[0].Descriptor.RegisterSpace = 0;
  rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 1: Root CBV for b1 (UniformsPerObject)
  rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[1].Descriptor.ShaderRegister = 1;  // b1
  rootParams[1].Descriptor.RegisterSpace = 0;
  rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 2: Descriptor table for SRVs (textures)
  rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;
  rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // Parameter 3: Descriptor table for Samplers
  rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[3].DescriptorTable.pDescriptorRanges = &samplerRange;
  rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;'''

new_params = '''  // Parameter 0: Root Constants for b2 (Push Constants)
  // Max 64 bytes = 16 DWORDs to match Vulkan push constant limits
  rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  rootParams[0].Constants.ShaderRegister = 2;  // b2 (b0/b1 reserved for uniform buffers)
  rootParams[0].Constants.RegisterSpace = 0;
  rootParams[0].Constants.Num32BitValues = 16;  // 16 DWORDs = 64 bytes
  rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 1: Root CBV for b0 (UniformsPerFrame)
  rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[1].Descriptor.ShaderRegister = 0;  // b0
  rootParams[1].Descriptor.RegisterSpace = 0;
  rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 2: Root CBV for b1 (UniformsPerObject)
  rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[2].Descriptor.ShaderRegister = 1;  // b1
  rootParams[2].Descriptor.RegisterSpace = 0;
  rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 3: Descriptor table for SRVs (textures)
  rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[3].DescriptorTable.pDescriptorRanges = &srvRange;
  rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // Parameter 4: Descriptor table for Samplers
  rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[4].DescriptorTable.pDescriptorRanges = &samplerRange;
  rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;'''

content = content.replace(old_params, new_params)

# Update NumParameters from 4 to 5
content = re.sub(r'rootSigDesc\.NumParameters = 4;', 'rootSigDesc.NumParameters = 5;', content)

# Update comment
content = content.replace(
    '  // - Root parameter 0: CBV for uniform buffer b0 (UniformsPerFrame)\n  // - Root parameter 1: CBV for uniform buffer b1 (UniformsPerObject)\n  // - Root parameter 2: Descriptor table with 2 SRVs for textures t0-t1\n  // - Root parameter 3: Descriptor table with 2 Samplers for s0-s1',
    '  // - Root parameter 0: Root Constants for b2 (Push Constants) - 16 DWORDs = 64 bytes max\n  // - Root parameter 1: CBV for uniform buffer b0 (UniformsPerFrame)\n  // - Root parameter 2: CBV for uniform buffer b1 (UniformsPerObject)\n  // - Root parameter 3: Descriptor table with SRVs for textures t0-tN (unbounded)\n  // - Root parameter 4: Descriptor table with Samplers for s0-sN (unbounded)'
)

content = content.replace(
    '"  Creating root signature with CBVs (b0,b1)/SRVs/Samplers\\n"',
    '"  Creating root signature with Push Constants (b2)/CBVs (b0,b1)/SRVs/Samplers\\n"'
)

# Write back
with open('src/igl/d3d12/Device.cpp', 'w') as f:
    f.write(content)

print("Device.cpp updated successfully!")
