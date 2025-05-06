/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/DataReader.h>

namespace iglu::textureloader {

std::optional<DataReader> DataReader::tryCreate(const uint8_t* FOLLY_NONNULL data,
                                                uint32_t length,
                                                igl::Result* IGL_NULLABLE outResult) {
  if (data == nullptr) {
    igl::Result::setResult(outResult, igl::Result::Code::ArgumentInvalid, "data is nullptr.");
    return {};
  }

  return DataReader(data, length);
}

DataReader::DataReader(const uint8_t* FOLLY_NONNULL data, uint32_t length) noexcept :
  data_(data), length_(length) {}

const uint8_t* DataReader::data() const noexcept {
  return data_;
}

uint32_t DataReader::length() const noexcept {
  return length_;
}

const uint8_t* IGL_NULLABLE DataReader::tryAt(uint32_t offset,
                                              igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (!ensureLength(0, offset, outResult)) {
    return nullptr;
  }
  return at(offset);
}

const uint8_t* IGL_NONNULL DataReader::at(uint32_t offset) const noexcept {
  IGL_DEBUG_ASSERT(length_ >= offset);
  return data_ + offset;
}

bool DataReader::tryAdvance(uint32_t bytesToAdvance, igl::Result* IGL_NULLABLE outResult) noexcept {
  if (!ensureLength(bytesToAdvance, 0, outResult)) {
    return false;
  }
  advance(bytesToAdvance);
  return true;
}

bool DataReader::ensureLength(uint32_t requestedLength,
                              uint32_t offset,
                              igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (offset > length_ || requestedLength > length_ - offset) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "data length is too small.");
    return false;
  }

  return true;
}

void DataReader::advance(uint32_t bytesToAdvance) noexcept {
  IGL_DEBUG_ASSERT(length_ >= bytesToAdvance);

  data_ += bytesToAdvance;
  length_ -= bytesToAdvance;
}

} // namespace iglu::textureloader
