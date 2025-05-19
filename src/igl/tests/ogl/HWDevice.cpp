/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/tests/util/device/opengl/TestDevice.h>

#if IGL_PLATFORM_IOS
#include <igl/opengl/ios/HWDevice.h>
#elif IGL_PLATFORM_MACOSX
#include <igl/opengl/macos/HWDevice.h>
#elif IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX_USE_EGL
#include <igl/opengl/egl/HWDevice.h>
#elif IGL_PLATFORM_LINUX
#include <igl/opengl/glx/HWDevice.h>
#elif IGL_PLATFORM_WINDOWS
#if defined(FORCE_USE_ANGLE)
#include <igl/opengl/egl/HWDevice.h>
#else
#include <igl/opengl/wgl/HWDevice.h>
#endif // FORCE_USE_ANGLE
#else
#error "Unsupported testing platform"
#endif

namespace igl::tests {

class HWDeviceOGLTest : public ::testing::Test {
 public:
  HWDeviceOGLTest() = default;
  ~HWDeviceOGLTest() override = default;

  // Set up common resources. This will create a device
  void SetUp() override {
    // Turn off debug break so unit tests can run
    setDebugBreakEnabled(false);

    iglHwDev_ = createHWTestDevice();

    ASSERT_TRUE(iglHwDev_ != nullptr);
  }

  void TearDown() override {}

  std::shared_ptr<opengl::HWDevice> createHWTestDevice() {
#if IGL_PLATFORM_IOS
    return std::make_shared<opengl::ios::HWDevice>();
#elif IGL_PLATFORM_MACOSX
    return std::make_shared<opengl::macos::HWDevice>();
#elif IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX_USE_EGL
    return std::make_shared<opengl::egl::HWDevice>();
#elif IGL_PLATFORM_LINUX
    return std::make_shared<opengl::glx::HWDevice>();
#elif IGL_PLATFORM_WINDOWS
#if defined(FORCE_USE_ANGLE)
    return std::make_shared<opengl::egl::HWDevice>();
#else
    return std::make_shared<opengl::wgl::HWDevice>();
#endif // FORCE_USE_ANGLE
#else
    return nullptr;
#endif
  }

  // Member variables
 protected:
  std::shared_ptr<opengl::HWDevice> iglHwDev_;
};

/// This test ensures a device can be created when calling create()
TEST_F(HWDeviceOGLTest, DeviceCreationSanityTest) {
  {
    Result result;
    const std::unique_ptr<IDevice> device = iglHwDev_->create({}, &result);

    // Ensure the result of the device creation is ok
    ASSERT_TRUE(result.isOk());
  }
  {
    Result result;
    const std::unique_ptr<IDevice> device = iglHwDev_->create(&result);

    // Ensure the result of the device creation is ok
    ASSERT_TRUE(result.isOk());
  }
}

} // namespace igl::tests
