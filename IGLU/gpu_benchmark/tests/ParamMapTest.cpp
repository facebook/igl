/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/gpu_benchmark/ParamMap.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
#include <igl/Common.h> // IWYU pragma: keep

namespace iglu::gpu_benchmark::tests {

namespace {

std::vector<std::string>& captured() {
  static auto* const kLines = new std::vector<std::string>();
  return *kLines;
}

int captureHandler(IGLLogLevel /*level*/, const char* IGL_RESTRICT format, va_list ap) {
  char buffer[1024];
  const int length = std::vsnprintf(buffer, sizeof(buffer), format, ap);
  captured().emplace_back(buffer);
  return length;
}

bool contains(const std::string& text, const char* part) {
  return text.find(part) != std::string::npos;
}

} // namespace

TEST(ParamMapTest, SourcesAndPrecedence) {
  const std::vector<std::pair<std::string, std::string>> custom = {
      {"--tile-rounds", "5"},
      {"tile-name", "abc"},
      {"other", "x"},
      {"-tile-dash", "1"},
      {"tile-flag-empty", ""},
  };
  const std::vector<std::string> args = {"prog",
                                         "--tile-rounds",
                                         "7",
                                         "--tile-flag",
                                         "--tile-eq=3",
                                         "--other",
                                         "value",
                                         "--tile-neg",
                                         "-5",
                                         "--tile-last"};
  const ParamMap params(custom, args, "tile-");
  EXPECT_EQ(params.getInt("tile-rounds", 0), 7);
  EXPECT_EQ(params.getString("tile-name", "d"), "abc");
  EXPECT_FALSE(params.has("other"));
  EXPECT_FALSE(params.has("--tile-rounds"));
  EXPECT_TRUE(params.getBool("tile-flag", false));
  EXPECT_EQ(params.getString("tile-flag", "d"), "1");
  EXPECT_EQ(params.getInt("tile-eq", 0), 3);
  EXPECT_EQ(params.getInt("tile-dash", 0), 1);
  EXPECT_EQ(params.getInt("tile-neg", 0), -5);
  EXPECT_TRUE(params.getBool("tile-last", false));
  EXPECT_TRUE(params.has("tile-flag-empty"));
  EXPECT_TRUE(params.getBool("tile-flag-empty", false));
  EXPECT_EQ(params.getInt("tile-missing", 42), 42);
}

TEST(ParamMapTest, Typed) {
  const ParamMap params(
      {{"tile-i", "12"},
       {"tile-bad-i", "12x"},
       {"tile-big", "99999999999999999999"},
       {"tile-d", "0.25"},
       {"tile-bad-d", "abc"},
       {"tile-nan", "nan"},
       {"tile-inf", "inf"},
       {"tile-d-trail", "1.5x"},
       {"tile-ws", " 3 "},
       {"tile-b1", "TRUE"},
       {"tile-b2", "Off"},
       {"tile-b3", "maybe"},
       {"tile-list", " a, b ,,c , "},
       {"tile-sizes", "1920x1080, 3840X2160,bad,0x10,10x0,1920,x5,5x,-1x4,7x8x9,640x480"}},
      {},
      "tile-");
  EXPECT_EQ(params.getInt("tile-i", -1), 12);
  EXPECT_EQ(params.getInt("tile-bad-i", -1), -1);
  EXPECT_EQ(params.getInt("tile-big", -1), -1);
  EXPECT_DOUBLE_EQ(params.getDouble("tile-d", -1.0), 0.25);
  EXPECT_DOUBLE_EQ(params.getDouble("tile-i", -1.0), 12.0);
  EXPECT_DOUBLE_EQ(params.getDouble("tile-bad-d", -1.0), -1.0);
  EXPECT_DOUBLE_EQ(params.getDouble("tile-nan", -1.0), -1.0);
  EXPECT_DOUBLE_EQ(params.getDouble("tile-inf", -1.0), -1.0);
  EXPECT_DOUBLE_EQ(params.getDouble("tile-d-trail", -1.0), -1.0);
  EXPECT_EQ(params.getInt("tile-ws", -1), 3);
  EXPECT_TRUE(params.getBool("tile-b1", false));
  EXPECT_FALSE(params.getBool("tile-b2", true));
  EXPECT_TRUE(params.getBool("tile-b3", true));
  EXPECT_FALSE(params.getBool("tile-b3", false));
  EXPECT_EQ(params.getList("tile-list", {"z"}), (std::vector<std::string>{"a", "b", "c"}));
  EXPECT_EQ(params.getList("tile-nolist", {"z"}), (std::vector<std::string>{"z"}));
  using Sizes = std::vector<std::pair<uint32_t, uint32_t>>;
  EXPECT_EQ(params.getSizes("tile-sizes", {}), (Sizes{{1920, 1080}, {3840, 2160}, {640, 480}}));
  EXPECT_EQ(params.getSizes("tile-nosizes", {{1, 2}}), (Sizes{{1, 2}}));
}

TEST(ParamMapTest, MoreEdgeCases) {
  const ParamMap params({},
                        {"--tile-expr=a=b",
                         "-tile-single",
                         "5",
                         "--tile-empty=",
                         "--",
                         "--tile-list",
                         "x, y",
                         "--tile-sizes",
                         "",
                         "--tile-twice",
                         "1",
                         "--tile-twice",
                         "2",
                         "---tile-three",
                         "3"},
                        "tile-");
  EXPECT_EQ(params.getString("tile-expr", "d"), "a=b");
  EXPECT_FALSE(params.has("tile-single"));
  EXPECT_TRUE(params.has("tile-empty"));
  EXPECT_EQ(params.getString("tile-empty", "d"), "");
  EXPECT_TRUE(params.getBool("tile-empty", false));
  EXPECT_EQ(params.getList("tile-list", {}), (std::vector<std::string>{"x", "y"}));
  EXPECT_EQ(params.getSizes("tile-sizes", {{1, 1}}),
            (std::vector<std::pair<uint32_t, uint32_t>>{{1, 1}}));
  EXPECT_EQ(params.getInt("tile-twice", 0), 2);
  EXPECT_EQ(params.getInt("tile-three", 0), 3);
  const ParamMap everything({}, {"--a", "1", "--b=2"}, "");
  EXPECT_EQ(everything.getInt("a", 0), 1);
  EXPECT_EQ(everything.getInt("b", 0), 2);
}

TEST(ParamMapTest, GetUint) {
  const ParamMap params({{"tile-a", "7"},
                         {"tile-low", "-5"},
                         {"tile-high", "5000000000"},
                         {"tile-huge", "99999999999999999999"},
                         {"tile-bad", "x"},
                         {"tile-ws", " 12 "},
                         {"tile-zero", "0"}},
                        {},
                        "tile-");
  EXPECT_EQ(params.getUint("tile-a", 1, 1, 10), 7u);
  EXPECT_EQ(params.getUint("tile-low", 4, 1, 10), 1u);
  EXPECT_EQ(params.getUint("tile-high", 4, 1, 10), 10u);
  EXPECT_EQ(params.getUint("tile-high", 4, 0, UINT32_MAX), UINT32_MAX);
  EXPECT_EQ(params.getUint("tile-huge", 4, 1, 10), 4u);
  EXPECT_EQ(params.getUint("tile-bad", 4, 1, 10), 4u);
  EXPECT_EQ(params.getUint("tile-missing", 4, 1, 10), 4u);
  EXPECT_EQ(params.getUint("tile-missing", 0, 1, 10), 0u);
  EXPECT_EQ(params.getUint("tile-ws", 4, 1, 10), 10u);
  EXPECT_EQ(params.getUint("tile-zero", 4, 1, 10), 1u);
  EXPECT_EQ(params.getUint("tile-a", 1, 10, 3), 10u);
}

TEST(ParamMapTest, EmptyResultsReturnTheDefault) {
  const ParamMap params({{"tile-empty", ""},
                         {"tile-commas", " , ,, "},
                         {"tile-garbage", "bad,0x10,5x"},
                         {"tile-mixed", "bad,640x480"}},
                        {"--tile-flag"},
                        "tile-");
  EXPECT_EQ(params.getList("tile-empty", {"d"}), (std::vector<std::string>{"d"}));
  EXPECT_EQ(params.getList("tile-commas", {"d"}), (std::vector<std::string>{"d"}));
  EXPECT_EQ(params.getList("tile-flag", {"d"}), (std::vector<std::string>{"1"}));
  using Sizes = std::vector<std::pair<uint32_t, uint32_t>>;
  EXPECT_EQ(params.getSizes("tile-empty", {{1, 1}}), (Sizes{{1, 1}}));
  EXPECT_EQ(params.getSizes("tile-garbage", {{2, 2}}), (Sizes{{2, 2}}));
  EXPECT_EQ(params.getSizes("tile-mixed", {{2, 2}}), (Sizes{{640, 480}}));
  EXPECT_TRUE(params.getList("tile-empty", {}).empty());
}

TEST(ParamMapTest, WarnsAboutIgnoredAndClampedValues) {
  const ParamMap params({{"tile-i", "12x"},
                         {"tile-u", "99"},
                         {"tile-d", "abc"},
                         {"tile-b", "maybe"},
                         {"tile-s", "0x0,8x8,wide"},
                         {"tile-ok", "5"}},
                        {},
                        "tile-");
  const IGLLogHandlerFunc previous = IGLLogGetHandler();
  IGLLogSetHandler(captureHandler);
  captured().clear();
  EXPECT_EQ(params.getInt("tile-i", 1), 1);
  EXPECT_EQ(params.getUint("tile-u", 1, 1, 7), 7u);
  EXPECT_EQ(params.getDouble("tile-d", 0.5), 0.5);
  EXPECT_TRUE(params.getBool("tile-b", true));
  EXPECT_EQ(params.getSizes("tile-s", {}).size(), 1u);
  const std::vector<std::string> warnings = captured();
  EXPECT_EQ(params.getUint("tile-ok", 1, 1, 7), 5u);
  EXPECT_EQ(params.getInt("tile-missing", 3), 3);
  EXPECT_EQ(params.getString("tile-s", ""), "0x0,8x8,wide");
  const size_t numLines = captured().size();
  IGLLogSetHandler(previous);

  ASSERT_EQ(warnings.size(), 6u);
  EXPECT_TRUE(contains(warnings[0], "'12x' of tile-i")) << warnings[0];
  EXPECT_TRUE(contains(warnings[1], "99 of tile-u is outside 1 to 7, using 7")) << warnings[1];
  EXPECT_TRUE(contains(warnings[2], "'abc' of tile-d")) << warnings[2];
  EXPECT_TRUE(contains(warnings[3], "'maybe' of tile-b")) << warnings[3];
  EXPECT_TRUE(contains(warnings[4], "'0x0' of tile-s")) << warnings[4];
  EXPECT_TRUE(contains(warnings[5], "'wide' of tile-s")) << warnings[5];
  EXPECT_EQ(numLines, warnings.size()) << "valid and missing values log nothing";
}

} // namespace iglu::gpu_benchmark::tests
