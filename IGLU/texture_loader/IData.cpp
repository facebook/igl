/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/IData.h>

namespace iglu::textureloader {
namespace {
class ByteData final : public IData {
 public:
  ByteData(std::unique_ptr<uint8_t[]> data, size_t length) noexcept;

  ~ByteData() final = default;

  [[nodiscard]] const uint8_t* IGL_NONNULL data() const noexcept final;
  [[nodiscard]] uint32_t length() const noexcept final;

  [[nodiscard]] ExtractedData extractData() noexcept final;

 private:
  std::unique_ptr<uint8_t[]> data_;
  uint32_t length_ = 0;
};

ByteData::ByteData(std::unique_ptr<uint8_t[]> data, size_t length) noexcept :
  data_(std::move(data)), length_(length) {}

const uint8_t* IGL_NONNULL ByteData::data() const noexcept {
  IGL_DEBUG_ASSERT(data_ != nullptr);
  return data_.get();
}

uint32_t ByteData::length() const noexcept {
  return length_;
}

IData::ExtractedData ByteData::extractData() noexcept {
  return {
      .data = data_.release(),
      .length = length_,
      .deleter = [](void* d) { delete[] reinterpret_cast<uint8_t*>(d); },
  };
}
} // namespace

std::unique_ptr<IData> IData::tryCreate(std::unique_ptr<uint8_t[]> data,
                                        uint32_t length,
                                        igl::Result* IGL_NULLABLE outResult) {
  if (data == nullptr) {
    igl::Result::setResult(outResult, igl::Result::Code::ArgumentNull, "data is nullptr");
    return nullptr;
  }

  if (length == 0u) {
    igl::Result::setResult(outResult, igl::Result::Code::ArgumentInvalid, "length is 0");
    return nullptr;
  }

  return std::make_unique<ByteData>(std::move(data), length);
}

} // namespace iglu::textureloader
