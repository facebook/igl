/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/Common.h"
#include <IGLU/shaderCross/ShaderCross.h>
#include <IGLU/shaderCross/ShaderCrossUniformBuffer.h>

namespace igl::tests {

namespace {
[[nodiscard]] const char* getVulkanFragmentShaderSource() {
  return R"(#version 450
              precision highp float;
              precision highp sampler2D;
  
              layout(location = 0) in vec3 uvw;
              layout(location = 1) in vec3 color;
              layout(set = 0, binding = 0) uniform sampler2D inputImage;
              layout(location = 0) out vec4 fragmentColor;
  
              void main() {
                fragmentColor = texture(inputImage, uvw.xy) * vec4(color, 1.0);
              })";
}

[[nodiscard]] std::string getVertexShaderProlog(bool stereoRendering) {
  return stereoRendering ? R"(#version 450
      #extension GL_OVR_multiview2 : require
      layout(num_views = 2) in;
      precision highp float;
  
      #define VIEW_ID int(gl_ViewID_OVR)
    )"
                         : R"(#version 450
      precision highp float;
  
      #define VIEW_ID perFrame.viewId
    )";
}

[[nodiscard]] std::string getVulkanVertexShaderSource(bool stereoRendering) {
  return getVertexShaderProlog(stereoRendering) + R"(
              layout (set = 1, binding = 1, std140) uniform PerFrame {
                mat4 modelMatrix;
                mat4 viewProjectionMatrix[2];
                float scaleZ;
                int viewId;
              } perFrame;
  
              layout(location = 0) in vec3 position;
              layout(location = 1) in vec3 uvw_in;
              layout(location = 0) out vec3 uvw;
              layout(location = 1) out vec3 color;
  
              void main() {
                mat4 mvpMatrix = perFrame.viewProjectionMatrix[VIEW_ID] * perFrame.modelMatrix;
                gl_Position = mvpMatrix * vec4(position, 1.0);
                uvw = vec3(uvw_in.x, uvw_in.y, (uvw_in.z - 0.5) * perFrame.scaleZ + 0.5);
                color = vec3(1.0, 1.0, 0.0);
              })";
}
} // namespace

class ShaderCrossTest : public ::testing::Test {
 public:
  ShaderCrossTest() = default;
  ~ShaderCrossTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
  }

  void TearDown() override {}

  // Member variables
 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(ShaderCrossTest, Construction) {
  const iglu::ShaderCross shaderCross(*iglDev_);
}

TEST_F(ShaderCrossTest, EntryPointName) {
  const iglu::ShaderCross shaderCross(*iglDev_);
  if (iglDev_->getBackendType() == igl::BackendType::Metal) {
    EXPECT_EQ(shaderCross.entryPointName(igl::ShaderStage::Vertex), "main0");
  } else if (iglDev_->getBackendType() == igl::BackendType::OpenGL) {
    EXPECT_EQ(shaderCross.entryPointName(igl::ShaderStage::Vertex), "main");
  } else {
    EXPECT_EQ(shaderCross.entryPointName(igl::ShaderStage::Vertex), "");
  }
}

TEST_F(ShaderCrossTest, CrossCompile) {
  const iglu::ShaderCross shaderCross(*iglDev_);
  Result res;
  if (iglDev_->getBackendType() == igl::BackendType::Metal) {
    const auto vs = shaderCross.crossCompileFromVulkanSource(
        getVulkanVertexShaderSource(false).c_str(), igl::ShaderStage::Vertex, &res);
    EXPECT_TRUE(res.isOk());
    EXPECT_TRUE(!vs.empty());

    const auto fs = shaderCross.crossCompileFromVulkanSource(
        getVulkanFragmentShaderSource(), igl::ShaderStage::Fragment, &res);
    EXPECT_TRUE(res.isOk());
    EXPECT_TRUE(!fs.empty());

  } else if (iglDev_->getBackendType() == igl::BackendType::OpenGL) {
    const auto vs = shaderCross.crossCompileFromVulkanSource(
        getVulkanVertexShaderSource(true).c_str(), igl::ShaderStage::Vertex, &res);
    EXPECT_TRUE(res.isOk());
    EXPECT_TRUE(!vs.empty());

    const auto fs = shaderCross.crossCompileFromVulkanSource(
        getVulkanFragmentShaderSource(), igl::ShaderStage::Fragment, &res);
    EXPECT_TRUE(res.isOk());
    EXPECT_TRUE(!fs.empty());
  }
}

TEST_F(ShaderCrossTest, ShaderCrossUniformBuffer) {
  iglu::ShaderCrossUniformBuffer buffer(
      *iglDev_, "perFrame", {0, 10, {{"myUniform", 0, UniformType::Float, 1, 0, 0}}});
  EXPECT_EQ(buffer.uniformInfo.uniforms.size(), 1);
  EXPECT_EQ(buffer.uniformInfo.uniforms[0].name, "perFrame.myUniform");
}

} // namespace igl::tests
