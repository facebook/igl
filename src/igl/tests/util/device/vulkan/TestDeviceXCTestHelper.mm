/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/vulkan/TestDeviceXCTestHelper.h>

#import <Foundation/Foundation.h>

// We store icd files and the Frameworks in the app bundle for apps and the test bundle for XCTests.
// Meanwhile Vulkan Loader uses `CFBundleGetMainBundle` to locate those icd files
// (in Vulkan-Loader/loader/loader.c). This works good with bundled Mac apps and even auxiliary
// binaries in the app bundle. However for XCTest instances - the main bundle would be the bundle of
// the test runner but not the actual bundle with test code. Hence we need to add this dummy
// Objective-C class here in order to use `+[NSBundle bundleForClass:]` to locate the test bundle
// packaged with Vulkan's manifest files, and then provide paths to the Vulkan loader by ENVs.
@interface IGLVulkanTestsBundleAccessor : NSObject
@end
@implementation IGLVulkanTestsBundleAccessor
@end

namespace igl::tests::util::device::vulkan {
namespace {

// These constants are defined in Vulkan-Loader/loader/vk_loader_platform.h
constexpr const char* kVulkanResourcesDirectoryName = "vulkan";
constexpr const char* kVulkanAdditionalDriverFilesEnvKey = "VK_ADD_DRIVER_FILES";
constexpr const char* kVulkanAdditionalLayerFilesEnvKey = "VK_ADD_LAYER_PATH";
constexpr const char* kVulkanDriverConfigFileDirectoryName = "icd.d";
constexpr const char* kVulkanLayerConfigFileDirectoryName = "explicit_layer.d";

enum class VulkanConfigFileType {
  Driver,
  Layer,
};

auto getAdditionalConfigFileDirectoryEnvKey(VulkanConfigFileType configFileType) {
  switch (configFileType) {
  case VulkanConfigFileType::Driver:
    return kVulkanAdditionalDriverFilesEnvKey;
  case VulkanConfigFileType::Layer:
    return kVulkanAdditionalLayerFilesEnvKey;
  }
}

auto getConfigFileDirectoryName(VulkanConfigFileType configFileType) {
  switch (configFileType) {
  case VulkanConfigFileType::Driver:
    return kVulkanDriverConfigFileDirectoryName;
  case VulkanConfigFileType::Layer:
    return kVulkanLayerConfigFileDirectoryName;
  }
}

NSString* _Nullable getVulkanConfigFileDirectoryPath(NSBundle* _Nonnull bundle,
                                                     VulkanConfigFileType configFileType) {
  return [bundle pathForResource:@(getConfigFileDirectoryName(configFileType))
                          ofType:nil
                     inDirectory:@(kVulkanResourcesDirectoryName)];
}

void setEnvForAdditionalVulkanConfigFileDirectory(NSBundle* _Nonnull bundle,
                                                  VulkanConfigFileType configFileType) {
  NSString* directoryPath = getVulkanConfigFileDirectoryPath(bundle, configFileType);
  if (!directoryPath) {
    return;
  }
  const auto* envKey = getAdditionalConfigFileDirectoryEnvKey(configFileType);
  const auto* envValue = directoryPath.UTF8String;
  ::setenv(envKey, envValue, 0 /* override */);
}

} // namespace

void setupXCTestEnvironment() {
  @autoreleasepool {
    NSBundle* testBundle = [NSBundle bundleForClass:IGLVulkanTestsBundleAccessor.class];
    setEnvForAdditionalVulkanConfigFileDirectory(testBundle, VulkanConfigFileType::Driver);
    setEnvForAdditionalVulkanConfigFileDirectory(testBundle, VulkanConfigFileType::Layer);
  }
}

} // namespace igl::tests::util::device::vulkan
