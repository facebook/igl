/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <igl/vulkan/util/SpvConstantSpecialization.h>

namespace igl::tests {

namespace {
uint32_t floatToWord(float val) {
  uint32_t result = 0;
  std::memcpy(&result, &val, sizeof(result));
  return result;
}

uint32_t intToWord(int32_t val) {
  uint32_t result = 0;
  std::memcpy(&result, &val, sizeof(result));
  return result;
}
// Same SPIR-V binary as in SpvConstantSpecializationTest.cpp with two int spec constants
// at indices 50 and 54 (values 10 and 11 respectively)
std::vector<uint32_t> getTestSpv() {
  return {
      0x07230203, 0x00010300, 0xdeadbeef, 0x00000011, 0x00000000, 0x00020011, 0x00000001,
      0x0003000e, 0x00000000, 0x00000000, 0x0006000f, 0x00000004, 0x00000001, 0x6e69616d,
      0x00000000, 0x00000002, 0x00050005, 0x00000003, 0x6e6f436b, 0x6e617473, 0x00003074,
      0x00050005, 0x00000004, 0x6e6f436b, 0x6e617473, 0x00003174, 0x00050005, 0x00000002,
      0x67617266, 0x6f6c6f43, 0x00000072, 0x00040005, 0x00000001, 0x6e69616d, 0x00000000,
      0x00040047, 0x00000003, 0x00000001, 0x00000000, 0x00040047, 0x00000004, 0x00000001,
      0x00000001, 0x00040015, 0x00000005, 0x00000020, 0x00000001, 0x00040032, 0x00000005,
      0x00000003, 0x0000000a, 0x00040032, 0x00000005, 0x00000004, 0x0000000b, 0x00030016,
      0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020,
      0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000002, 0x00000003,
      0x00020013, 0x00000009, 0x00030021, 0x0000000a, 0x00000009, 0x0004002b, 0x00000006,
      0x0000000b, 0x00000000, 0x0004002b, 0x00000006, 0x0000000c, 0x3f800000, 0x00050036,
      0x00000009, 0x00000001, 0x00000000, 0x0000000a, 0x000200f8, 0x0000000d, 0x0004006f,
      0x00000006, 0x0000000e, 0x00000003, 0x0004006f, 0x00000006, 0x0000000f, 0x00000004,
      0x00070050, 0x00000007, 0x00000010, 0x0000000e, 0x0000000f, 0x0000000b, 0x0000000c,
      0x0003003e, 0x00000002, 0x00000010, 0x000100fd, 0x00010038,
  };
}
} // namespace

TEST(SpvConstantSpecializationExtendedTest, FloatConstantSpecialization) {
  using namespace vulkan::util;

  auto spv = getTestSpv();

  EXPECT_EQ(spv[50], intToWord(10));
  EXPECT_EQ(spv[54], intToWord(11));

  const std::vector<uint32_t> values = {floatToWord(3.14f), floatToWord(2.71f)};
  specializeConstants(spv.data(), spv.size() * sizeof(uint32_t), values);

  EXPECT_EQ(spv[50], floatToWord(3.14f));
  EXPECT_EQ(spv[54], floatToWord(2.71f));
}

TEST(SpvConstantSpecializationExtendedTest, MultipleConstantsMixedTypes) {
  using namespace vulkan::util;

  auto spv = getTestSpv();

  EXPECT_EQ(spv[50], intToWord(10));
  EXPECT_EQ(spv[54], intToWord(11));

  const std::vector<uint32_t> values = {intToWord(100), intToWord(200)};
  specializeConstants(spv.data(), spv.size() * sizeof(uint32_t), values);

  EXPECT_EQ(spv[50], intToWord(100));
  EXPECT_EQ(spv[54], intToWord(200));
}

TEST(SpvConstantSpecializationExtendedTest, NoSpecConstants) {
  using namespace vulkan::util;

  auto spv = getTestSpv();

  uint32_t originalVal0 = spv[50];
  uint32_t originalVal1 = spv[54];

  const std::vector<uint32_t> values = {vulkan::util::kNoValue, vulkan::util::kNoValue};
  specializeConstants(spv.data(), spv.size() * sizeof(uint32_t), values);

  EXPECT_EQ(spv[50], originalVal0);
  EXPECT_EQ(spv[54], originalVal1);
}

} // namespace igl::tests
