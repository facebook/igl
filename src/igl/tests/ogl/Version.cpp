/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/Common.h"

#include <igl/opengl/Macros.h>
#include <igl/opengl/Version.h>

namespace igl::tests {

class VersionOGLTest : public ::testing::Test {
 public:
  VersionOGLTest() = default;
  ~VersionOGLTest() override = default;

  // Set up VertexInputStateDesc for the different test cases
  void SetUp() override {
    setDebugBreakEnabled(false);
  }

  void TearDown() override {}
};

TEST_F(VersionOGLTest, GetGLVersionEnum) {
#if IGL_OPENGL_ES
  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 2.0", false), opengl::GLVersion::v2_0_ES);
  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 2.1", false), opengl::GLVersion::v2_0_ES);
  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 3.0", false), opengl::GLVersion::v3_0_ES);
  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 3.1", false), opengl::GLVersion::v3_1_ES);
  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 3.2", false), opengl::GLVersion::v3_2_ES);
  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 3.3", false), opengl::GLVersion::v3_0_ES);
  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 4.0", false), opengl::GLVersion::v2_0_ES);

  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 2.0", true), opengl::GLVersion::v2_0_ES);
  EXPECT_EQ(opengl::getGLVersion("OpenGL ES 4.0", true), opengl::GLVersion::v3_0_ES);
  EXPECT_EQ(opengl::getGLVersion(nullptr, false), opengl::GLVersion::v2_0_ES);
#else
  EXPECT_EQ(opengl::getGLVersion("1.1", false), opengl::GLVersion::v1_1);
  EXPECT_EQ(opengl::getGLVersion("2.0", false), opengl::GLVersion::v2_0);
  EXPECT_EQ(opengl::getGLVersion("2.1", false), opengl::GLVersion::v2_1);
  EXPECT_EQ(opengl::getGLVersion("2.2", false), opengl::GLVersion::v2_0);

  EXPECT_EQ(opengl::getGLVersion("3.0", false), opengl::GLVersion::v3_0);
  EXPECT_EQ(opengl::getGLVersion("3.1", false), opengl::GLVersion::v3_1);
  EXPECT_EQ(opengl::getGLVersion("3.2", false), opengl::GLVersion::v3_2);
  EXPECT_EQ(opengl::getGLVersion("3.3", false), opengl::GLVersion::v3_3);
  EXPECT_EQ(opengl::getGLVersion("3.4", false), opengl::GLVersion::v3_0);

  EXPECT_EQ(opengl::getGLVersion("4.0", false), opengl::GLVersion::v4_0);
  EXPECT_EQ(opengl::getGLVersion("4.1", false), opengl::GLVersion::v4_1);
  EXPECT_EQ(opengl::getGLVersion("4.2", false), opengl::GLVersion::v4_2);
  EXPECT_EQ(opengl::getGLVersion("4.3", false), opengl::GLVersion::v4_3);
  EXPECT_EQ(opengl::getGLVersion("4.4", false), opengl::GLVersion::v4_4);
  EXPECT_EQ(opengl::getGLVersion("4.5", false), opengl::GLVersion::v4_5);
  EXPECT_EQ(opengl::getGLVersion("4.6", false), opengl::GLVersion::v4_6);
  EXPECT_EQ(opengl::getGLVersion("4.7", false), opengl::GLVersion::v4_0);

  EXPECT_EQ(opengl::getGLVersion("5.0", false), opengl::GLVersion::v2_0);
  EXPECT_EQ(opengl::getGLVersion(nullptr, false), opengl::GLVersion::v2_0);

  EXPECT_EQ(opengl::getGLVersion("2.0", true), opengl::GLVersion::v2_0);
#endif
}

TEST_F(VersionOGLTest, GetShaderVersion) {
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v2_0_ES);
    EXPECT_EQ(version.family, ShaderFamily::GlslEs);
    EXPECT_EQ(version.majorVersion, 1);
    EXPECT_EQ(version.minorVersion, 0);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v3_0_ES);
    EXPECT_EQ(version.family, ShaderFamily::GlslEs);
    EXPECT_EQ(version.majorVersion, 3);
    EXPECT_EQ(version.minorVersion, 0);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v3_1_ES);
    EXPECT_EQ(version.family, ShaderFamily::GlslEs);
    EXPECT_EQ(version.majorVersion, 3);
    EXPECT_EQ(version.minorVersion, 10);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v3_2_ES);
    EXPECT_EQ(version.family, ShaderFamily::GlslEs);
    EXPECT_EQ(version.majorVersion, 3);
    EXPECT_EQ(version.minorVersion, 20);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v2_0);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 1);
    EXPECT_EQ(version.minorVersion, 10);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v2_1);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 1);
    EXPECT_EQ(version.minorVersion, 20);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v3_0);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 1);
    EXPECT_EQ(version.minorVersion, 30);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v3_1);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 1);
    EXPECT_EQ(version.minorVersion, 40);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v3_2);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 1);
    EXPECT_EQ(version.minorVersion, 50);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v3_3);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 3);
    EXPECT_EQ(version.minorVersion, 30);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v4_0);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 4);
    EXPECT_EQ(version.minorVersion, 0);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v4_1);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 4);
    EXPECT_EQ(version.minorVersion, 10);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v4_2);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 4);
    EXPECT_EQ(version.minorVersion, 20);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v4_3);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 4);
    EXPECT_EQ(version.minorVersion, 30);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v4_4);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 4);
    EXPECT_EQ(version.minorVersion, 40);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v4_5);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 4);
    EXPECT_EQ(version.minorVersion, 50);
  }
  {
    const auto version = opengl::getShaderVersion(opengl::GLVersion::v4_6);
    EXPECT_EQ(version.family, ShaderFamily::Glsl);
    EXPECT_EQ(version.majorVersion, 4);
    EXPECT_EQ(version.minorVersion, 60);
  }
}
} // namespace igl::tests
