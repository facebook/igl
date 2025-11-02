#!/usr/bin/env python3

# Read the file
with open('src/igl/tests/RenderCommandEncoder.cpp', 'r') as f:
    content = f.read()

# Replace the skip check
content = content.replace(
    '''  if (iglDev_->getBackendType() != igl::BackendType::Vulkan) {
    GTEST_SKIP() << "Push constants are only supported in Vulkan";
    return;
  }''',
    '''  if (iglDev_->getBackendType() != igl::BackendType::Vulkan &&
      iglDev_->getBackendType() != igl::BackendType::D3D12) {
    GTEST_SKIP() << "Push constants are only supported in Vulkan and D3D12";
    return;
  }'''
)

# Replace shader creation
old_shader_create = '''  // Create new shader stages with push constant shaders
  std::unique_ptr<IShaderStages> pushConstantStages;
  igl::tests::util::createShaderStages(iglDev_,
                                       data::shader::kVulkanPushConstantVertShader,
                                       igl::tests::data::shader::kShaderFunc,
                                       data::shader::kVulkanPushConstantFragShader,
                                       igl::tests::data::shader::kShaderFunc,
                                       pushConstantStages);
  ASSERT_TRUE(pushConstantStages);
  shaderStages_ = std::move(pushConstantStages);'''

new_shader_create = '''  // Create new shader stages with push constant shaders
  std::unique_ptr<IShaderStages> pushConstantStages;
  if (iglDev_->getBackendType() == igl::BackendType::D3D12) {
    igl::tests::util::createShaderStages(iglDev_,
                                         data::shader::kD3D12PushConstantVertShader,
                                         std::string("main"),
                                         data::shader::kD3D12PushConstantFragShader,
                                         std::string("main"),
                                         pushConstantStages);
  } else {
    igl::tests::util::createShaderStages(iglDev_,
                                         data::shader::kVulkanPushConstantVertShader,
                                         igl::tests::data::shader::kShaderFunc,
                                         data::shader::kVulkanPushConstantFragShader,
                                         igl::tests::data::shader::kShaderFunc,
                                         pushConstantStages);
  }
  ASSERT_TRUE(pushConstantStages);
  shaderStages_ = std::move(pushConstantStages);'''

content = content.replace(old_shader_create, new_shader_create)

# Write back
with open('src/igl/tests/RenderCommandEncoder.cpp', 'w') as f:
    f.write(content)

print("RenderCommandEncoder.cpp test updated successfully!")
