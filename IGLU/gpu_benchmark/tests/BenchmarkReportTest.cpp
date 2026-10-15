/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/gpu_benchmark/BenchmarkReport.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <vector>
#include <igl/Common.h> // IWYU pragma: keep

namespace iglu::gpu_benchmark::tests {

namespace {

std::vector<std::string>& captured() {
  static auto* const kLines = new std::vector<std::string>();
  return *kLines;
}

int captureHandler(IGLLogLevel /*level*/, const char* IGL_RESTRICT format, va_list ap) {
  char buffer[2048];
  const int length = std::vsnprintf(buffer, sizeof(buffer), format, ap);
  captured().emplace_back(buffer);
  return length;
}

BenchmarkReport makeReport() {
  BenchmarkReport report("TILEBENCH_V1");
  report.addMeta("device", "Test, Device");
  report.addMeta("api", "vulkan");
  report.addResult({
      .scenario = "blur",
      .size = "1920x1080",
      .variant = "classic",
      .gpuNanos = {1000, 2000, 3000},
      .roundMedianNanos = {1000, 1000},
      .traffic = {.readBytes = 2000000, .writeBytes = 1000000},
      .footprint = {.bytes = 8000000, .memorylessBytes = 0, .memorylessBacked = false},
      .note = "a,b",
  });
  report.addResult({
      .scenario = "blur",
      .size = "1920x1080",
      .variant = "tile",
      .gpuNanos = {500, 1000, 1500},
      .roundMedianNanos = {1000, 1100, 1200},
      .traffic = {.readBytes = 1000000, .writeBytes = 500000},
      .footprint = {.bytes = 4000000, .memorylessBytes = 4000000, .memorylessBacked = true},
  });
  report.addResult({
      .scenario = "blur",
      .size = "3840x2160",
      .variant = "tile",
      .gpuNanos = {},
  });
  return report;
}

} // namespace

TEST(BenchmarkReportTest, CsvRows) {
  const BenchmarkReport report = makeReport();
  const std::vector<std::string> rows = report.csvRows("classic");
  ASSERT_EQ(rows.size(), 5u);
  EXPECT_EQ(rows[0], "TILEBENCH_V1,meta,device,Test; Device");
  EXPECT_EQ(rows[1], "TILEBENCH_V1,meta,api,vulkan");
  EXPECT_EQ(rows[2],
            "TILEBENCH_V1,summary,blur,1920x1080,classic,3,gpu,2000.0,1200.0,2800.0,1000.0,1.0000,"
            "2000000,1000000,1500.000,8000000,0,0.000000,a;b");
  EXPECT_EQ(rows[3],
            "TILEBENCH_V1,summary,blur,1920x1080,tile,3,gpu,1000.0,600.0,1400.0,500.0,0.5000,"
            "1000000,500000,1500.000,4000000,1,0.090909,");
  EXPECT_EQ(rows[4],
            "TILEBENCH_V1,summary,blur,3840x2160,tile,0,gpu,0.0,0.0,0.0,0.0,,0,0,,0,0,0.000000,");

  const std::vector<std::string> noBaseline = report.csvRows("");
  EXPECT_NE(noBaseline[2].find("2000.0,1200.0,2800.0,1000.0,,2000000"), std::string::npos)
      << noBaseline[2];
  const std::vector<std::string> missingBaseline = report.csvRows("nope");
  EXPECT_NE(missingBaseline[3].find("1400.0,500.0,,1000000"), std::string::npos)
      << missingBaseline[3];

  const std::vector<std::string> withSamples = report.csvRows("classic", true);
  ASSERT_EQ(withSamples.size(), 5u + 6u);
  EXPECT_EQ(withSamples[3], "TILEBENCH_V1,sample,blur,1920x1080,classic,0,1000.0");
  EXPECT_EQ(withSamples[5], "TILEBENCH_V1,sample,blur,1920x1080,classic,2,3000.0");
  EXPECT_EQ(withSamples[6].rfind("TILEBENCH_V1,summary,blur,1920x1080,tile", 0), 0u);
  EXPECT_EQ(withSamples[7], "TILEBENCH_V1,sample,blur,1920x1080,tile,0,500.0");
}

TEST(BenchmarkReportTest, PhaseRow) {
  EXPECT_EQ(BenchmarkReport::phaseRow("P", "begin", "s", "z", "v", 1.5, 3),
            "P,phase,begin,s,z,v,1.500000,3");
  EXPECT_EQ(BenchmarkReport::phaseRow("P,x", "end", "s,1", "z", "v", 0.0, 0),
            "P;x,phase,end,s;1,z,v,0.000000,0");
}

TEST(BenchmarkReportTest, TableAndLog) {
  const BenchmarkReport report = makeReport();
  const std::string table = report.table("classic");
  std::cout << table;
  EXPECT_FALSE(table.empty());
  EXPECT_EQ(table.back(), '\n');
  EXPECT_NE(table.find("footprint MB"), std::string::npos);
  EXPECT_NE(table.find("4.00*"), std::string::npos);
  EXPECT_NE(table.find("* memoryless textures"), std::string::npos);
  EXPECT_EQ(BenchmarkReport("X").table("a"), "");

  IGLLogHandlerFunc previous = IGLLogGetHandler();
  IGLLogSetHandler(captureHandler);
  captured().clear();
  report.log("classic", true);
  IGLLogSetHandler(previous);
  ASSERT_FALSE(captured().empty());
  size_t csvLines = 0;
  size_t tableLines = 0;
  for (const std::string& line : captured()) {
    EXPECT_EQ(line.back(), '\n');
    EXPECT_EQ(line.find('\n'), line.size() - 1) << line;
    if (line.rfind("TILEBENCH_V1,", 0) == 0) {
      ++csvLines;
    } else {
      ++tableLines;
    }
  }
  EXPECT_EQ(csvLines, report.csvRows("classic", true).size());
  EXPECT_EQ(tableLines, 1u + 3u + 1u);
  std::cout << "---- csv:\n";
  for (const std::string& row : report.csvRows("classic", false)) {
    std::cout << row << '\n';
  }
}

TEST(BenchmarkReportTest, TimerColumnAndQuotes) {
  BenchmarkReport report("P");
  report.addResult({.scenario = "s",
                    .size = "z",
                    .variant = "cpu",
                    .timerKind = TimerKind::CpuWallClock,
                    .gpuNanos = {100, 100},
                    .note = "say \"hi\", ok"});
  report.addResult({.scenario = "s", .size = "z", .variant = "gpu", .gpuNanos = {100, 100}});
  const auto rows = report.csvRows("");
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0],
            "P,summary,s,z,cpu,2,cpu,100.0,100.0,100.0,0.0,,0,0,0.000,0,0,0.000000,say 'hi'; ok");
  EXPECT_EQ(rows[1], "P,summary,s,z,gpu,2,gpu,100.0,100.0,100.0,0.0,,0,0,0.000,0,0,0.000000,");
}

