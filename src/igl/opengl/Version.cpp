/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Version.h>

#include <cstdint>
#include <igl/Assert.h>
#include <igl/Log.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/Macros.h>

namespace igl::opengl {
namespace {
// Gets the highest version available in header files the app is compiled against.
std::pair<uint32_t, uint32_t> highestSupportedVersion() {
#if IGL_OPENGL_ES

#if defined(GL_ES_VERSION_3_2)
  return std::make_pair(3, 2);
#elif defined(GL_ES_VERSION_3_1)
  return std::make_pair(3, 1);
#elif defined(GL_ES_VERSION_3_0)
  return std::make_pair(3, 0);
#elif defined(GL_ES_VERSION_2_0)
  return std::make_pair(2, 0);
#else
#error "IGL requires at least OpenGL ES 2.0";
#endif

#else

#if defined(GL_VERSION_4_6)
  return std::make_pair(4, 6);
#elif defined(GL_VERSION_4_5)
  return std::make_pair(4, 5);
#elif defined(GL_VERSION_4_4)
  return std::make_pair(4, 4);
#elif defined(GL_VERSION_4_3)
  return std::make_pair(4, 3);
#elif defined(GL_VERSION_4_2)
  return std::make_pair(4, 2);
#elif defined(GL_VERSION_4_1)
  return std::make_pair(4, 1);
#elif defined(GL_VERSION_4_0)
  return std::make_pair(4, 0);
#elif defined(GL_VERSION_3_3)
  return std::make_pair(3, 3);
#elif defined(GL_VERSION_3_2)
  return std::make_pair(3, 2);
#elif defined(GL_VERSION_3_1)
  return std::make_pair(3, 1);
#elif defined(GL_VERSION_3_0)
  return std::make_pair(3, 0);
#elif defined(GL_VERSION_2_1)
  return std::make_pair(2, 1);
#elif defined(GL_VERSION_2_0)
  return std::make_pair(2, 0);
#else
#error "IGL requires at least OpenGL 2.0";
#endif

#endif // IGL_OPENGL_ES
}

// Constrains version to the highest version available in header files the app is compiled against.
std::pair<uint32_t, uint32_t> constrainVersion(uint32_t majorVersion, uint32_t minorVersion) {
  auto [supportedMajorVersion, supportedMinorVersion] = highestSupportedVersion();
  if (majorVersion > supportedMajorVersion ||
      (majorVersion == supportedMajorVersion && minorVersion > supportedMinorVersion)) {
    return std::make_pair(supportedMajorVersion, supportedMinorVersion);
  } else {
    return std::make_pair(majorVersion, minorVersion);
  }
}

GLVersion getGLVersionEnum(uint32_t majorVersion, uint32_t minorVersion) {
#if IGL_OPENGL_ES
  switch (majorVersion) {
  case 2:
    switch (minorVersion) {
    case 0:
      return GLVersion::v2_0_ES;
    default:
      IGL_ASSERT_NOT_IMPLEMENTED();
      return GLVersion::v2_0_ES;
    }
  case 3:
    switch (minorVersion) {
    case 0:
      return GLVersion::v3_0_ES;
    case 1:
      return GLVersion::v3_1_ES;
    case 2:
      return GLVersion::v3_2_ES;
    default:
      IGL_ASSERT_NOT_IMPLEMENTED();
      return GLVersion::v3_0_ES;
    }
  default:
    IGL_ASSERT_NOT_IMPLEMENTED();
    return GLVersion::v2_0_ES;
  }

#else
  switch (majorVersion) {
  case 2:
    switch (minorVersion) {
    case 0:
      return GLVersion::v2_0;
    case 1:
      return GLVersion::v2_1;
    default:
      IGL_ASSERT_NOT_IMPLEMENTED();
      return GLVersion::v2_0;
    }

  case 3:
    switch (minorVersion) {
    case 0:
      return GLVersion::v3_0;
    case 1:
      return GLVersion::v3_1;
    case 2:
      return GLVersion::v3_2;
    case 3:
      return GLVersion::v3_3;
    default:
      IGL_ASSERT_NOT_IMPLEMENTED();
      return GLVersion::v3_0;
    }

  case 4:
    switch (minorVersion) {
    case 0:
      return GLVersion::v4_0;
    case 1:
      return GLVersion::v4_1;
    case 2:
      return GLVersion::v4_2;
    case 3:
      return GLVersion::v4_3;
    case 4:
      return GLVersion::v4_4;
    case 5:
      return GLVersion::v4_5;
    case 6:
      return GLVersion::v4_6;
    default:
      IGL_ASSERT_NOT_IMPLEMENTED();
      return GLVersion::v4_0;
    }
  default:
    IGL_ASSERT_NOT_IMPLEMENTED();
    return GLVersion::v2_0;
  }
#endif // IGL_OPENGL_ES
}

} // namespace

std::pair<uint32_t, uint32_t> parseVersionString(const char* version) {
  // If GL_MAJOR_VERSION and/or GL_MINOR_VERSION fail,
  // get the gl version from GL_VERSION string
  if (!version) {
    IGL_LOG_DEBUG("Unable to get GL version string\n");
    return std::make_pair(2, 0);
  }
  uint32_t majorVersion, minorVersion;
#if IGL_OPENGL_ES
  constexpr char versionFormat[] = "OpenGL ES %d.%d";
#else
  constexpr char versionFormat[] = "%d.%d";
#endif // IGL_OPENGL_ES
#ifdef _MSC_VER
  const int ret = sscanf_s(version, versionFormat, &majorVersion, &minorVersion);
#else
  const int ret = sscanf(version, versionFormat, &majorVersion, &minorVersion);
#endif // _MSC_VER
  if (ret != 2) {
    IGL_LOG_DEBUG("failed to parse GL version string %s\n", version);
    return std::make_pair(2, 0);
  }

  return std::make_pair(majorVersion, minorVersion);
}

GLVersion getGLVersion(const char* version, bool constrain) {
  auto [majorVersion, minorVersion] = parseVersionString(version);
  if (constrain) {
    auto [constrainedMajorVersion, constrainedMinorVersion] =
        constrainVersion(majorVersion, minorVersion);
#if IGL_DEBUG
    if (constrainedMajorVersion != majorVersion || constrainedMinorVersion != minorVersion) {
#if IGL_OPENGL_ES
      static constexpr std::string_view gl = "OpenGL ES";
#else
      static constexpr std::string_view gl = "OpenGL";
#endif
      IGL_LOG_INFO(
          "Context supports %s %d.%d, but IGL was only compiled with support for %s "
          "%d.%d\n",
          gl.data(),
          majorVersion,
          minorVersion,
          gl.data(),
          constrainedMajorVersion,
          constrainedMinorVersion);
      IGL_LOG_INFO("Constraining supported version to %s %d.%d\n",
                   gl.data(),
                   constrainedMajorVersion,
                   constrainedMinorVersion);
    }
#endif // IGL_DEBUG
    majorVersion = constrainedMajorVersion;
    minorVersion = constrainedMinorVersion;
  }

  return getGLVersionEnum(majorVersion, minorVersion);
}

ShaderVersion getShaderVersion(GLVersion version) {
  // TODO: Return proper GLSL ES versions
  switch (version) {
  case GLVersion::v2_0_ES:
    return {ShaderFamily::GlslEs, 1, 0};
  case GLVersion::v3_0_ES:
    return {ShaderFamily::GlslEs, 3, 0};
  case GLVersion::v3_1_ES:
    return {ShaderFamily::GlslEs, 3, 10};
  case GLVersion::v3_2_ES:
    return {ShaderFamily::GlslEs, 3, 20};
  case GLVersion::v2_0:
    return {ShaderFamily::Glsl, 1, 10};
  case GLVersion::v2_1:
    return {ShaderFamily::Glsl, 1, 20};
  case GLVersion::v3_0:
    return {ShaderFamily::Glsl, 1, 30};
  case GLVersion::v3_1:
    return {ShaderFamily::Glsl, 1, 40};
  case GLVersion::v3_2:
    return {ShaderFamily::Glsl, 1, 50};
  case GLVersion::v3_3:
    return {ShaderFamily::Glsl, 3, 30};
  case GLVersion::v4_0:
    return {ShaderFamily::Glsl, 4, 0};
  case GLVersion::v4_1:
    return {ShaderFamily::Glsl, 4, 10};
  case GLVersion::v4_2:
    return {ShaderFamily::Glsl, 4, 20};
  case GLVersion::v4_3:
    return {ShaderFamily::Glsl, 4, 30};
  case GLVersion::v4_4:
    return {ShaderFamily::Glsl, 4, 40};
  case GLVersion::v4_5:
    return {ShaderFamily::Glsl, 4, 50};
  case GLVersion::v4_6:
    return {ShaderFamily::Glsl, 4, 60};
  default:
    IGL_ASSERT_NOT_REACHED();
    return {};
  }
}

std::string getStringFromShaderVersion(ShaderVersion version) {
  if (version.family == ShaderFamily::GlslEs) {
    if (version.majorVersion == 1 && version.minorVersion == 0) {
      return "#version 100";

    } else if (version.majorVersion == 3) {
      if (version.minorVersion == 0) {
        return "#version 300 es";
      } else if (version.minorVersion == 10) {
        return "#version 310 es";
      } else if (version.minorVersion == 20) {
        return "#version 320 es";
      } else {
        IGL_ASSERT_NOT_IMPLEMENTED();
      }
    }
  } else {
    if (version.majorVersion == 1) {
      if (version.minorVersion == 10) {
        return "#version 110";
      } else if (version.minorVersion == 20) {
        return "#version 120";
      } else if (version.minorVersion == 30) {
        return "#version 130";
      } else if (version.minorVersion == 40) {
        return "#version 140";
      } else if (version.minorVersion == 50) {
        return "#version 150";
      }
    } else if (version.majorVersion == 3 && version.minorVersion == 30) {
      return "#version 330";
    } else if (version.majorVersion == 4) {
      if (version.minorVersion == 0) {
        return "#version 400";
      } else if (version.minorVersion == 10) {
        return "#version 410";
      } else if (version.minorVersion == 20) {
        return "#version 420";
      } else if (version.minorVersion == 30) {
        return "#version 430";
      } else if (version.minorVersion == 40) {
        return "#version 440";
      } else if (version.minorVersion == 50) {
        return "#version 450";
      } else if (version.minorVersion == 60) {
        return "#version 460";
      }
    }
  }
  IGL_ASSERT_NOT_IMPLEMENTED();
  return "";
}

} // namespace igl::opengl
