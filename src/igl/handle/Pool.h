/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <igl/handle/Handle.h>

#ifndef IGL_DEBUG_ASSERT
#define IGL_DEBUG_ASSERT(...)
#endif // IGL_DEBUG_ASSERT

namespace igl {

///--------------------------------------
/// MARK: - Pool

// A Pool of objects which is compatible with the abovementioned Handle<> types; based on:
// https://enginearchitecture.realtimerendering.com/downloads/reac2023_modern_mobile_rendering_at_hypehype.pdf
// https://github.com/corporateshark/lightweightvk/blob/main/lvk/Pool.h
template<typename ObjectType, typename ImplObjectType>
class Pool {
  static constexpr uint32_t kListEndSentinel = 0xffffffff;
  struct PoolEntry {
    explicit PoolEntry(ImplObjectType& obj) noexcept : obj_(std::move(obj)) {}
    ImplObjectType obj_ = {};
    uint32_t gen_ = 1;
    uint32_t nextFree_ = kListEndSentinel;
  };
  uint32_t freeListHead_ = kListEndSentinel;
  uint32_t numObjects_ = 0;

 public:
  std::vector<PoolEntry> objects_;

  [[nodiscard]] Handle<ObjectType> create(ImplObjectType&& obj) {
    uint32_t idx = 0;
    if (freeListHead_ != kListEndSentinel) {
      idx = freeListHead_;
      freeListHead_ = objects_[idx].nextFree_;
      objects_[idx].obj_ = std::move(obj);
    } else {
      idx = (uint32_t)objects_.size();
      objects_.emplace_back(obj);
    }
    numObjects_++;
    return Handle<ObjectType>(idx, objects_[idx].gen_);
  }
  void destroy(Handle<ObjectType> handle) noexcept {
    if (handle.empty()) {
      return;
    }
    IGL_DEBUG_ASSERT(numObjects_ > 0, "Double deletion");
    const uint32_t index = handle.index();
    IGL_DEBUG_ASSERT(index < objects_.size());
    IGL_DEBUG_ASSERT(handle.gen() == objects_[index].gen_, "Double deletion");
    objects_[index].obj_ = ImplObjectType{};
    objects_[index].gen_++;
    objects_[index].nextFree_ = freeListHead_;
    freeListHead_ = index;
    numObjects_--;
  }
  // this is a helper function to simplify migration to handles (should be deprecated after the
  // migration is completed)
  void destroy(uint32_t index) noexcept {
    IGL_DEBUG_ASSERT(numObjects_ > 0, "Double deletion");
    IGL_DEBUG_ASSERT(index < objects_.size());
    objects_[index].obj_ = ImplObjectType{};
    objects_[index].gen_++;
    objects_[index].nextFree_ = freeListHead_;
    freeListHead_ = index;
    numObjects_--;
  }
  [[nodiscard]] const ImplObjectType* IGL_NULLABLE get(Handle<ObjectType> handle) const noexcept {
    if (handle.empty()) {
      return nullptr;
    }

    const uint32_t index = handle.index();
    IGL_DEBUG_ASSERT(index < objects_.size());
    IGL_DEBUG_ASSERT(handle.gen() == objects_[index].gen_, "Accessing a deleted object");
    return &objects_[index].obj_;
  }
  [[nodiscard]] ImplObjectType* IGL_NULLABLE get(Handle<ObjectType> handle) noexcept {
    if (handle.empty()) {
      return nullptr;
    }

    const uint32_t index = handle.index();
    IGL_DEBUG_ASSERT(index < objects_.size());
    IGL_DEBUG_ASSERT(handle.gen() == objects_[index].gen_, "Accessing a deleted object");
    return &objects_[index].obj_;
  }
  [[nodiscard]] Handle<ObjectType> findObject(const ImplObjectType* IGL_NULLABLE obj) noexcept {
    if (!obj) {
      return {};
    }

    for (size_t idx = 0; idx != objects_.size(); idx++) {
      if (objects_[idx].obj_ == *obj) {
        return Handle<ObjectType>((uint32_t)idx, objects_[idx].gen_);
      }
    }

    return {};
  }
  void clear() noexcept {
    objects_.clear();
    freeListHead_ = kListEndSentinel;
    numObjects_ = 0;
  }
  [[nodiscard]] uint32_t numObjects() const noexcept {
    return numObjects_;
  }
};

} // namespace igl
