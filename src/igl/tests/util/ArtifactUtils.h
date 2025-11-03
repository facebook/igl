/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace igl::tests::util {

struct ArtifactPaths {
  std::filesystem::path shaFile;
  std::filesystem::path pngFile;
};

std::string currentBackend();

std::filesystem::path artifactsRoot();

std::filesystem::path ensureArtifactDirectory(const std::string& relativeGroup,
                                              const std::string& backend);

ArtifactPaths makeArtifactPaths(const std::string& relativeGroup,
                                const std::string& backend,
                                const std::string& testName,
                                bool includeImage);

void writeBinaryFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes);

void writeTextFile(const std::filesystem::path& path, const std::string& text);

std::string computeSha256(std::span<const std::uint8_t> bytes);

void writeSha256File(const std::filesystem::path& path, const std::string& hash);

void writePng(const std::filesystem::path& path,
              const std::uint8_t* rgbaPixels,
              std::uint32_t width,
              std::uint32_t height);

} // namespace igl::tests::util

