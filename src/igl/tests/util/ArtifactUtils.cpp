/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "ArtifactUtils.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <stb_image_write.h>

namespace igl::tests::util {

namespace {

constexpr std::array<std::uint32_t, 8> kInitialState = {
    0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
    0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u};

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u, 0x923F82A4u,
    0xAB1C5ED5u, 0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu,
    0x9BDC06A7u, 0xC19BF174u, 0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu,
    0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu, 0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u,
    0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u, 0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu,
    0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u, 0xA2BFE8A1u, 0xA81A664Bu,
    0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u, 0x19A4C116u,
    0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
    0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u, 0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u,
    0xC67178F2u};

inline std::uint32_t rotr(std::uint32_t value, std::uint32_t bits) {
  return (value >> bits) | (value << (32u - bits));
}

class Sha256Context {
 public:
  Sha256Context() = default;

  void update(const std::uint8_t* data, std::size_t len) {
    if (finalized_) {
      throw std::logic_error("SHA256 update after finalization");
    }

    totalBits_ += static_cast<std::uint64_t>(len) * 8u;

    while (len > 0) {
      const auto space = 64u - bufferSize_;
      const auto toCopy = std::min<std::size_t>(len, space);
      std::memcpy(buffer_.data() + bufferSize_, data, toCopy);
      bufferSize_ += toCopy;
      data += toCopy;
      len -= toCopy;

      if (bufferSize_ == 64u) {
        processBlock(buffer_.data());
        bufferSize_ = 0u;
      }
    }
  }

  std::array<std::uint8_t, 32> finalize() {
    if (!finalized_) {
      finalizeInternal();
    }
    return digest_;
  }

 private:
  void processBlock(const std::uint8_t* block) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i != 16; ++i) {
      const auto idx = i * 4;
      w[i] = (static_cast<std::uint32_t>(block[idx]) << 24u) |
             (static_cast<std::uint32_t>(block[idx + 1]) << 16u) |
             (static_cast<std::uint32_t>(block[idx + 2]) << 8u) |
             static_cast<std::uint32_t>(block[idx + 3]);
    }

    for (std::size_t i = 16; i != 64; ++i) {
      const auto s0 = rotr(w[i - 15], 7u) ^ rotr(w[i - 15], 18u) ^ (w[i - 15] >> 3u);
      const auto s1 = rotr(w[i - 2], 17u) ^ rotr(w[i - 2], 19u) ^ (w[i - 2] >> 10u);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    auto a = state_[0];
    auto b = state_[1];
    auto c = state_[2];
    auto d = state_[3];
    auto e = state_[4];
    auto f = state_[5];
    auto g = state_[6];
    auto h = state_[7];

    for (std::size_t i = 0; i != 64; ++i) {
      const auto S1 = rotr(e, 6u) ^ rotr(e, 11u) ^ rotr(e, 25u);
      const auto ch = (e & f) ^ ((~e) & g);
      const auto temp1 = h + S1 + ch + kRoundConstants[i] + w[i];
      const auto S0 = rotr(a, 2u) ^ rotr(a, 13u) ^ rotr(a, 22u);
      const auto maj = (a & b) ^ (a & c) ^ (b & c);
      const auto temp2 = S0 + maj;

      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  void finalizeInternal() {
    buffer_[bufferSize_] = 0x80u;
    ++bufferSize_;

    if (bufferSize_ > 56u) {
      std::fill(buffer_.begin() + bufferSize_, buffer_.end(), 0u);
      processBlock(buffer_.data());
      bufferSize_ = 0u;
    }

    std::fill(buffer_.begin() + bufferSize_, buffer_.begin() + 56u, 0u);

    for (int i = 0; i < 8; ++i) {
      buffer_[56u + i] = static_cast<std::uint8_t>((totalBits_ >> (56u - 8u * i)) & 0xFFu);
    }

    processBlock(buffer_.data());

    for (std::size_t i = 0; i != state_.size(); ++i) {
      digest_[i * 4u + 0u] = static_cast<std::uint8_t>((state_[i] >> 24u) & 0xFFu);
      digest_[i * 4u + 1u] = static_cast<std::uint8_t>((state_[i] >> 16u) & 0xFFu);
      digest_[i * 4u + 2u] = static_cast<std::uint8_t>((state_[i] >> 8u) & 0xFFu);
      digest_[i * 4u + 3u] = static_cast<std::uint8_t>(state_[i] & 0xFFu);
    }

    finalized_ = true;
  }

  std::array<std::uint32_t, 8> state_ = kInitialState;
  std::array<std::uint8_t, 64> buffer_{};
  std::array<std::uint8_t, 32> digest_{};
  std::uint64_t totalBits_ = 0u;
  std::size_t bufferSize_ = 0u;
  bool finalized_ = false;
};

} // namespace

std::string currentBackend() {
  return std::string(IGL_BACKEND_TYPE);
}

std::filesystem::path artifactsRoot() {
  if (const char* env = std::getenv("IGL_ARTIFACT_ROOT"); env && *env != '\0') {
    return std::filesystem::path(env);
  }
  return std::filesystem::current_path() / "artifacts";
}

std::filesystem::path ensureArtifactDirectory(const std::string& relativeGroup,
                                              const std::string& backend) {
  std::filesystem::path base = artifactsRoot() / std::filesystem::path(relativeGroup) / backend;
  std::filesystem::create_directories(base);
  return base;
}

ArtifactPaths makeArtifactPaths(const std::string& relativeGroup,
                                const std::string& backend,
                                const std::string& testName,
                                bool includeImage) {
  ArtifactPaths paths;
  auto base = ensureArtifactDirectory(relativeGroup, backend);

  paths.shaFile = base / (testName + ".sha256");

  if (includeImage) {
    auto imageDir = base / "640x360";
    std::filesystem::create_directories(imageDir);
    paths.pngFile = imageDir / (testName + ".png");
  }

  return paths;
}

void writeBinaryFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Failed to open file for writing: " + path.string());
  }
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  if (!out) {
    throw std::runtime_error("Failed to write all bytes to: " + path.string());
  }
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Failed to open file for writing: " + path.string());
  }
  out << text;
  if (!out) {
    throw std::runtime_error("Failed to write text to: " + path.string());
  }
}

std::string computeSha256(std::span<const std::uint8_t> bytes) {
  Sha256Context ctx;
  ctx.update(bytes.data(), bytes.size());
  const auto digest = ctx.finalize();

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (auto byte : digest) {
    oss << std::setw(2) << static_cast<unsigned>(byte);
  }
  return oss.str();
}

void writeSha256File(const std::filesystem::path& path, const std::string& hash) {
  writeTextFile(path, hash + "\n");
}

void writePng(const std::filesystem::path& path,
              const std::uint8_t* rgbaPixels,
              std::uint32_t width,
              std::uint32_t height) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }
  if (stbi_write_png(path.string().c_str(), static_cast<int>(width), static_cast<int>(height), 4,
                     rgbaPixels, static_cast<int>(width * 4u)) == 0) {
    throw std::runtime_error("Failed to write PNG: " + path.string());
  }
}

} // namespace igl::tests::util