TEST(BenchmarkReportTest, PairedRatio) {
  const double nan = std::nan("");
  const auto ratioOf = [&](std::vector<double> baselineRounds,
                           std::vector<double> otherRounds,
                           const std::string& baselineVariant = "base") {
    BenchmarkReport report("P");
    report.addResult({.scenario = "s",
                      .size = "z",
                      .variant = "base",
                      .gpuNanos = {100, 100, 100, 100},
                      .roundMedianNanos = std::move(baselineRounds)});
    report.addResult({.scenario = "s",
                      .size = "z",
                      .variant = "other",
                      .gpuNanos = {50, 50, 50, 50},
                      .roundMedianNanos = std::move(otherRounds)});
    const auto rows = report.csvRows(baselineVariant);
    EXPECT_EQ(rows.size(), 2u);
    std::vector<std::string> ratios;
    for (const std::string& row : rows) {
      EXPECT_EQ(row.find("nan"), std::string::npos) << row;
      std::vector<std::string> fields;
      size_t begin = 0;
      while (true) {
        const size_t comma = row.find(',', begin);
        fields.push_back(row.substr(begin, comma == std::string::npos ? comma : comma - begin));
        if (comma == std::string::npos) {
          break;
        }
        begin = comma + 1;
      }
      ratios.push_back(fields.at(11));
    }
    return ratios;
  };

  auto ratios = ratioOf({100, 200, 100, 400}, {50, 50, 100, 100});
  EXPECT_EQ(ratios[0], "1.0000");
  EXPECT_EQ(ratios[1], "0.3750");
  ratios = ratioOf({100, 200, 100, 400}, {nan, 50, 100, 100});
  EXPECT_EQ(ratios[1], "0.2500");
  ratios = ratioOf({100, nan, 100, 400}, {50, 50, 100, 100});
  EXPECT_EQ(ratios[1], "0.5000");
  ratios = ratioOf({100, 200, 100, 400}, {50, 50});
  EXPECT_EQ(ratios[1], "0.5000");
  ratios = ratioOf({100, 200}, {50, 50, 100});
  EXPECT_EQ(ratios[1], "0.5000");
  ratios = ratioOf({}, {});
  EXPECT_EQ(ratios[1], "0.5000");
  ratios = ratioOf({100, 200}, {});
  EXPECT_EQ(ratios[1], "0.5000");
  ratios = ratioOf({nan, nan}, {50, 50});
  EXPECT_EQ(ratios[1], "0.5000");
  ratios = ratioOf({100, 200, 100, 400}, {50, 50, 100, 100});
  EXPECT_EQ(ratios[0], "1.0000");
  ratios = ratioOf({100, 200, 100, 400}, {50, 50, 100, 100}, "");
  EXPECT_EQ(ratios[0], "");
  EXPECT_EQ(ratios[1], "");
  ratios = ratioOf({100, 200, 100, 400}, {50, 50, 100, 100}, "missing");
  EXPECT_EQ(ratios[1], "");
}

TEST(BenchmarkReportTest, TableUsesThePairedRatio) {
  BenchmarkReport report("P");
  report.addResult({.scenario = "s",
                    .size = "z",
                    .variant = "base",
                    .gpuNanos = {100, 100, 100, 100},
                    .roundMedianNanos = {100, 200, 100, 400}});
  report.addResult({.scenario = "s",
                    .size = "z",
                    .variant = "other",
                    .gpuNanos = {50, 50, 50, 50},
                    .roundMedianNanos = {50, 50, 100, 100}});
  const std::string table = report.table("base");
  EXPECT_NE(table.find("0.3750"), std::string::npos) << table;
  EXPECT_EQ(table.find("0.5000"), std::string::npos) << table;
}

TEST(BenchmarkReportTest, NanRoundDoesNotBreakDrift) {
  BenchmarkReport report("P");
  report.addResult({.scenario = "s",
                    .size = "z",
                    .variant = "v",
                    .gpuNanos = {100},
                    .roundMedianNanos = {100, std::nan(""), 120}});
  const auto rows = report.csvRows("");
  ASSERT_EQ(rows.size(), 1u);
  const std::string& row = rows[0];
  EXPECT_NE(row.find("P,summary"), std::string::npos);
  EXPECT_EQ(row.find("nan"), std::string::npos) << row;
}

} // namespace iglu::gpu_benchmark::tests
