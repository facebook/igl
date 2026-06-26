/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/HWDevice.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// HWDeviceType
// ---------------------------------------------------------------------------

TEST(HWDeviceTypeTest, EnumValues) {
  EXPECT_EQ(static_cast<int>(HWDeviceType::Unknown), 0);
  EXPECT_EQ(static_cast<int>(HWDeviceType::DiscreteGpu), 1);
  EXPECT_EQ(static_cast<int>(HWDeviceType::ExternalGpu), 2);
  EXPECT_EQ(static_cast<int>(HWDeviceType::IntegratedGpu), 3);
  EXPECT_EQ(static_cast<int>(HWDeviceType::SoftwareGpu), 4);
}

TEST(HWDeviceTypeTest, AllValuesDistinct) {
  EXPECT_NE(HWDeviceType::Unknown, HWDeviceType::DiscreteGpu);
  EXPECT_NE(HWDeviceType::DiscreteGpu, HWDeviceType::IntegratedGpu);
  EXPECT_NE(HWDeviceType::IntegratedGpu, HWDeviceType::SoftwareGpu);
  EXPECT_NE(HWDeviceType::ExternalGpu, HWDeviceType::SoftwareGpu);
}

// ---------------------------------------------------------------------------
// HWDeviceQueryDesc
// ---------------------------------------------------------------------------

TEST(HWDeviceQueryDescTest, ConstructWithType) {
  const HWDeviceQueryDesc desc(HWDeviceType::DiscreteGpu);
  EXPECT_EQ(desc.hardwareType, HWDeviceType::DiscreteGpu);
  EXPECT_EQ(desc.displayId, 0u);
  EXPECT_EQ(desc.flags, 0u);
}

TEST(HWDeviceQueryDescTest, ConstructWithTypeAndDisplay) {
  const HWDeviceQueryDesc desc(HWDeviceType::IntegratedGpu, 42);
  EXPECT_EQ(desc.hardwareType, HWDeviceType::IntegratedGpu);
  EXPECT_EQ(desc.displayId, 42u);
  EXPECT_EQ(desc.flags, 0u);
}

TEST(HWDeviceQueryDescTest, ConstructWithAllParams) {
  const HWDeviceQueryDesc desc(HWDeviceType::ExternalGpu, 100, 0x01);
  EXPECT_EQ(desc.hardwareType, HWDeviceType::ExternalGpu);
  EXPECT_EQ(desc.displayId, 100u);
  EXPECT_EQ(desc.flags, 0x01u);
}

// ---------------------------------------------------------------------------
// HWDeviceDesc
// ---------------------------------------------------------------------------

TEST(HWDeviceDescTest, ConstructWithGuidAndType) {
  const HWDeviceDesc desc(1234, HWDeviceType::DiscreteGpu);
  EXPECT_EQ(desc.guid, 1234u);
  EXPECT_EQ(desc.type, HWDeviceType::DiscreteGpu);
  EXPECT_EQ(desc.vendorId, 0u);
  EXPECT_TRUE(desc.name.empty());
  EXPECT_TRUE(desc.vendor.empty());
}

TEST(HWDeviceDescTest, ConstructWithAllParams) {
  const HWDeviceDesc desc(5678, HWDeviceType::IntegratedGpu, 0x10DE, "RTX 4090", "NVIDIA");
  EXPECT_EQ(desc.guid, 5678u);
  EXPECT_EQ(desc.type, HWDeviceType::IntegratedGpu);
  EXPECT_EQ(desc.vendorId, 0x10DEu);
  EXPECT_EQ(desc.name, "RTX 4090");
  EXPECT_EQ(desc.vendor, "NVIDIA");
}

TEST(HWDeviceDescTest, SoftwareGpuDevice) {
  const HWDeviceDesc desc(0, HWDeviceType::SoftwareGpu, 0, "llvmpipe", "Mesa");
  EXPECT_EQ(desc.type, HWDeviceType::SoftwareGpu);
  EXPECT_EQ(desc.name, "llvmpipe");
  EXPECT_EQ(desc.vendor, "Mesa");
}

TEST(HWDeviceDescTest, UnknownDeviceType) {
  const HWDeviceDesc desc(0, HWDeviceType::Unknown);
  EXPECT_EQ(desc.type, HWDeviceType::Unknown);
  EXPECT_EQ(desc.guid, 0u);
}

} // namespace igl::tests
