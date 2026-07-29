/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <igl/opengl/empty/HWDevice.h>

#include <memory>
#include <utility>
#include <igl/Common.h>
#include <igl/DeviceFeatures.h>
#include <igl/opengl/IContext.h>

namespace igl::opengl::empty::tests {

TEST(EmptyHWDeviceTest, CreateContextReturnsUsableEmptyContext) {
  const HWDevice hwDevice;
  Result result;

  auto context = hwDevice.createContext(&result);

  ASSERT_NE(context, nullptr);
  EXPECT_TRUE(result.isOk());
  // The empty backend's context is always the current context by design.
  EXPECT_TRUE(context->isCurrentContext());
}

TEST(EmptyHWDeviceTest, CreateContextWithNullResultStillReturnsUsableContext) {
  const HWDevice hwDevice;

  auto context = hwDevice.createContext(nullptr);

  ASSERT_NE(context, nullptr);
  EXPECT_TRUE(context->isCurrentContext());
}

TEST(EmptyHWDeviceTest, CreateContextWithBackendVersionReturnsUsableContext) {
  const HWDevice hwDevice;
  const BackendVersion backendVersion{
      .flavor = BackendFlavor::OpenGL_ES,
      .majorVersion = 3,
      .minorVersion = 0,
  };
  Result result;

  auto context = hwDevice.createContext(backendVersion, IGL_EGL_NULL_WINDOW, &result);

  ASSERT_NE(context, nullptr);
  EXPECT_TRUE(result.isOk());
  EXPECT_TRUE(context->isCurrentContext());
}

TEST(EmptyHWDeviceTest, CreateWithContextReturnsOpenGLDevice) {
  const HWDevice hwDevice;
  Result contextResult;
  auto context = hwDevice.createContext(&contextResult);
  ASSERT_NE(context, nullptr);

  Result result;
  auto device = hwDevice.createWithContext(std::move(context), &result);

  ASSERT_NE(device, nullptr);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(device->getBackendType(), BackendType::OpenGL);
}

TEST(EmptyHWDeviceTest, CreateWithNullContextReportsArgumentInvalid) {
  const HWDevice hwDevice;
  Result result;

  auto device = hwDevice.createWithContext(nullptr, &result);

  EXPECT_EQ(device, nullptr);
  EXPECT_EQ(result.code, Result::Code::ArgumentInvalid);
  EXPECT_THAT(result.message, ::testing::HasSubstr("null"));
}

TEST(EmptyHWDeviceTest, CreateBuildsFullOpenGLDevice) {
  const HWDevice hwDevice;
  Result result;

  auto device = hwDevice.create(&result);

  ASSERT_NE(device, nullptr);
  EXPECT_TRUE(result.isOk());
  EXPECT_EQ(device->getBackendType(), BackendType::OpenGL);
}

} // namespace igl::opengl::empty::tests
